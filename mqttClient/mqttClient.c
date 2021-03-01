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

    // Endless loop
    while(true)
    {
        if (kbhitUart0())
        {
            getsUart0(&userData);
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
                uint32_t mqttIpv4Address = readEeprom(PROJECT_META_DATA + 1);
                convertEncodedIpv4ToArray(ipv4Buffer, mqttIpv4Address);
                putsUart0("MQTT IP: ");
                printIpv4(ipv4Buffer);
                putcUart0('\n');
                putsUart0("MQTT Broker MAC: ");
                printMac(serverMacLocalCopy);
                putcUart0('\n');
            }

            if(isCommand(&userData, "arp", 0))
            {
            }

            if(isCommand(&userData, "connect", 0))
            {
                // Send an ARP request to find out what the MAC address of the server is
                etherSendArpRequest(etherData, ipv4Buffer);
            }

            if(isCommand(&userData, "tcp", 0))
            {
                socket source;
                etherGetIpAddress(source.ip);
                source.port = 0;
                etherGetMacAddress(source.mac);
                socket dest;
                copyUint8Array(ipv4Buffer, dest.ip, 4);
                dest.port = 1883;
                copyUint8Array(serverMacLocalCopy, dest.mac, 6);
                sendTcp(etherData, &source, &dest, 0x5000 | 0x0002);
            }
        }

        if(etherIsDataAvailable())
        {
            // This is just a test for now
            // Get packet
            etherGetPacket(etherData, MAX_PACKET_SIZE);

            // Add an error message here if we do not receive an arp response
            // Store the MAC address locally
            copyUint8Array(etherData->sourceAddress, serverMacLocalCopy, 6);
        }
    }
}
