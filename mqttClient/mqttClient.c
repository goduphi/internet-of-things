// Jason Losh

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

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4


//-----------------------------------------------------------------------------
// Configuring Wireshark to examine packets
//-----------------------------------------------------------------------------

// sudo ethtool --offload eno2 tx off rx off
// in wireshark, preferences->protocol->ipv4->validate the checksum if possible
// in wireshark, preferences->protocol->udp->validate the checksum if possible

//-----------------------------------------------------------------------------
// Sending UDP test packets
//-----------------------------------------------------------------------------

// test this with a udp send utility like sendip
//   if sender IP (-is) is 192.168.1.1, this will attempt to
//   send the udp datagram (-d) to 192.168.1.199, port 1024 (-ud)
// sudo sendip -p ipv4 -is 192.168.1.1 -p udp -ud 1024 -d "on" 192.168.1.199
// sudo sendip -p ipv4 -is 192.168.1.1 -p udp -ud 1024 -d "off" 192.168.1.199

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

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3

// Global variables
extern bool isCarriageReturn;
uint32_t seqNum = 200;
uint32_t ackNum = 0;

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

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

typedef enum _state
{
    IDLE,
    SEND_ARP,
    RECV_ARP,
    SEND_SYN,
    RECV_SYN_ACK
} state;

void showHelp()
{
}

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
    uint8_t ipv4Buffer[4];
    uint8_t serverMacLocalCopy[6];

    // Get the IP address from the EEPROM
    uint32_t mqttIpv4Address = readEeprom(PROJECT_META_DATA + 1);
    if(mqttIpv4Address != 0xFFFFFFFF)
        convertEncodedIpv4ToArray(ipv4Buffer, mqttIpv4Address);

    // These are here for testing purposes
    socket source;
    socket dest;

    // Fill up the socket information
    etherGetIpAddress(source.ip);
    source.port = 0;
    etherGetMacAddress(source.mac);
    copyUint8Array(ipv4Buffer, dest.ip, 4);
    dest.port = 1883;

    state currentState = IDLE;
    bool connect = false;

    /*
     * Followed RFC793
     * Only send during the initial SYN message
     * Kind = 2 for Maximum Segment Size
     * Length = 4
     * data = 1460 (0x05B4) MSB first
     * At the end include 0 to signify end of option
     */
    uint8_t optionsLength = 4;
    uint8_t options[] = {0x02, 0x04, 0x05, 0xB4, 0x00};

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
                            uint32_t ipv4Address = getIpv4Address(&userData, 1);
                            writeEeprom(PROJECT_META_DATA + 1, ipv4Address);
                        }
                        else
                            putsUart0("<www.xxx.yyy.zzz>\n");
                    }
                }

                if(isCommand(&userData, "status", 0))
                {
                    putsUart0("MQTT IP: ");
                    printIpv4(ipv4Buffer);
                    putcUart0('\n');
                    putsUart0("MQTT Broker MAC: ");
                    printMac(serverMacLocalCopy);
                    putcUart0('\n');
                }

                if(isCommand(&userData, "connect", 0))
                {
                    connect = true;
                    currentState = SEND_ARP;
                }
            }
        }

        if(connect)
        {
            // This switch controls the send part of the state machine
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

                // This switch controls the receive part of the state machine
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
                case RECV_SYN_ACK:
                    // Handle IP datagram
                    if(etherIsIp(etherData) && etherIsTcp(etherData))
                    {
                        ipHeader* rip = (ipHeader*)etherData->data;
                        tcpHeader* rtcp = (tcpHeader*)rip->data;
                        seqNum++;
                        ackNum = ntohl(rtcp->sequenceNumber) + 1;
                        sendTcp(etherData, &source, &dest, 0x5000 | ACK, seqNum, ackNum, 0, 0, 0);
                    }
                    break;
                }
            }
        }
    }
}
