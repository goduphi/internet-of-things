// Sarker Nadir Afridi Azmi

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6


//-----------------------------------------------------------------------------
// Configuring Wireshark to examine packets
//-----------------------------------------------------------------------------

// sudo ethtool --offload eno2 tx off rx off
// in wireshark, preferences->protocol->ipv4->validate the checksum if possible
// in wireshark, preferences->protocol->udp->validate the checksum if possible

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "clock.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "eth0.h"
#include "tm4c123gh6pm.h"
#include "eeprom.h"
#include "cli.h"
#include "utils.h"
#include "tcp.h"
#include "mqtt.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE         1522

typedef enum _state
{
    IDLE,
    SEND_ARP,
    RECV_ARP,
    SEND_SYN,
    RECV_SYN_ACK,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSED,
    // MQTT states
    CONNECT_MQTT,
    CONNACK_MQTT,
    PUBLISH_MQTT,
    PUBLISH_QOS0_MQTT,
    PUBLISH_QOS1_MQTT,
    SUBSCRIBE_MQTT,
    SUBACK_MQTT,
    UNSUBSCRIBE_MQTT,
    UNSUBACK_MQTT,
    PINGREQ_MQTT,
    PINGRESP_MQTT,
    DISCONNECT_MQTT
} state;

// Global variables
extern bool isCarriageReturn;

// Stores information about the connection
uint32_t seqNum = 200;
uint32_t ackNum = 0;
bool connect = false;
// Stores the size of the data payload sent
uint16_t size = 0;

uint8_t ipv4Buffer[4];
uint8_t serverMacLocalCopy[6];
uint8_t qos = QOS1;
uint16_t packetIdentifier = 69;

//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
}

void showHelp()
{
    putsUart0("\nMQTT Client Help Desk!\n");
    putsUart0("\thelp\t\tshows help menu\n");
    putsUart0("\tstatus\t\tshows the current IP and MAC of the server\n");
    putsUart0("\tconnect\t\tconnects to Mosquitto server\n");
    putsUart0("\tsubscribe <TOPIC>\t\tsubscribe to a topic\n");
    putcUart0('\n');
}

void displayInfo()
{
    putsUart0("MQTT Server IP: ");
    printIpv4(ipv4Buffer);
    putcUart0('\n');
    putsUart0("MQTT Broker MAC: ");
    printMac(serverMacLocalCopy);
    putcUart0('\n');
}

void resetConnection()
{
    ackNum = 0;
    seqNum = 200;
    size = 0;
    connect = false;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    // Init controller
    initHw();
    initEeprom();
    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // This code was directly taken from the ethernet example done in class
    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
    etherSetMacAddress(2, 3, 4, 5, 6, 101);
    etherDisableDhcpMode();
    etherSetIpAddress(192, 168, 2, 101);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 1, 1);
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    waitMicrosecond(100000);

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader* etherData = (etherHeader*)buffer;

    USER_DATA userData;

    // Get the IP address from the EEPROM
    // The default EEPROM value is 0xFFFFFFFF
    uint32_t mqttIpv4Address = readEeprom(PROJECT_META_DATA + 1);
    if(mqttIpv4Address != 0xFFFFFFFF)
        convertEncodedIpv4ToArray(ipv4Buffer, mqttIpv4Address);
    else
        putsUart0("The IP needs to be set before connecting!\n");

    // These are here for testing purposes
    socket source;
    socket dest;

    // Fill up the socket information
    etherGetIpAddress(source.ip);
    source.port = 0;
    etherGetMacAddress(source.mac);
    copyUint8Array(ipv4Buffer, dest.ip, 4);
    dest.port = 1883;

    /*
     * Followed RFC793
     * Only send during the initial SYN message
     * Kind = 2 for Maximum Segment Size
     * Length = 4
     * data = 1460 (0x05B4) - MSB first
     * At the end include 0 to signify end of option
     */
    uint8_t optionsLength = 4;
    uint8_t options[] = {0x02, 0x04, 0x05, 0xB4, 0x00};

    // Variables for the state machine
    ipHeader* recevedIpHeader = (ipHeader*)etherData->data;
    tcpHeader* receivedTcpHeader = (tcpHeader*)recevedIpHeader->data;

    state currentState = IDLE;

    // Endless loop
    while(true)
    {
        if (kbhitUart0())
        {
            getsUart0(&userData);

            if(isCarriageReturn)
            {
                isCarriageReturn = false;

                parseField(&userData);

                if(isCommand(&userData, "set", 2))
                {
                    if(stringCompare("MQTT", getFieldString(&userData, 1)))
                    {
                        if(isIpv4Address(&userData, 1))
                        {
                            // Save the MQTT IPv4 address
                            mqttIpv4Address = getIpv4Address(&userData, 1);
                            writeEeprom(PROJECT_META_DATA + 1, mqttIpv4Address);
                        }
                        else
                            putsUart0("Format: 255.255.255.255\n");
                    }
                }

                if(isCommand(&userData, "status", 0))
                    displayInfo();

                if(isCommand(&userData, "help", 0))
                    showHelp();

                if(isCommand(&userData, "connect", 0))
                    connect = true;

                if(isCommand(&userData, "publish", 2))
                    currentState = PUBLISH_MQTT;

                if(isCommand(&userData, "subscribe", 1))
                    currentState = SUBSCRIBE_MQTT;

                if(isCommand(&userData, "unsubscribe", 1))
                    currentState = UNSUBSCRIBE_MQTT;

                if(isCommand(&userData, "ping", 0))
                    currentState = PINGREQ_MQTT;

                if(isCommand(&userData, "disconnect", 0))
                    currentState = DISCONNECT_MQTT;
            }
        }

        if(connect)
        {
            connect = false;
            // Initiate the start of the conversation
            currentState = SEND_ARP;
        }

        // This switch controls the send part of the state machine
        // The requests/sends are always sent with the last known sequence and acknowledgement numbers
        switch(currentState)
        {
        case SEND_ARP:
            // Send an ARP request to find out what the MAC address of the server is
            etherSendArpRequest(etherData, ipv4Buffer);
            currentState = RECV_ARP;
            break;
        case SEND_SYN:
            sendTcp(etherData, &source, &dest, 0x6000 | SYN, seqNum, ackNum, options, optionsLength, 0);
            currentState = RECV_SYN_ACK;
            break;
        case CONNECT_MQTT:
            assembleMqttConnectPacket(receivedTcpHeader->data, CLEAN_SESSION, "test", 4, &size);
            sendTcp(etherData, &source, &dest, 0x5000 | PSH | ACK, seqNum, ackNum, 0, 0, size);
            currentState = CONNACK_MQTT;
            break;
        case PINGREQ_MQTT:
            assembleMqttPacket(receivedTcpHeader->data, PINGERQ, &size);
            sendTcp(etherData, &source, &dest, 0x5000 | PSH | ACK, seqNum, ackNum, 0, 0, size);
            currentState = PINGRESP_MQTT;
            break;
        case DISCONNECT_MQTT:
            assembleMqttPacket(receivedTcpHeader->data, DISCONNECT, &size);
            sendTcp(etherData, &source, &dest, 0x5000 | PSH | FIN | ACK, seqNum, ackNum, 0, 0, size);
            currentState = FIN_WAIT_1;
            break;
        case PUBLISH_MQTT:
            assembleMqttPublishPacket(receivedTcpHeader->data, getFieldString(&userData, 1), packetIdentifier, qos, getFieldString(&userData, 2), &size);
            sendTcp(etherData, &source, &dest, 0x5000 | PSH | ACK, seqNum, ackNum, 0, 0, size);
            switch(qos)
            {
            case QOS0:
                currentState = PUBLISH_QOS0_MQTT;
                break;
            case QOS1:
                currentState = PUBLISH_QOS1_MQTT;
                break;
            case QOS2:
                break;
            }
            break;
        case SUBSCRIBE_MQTT:
            assembleMqttSubscribePacket(receivedTcpHeader->data, packetIdentifier, getFieldString(&userData, 1), QOS0, &size);
            sendTcp(etherData, &source, &dest, 0x5000 | PSH | ACK, seqNum, ackNum, 0, 0, size);
            currentState = SUBACK_MQTT;
            break;
        case UNSUBSCRIBE_MQTT:
            assembleMqttUnsubscribePacket(receivedTcpHeader->data, packetIdentifier, getFieldString(&userData, 1), &size);
            sendTcp(etherData, &source, &dest, 0x5000 | PSH | ACK, seqNum, ackNum, 0, 0, size);
            currentState = UNSUBACK_MQTT;
            break;
        case CLOSED:
            putsUart0("Connection closed!\n");
            resetConnection();
            currentState = IDLE;
            break;
        }

        if(etherIsDataAvailable())
        {
            if (etherIsOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            etherGetPacket(etherData, MAX_PACKET_SIZE);

            if(etherIsArpRequest(etherData))
                etherSendArpResponse(etherData);

            // This part of the program controls the receive part of the state machine
            switch(currentState)
            {
            case RECV_ARP:
                if(etherIsArpResponse(etherData))
                {
                    putsUart0("Received an ARP response\n");
                    copyUint8Array(etherData->sourceAddress, serverMacLocalCopy, 6);
                    currentState = SEND_SYN;
                }
                break;
            }

            if(etherIsIp(etherData) && etherIsTcp(etherData))
            {

                // Get publish packets
                if(mqttIsPublishPacket(receivedTcpHeader->data))
                {
                    putsUart0("\nReceived new subscription information\n");

                    subscription receivedSubscriptionData;
                    getTopicData(receivedTcpHeader->data, &receivedSubscriptionData);

                    putsUart0("Topic name: ");
                    putsUart0(receivedSubscriptionData.topicName);
                    putcUart0('\n');
                    putsUart0("Message: ");
                    putsUart0(receivedSubscriptionData.message);
                    putcUart0('\n');

                    ackNum = ntohl(receivedTcpHeader->sequenceNumber) + receivedSubscriptionData.remainingLength + 2;
                    sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    currentState = IDLE;
                }

                switch(currentState)
                {
                case RECV_SYN_ACK:
                    // Check if SYN, ACK
                    if((ntohs(receivedTcpHeader->offsetFields) & SYN) && (ntohs(receivedTcpHeader->offsetFields) & ACK))
                    {
                        seqNum++;
                        ackNum = ntohl(receivedTcpHeader->sequenceNumber) + 1;
                        sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                        currentState = CONNECT_MQTT;
                    }
                    else
                    {
                        putsUart0("State: RECV_SYN_ACK error\n");
                        currentState = RECV_SYN_ACK;
                    }
                    break;
                case FIN_WAIT_1:
                    // Check if this is the ACK of FIN
                    if((ntohs(receivedTcpHeader->offsetFields) & ACK) || (htonl(receivedTcpHeader->sequenceNumber) == ackNum))
                    {
                        currentState = FIN_WAIT_2;
                    }
                    else
                    {
                        putsUart0("State: FIN_WAIT_1 error\n");
                        currentState = FIN_WAIT_1;
                    }
                    break;
                case FIN_WAIT_2:
                    // Check if FIN, ACK
                    if((ntohs(receivedTcpHeader->offsetFields) & FIN) && (ntohs(receivedTcpHeader->offsetFields) & ACK))
                    {
                        /* The plus 1 is from page 43 of the RFC793 showing that the sequence number is incremented by 1
                         * even if no data is sent
                         */
                        seqNum += size + 1;
                        ackNum = ntohl(receivedTcpHeader->sequenceNumber) + 1;
                        sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                        waitMicrosecond(TIMEOUT_2MS);
                        currentState = CLOSED;
                    }
                    else
                    {
                        putsUart0("State: FIN_WAIT_2 error\n");
                    }
                    break;
                case CONNACK_MQTT:
                    if(!mqttIsConnack(receivedTcpHeader->data) || (htonl(receivedTcpHeader->sequenceNumber) != ackNum))
                    {
                        putsUart0("State: MQTT_CONNACK error\n");
                        currentState = CONNACK_MQTT;
                        continue;
                    }
                    // Take the size of the previous data sent in bytes and add it to the sequence number
                    seqNum += size;
                    // Here, 4 is the size of the connack packet
                    ackNum = ntohl(receivedTcpHeader->sequenceNumber) + getPayloadSize(etherData);
                    sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    currentState = IDLE;
                    break;
                case PUBLISH_QOS0_MQTT:
                    // Take the size of the previous data sent in bytes and add it to the sequence number
                    seqNum += size;
                    break;
                case PUBLISH_QOS1_MQTT:
                    if(!mqttIsPuback(receivedTcpHeader->data, packetIdentifier) || (htonl(receivedTcpHeader->sequenceNumber) != ackNum))
                    {
                        putsUart0("State: PUBLISH_QOS1_MQTT error\n");
                        currentState = PUBLISH_QOS1_MQTT;
                        continue;
                    }
                    seqNum += size;
                    // Here, 4 is the size of the puback packet
                    ackNum = ntohl(receivedTcpHeader->sequenceNumber) + getPayloadSize(etherData);
                    sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    currentState = IDLE;
                    break;
                case SUBACK_MQTT:
                    if(!mqttIsAck(receivedTcpHeader->data, SUBACK, packetIdentifier, 1)  || (htonl(receivedTcpHeader->sequenceNumber) != ackNum))
                    {
                        putsUart0("State: SUBACK_MQTT error\n");
                        currentState = SUBACK_MQTT;
                        continue;
                    }
                    uint8_t returnCode = getSubackPayload(receivedTcpHeader->data);
                    if(returnCode == SUBACK_FAILURE)
                        putsUart0("Error: SUBACK_FAILURE");
                    else
                    {
                        putsUart0("Maximum QoS granted: ");
                        printUint8InDecimal(returnCode);
                        putcUart0('\n');
                    }
                    seqNum += size;
                    // Here, 5 is the size of the puback packet
                    ackNum = ntohl(receivedTcpHeader->sequenceNumber) + getPayloadSize(etherData);
                    sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    currentState = IDLE;
                    break;
                case UNSUBACK_MQTT:
                    // The number of topics should be zero as only a packet identifier is sent
                    if(!mqttIsAck(receivedTcpHeader->data, UNSUBACK, packetIdentifier, 0) || (htonl(receivedTcpHeader->sequenceNumber) != ackNum))
                    {
                        putsUart0("State: UNSUBACK_MQTT error\n");
                        currentState = UNSUBACK_MQTT;
                        continue;
                    }
                    seqNum += size;
                    // Here, 4 is the size of the suback packet
                    ackNum = ntohl(receivedTcpHeader->sequenceNumber) + getPayloadSize(etherData);
                    sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    currentState = IDLE;
                    break;
                case PINGRESP_MQTT:
                    if(!mqttIsPingResponse(receivedTcpHeader->data) || (htonl(receivedTcpHeader->sequenceNumber) != ackNum))
                    {
                        putsUart0("State: PINGRESP_MQTT -> No ping response\n");
                        currentState = PINGRESP_MQTT;
                        continue;
                    }
                    // Take the size of the previous data sent in bytes and add it to the sequence number
                    seqNum += size;
                    // Here, 4 is the size of the connack packet
                    ackNum = ntohl(receivedTcpHeader->sequenceNumber) + getPayloadSize(etherData);
                    sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    currentState = IDLE;
                    break;
                }
            }
        }
    }
}
