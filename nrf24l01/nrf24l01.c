/*
 * nrf24l01.c
 *
 *  Created on: Apr 6, 2021
 *      Author: Sarker Nadir Afridi Azmi
 *      Resource used: https://github.com/rohitdureja/Nordic-nRF24L01-RF-Driver
 *                     The rfReceiveBuffer() code was taken from the above mentioned
 *                     link.
 * Notes
 * -----
 * Page: 27, 7.4.1
 * This is only applicable if dynamic payload length is not enabled.
 * The length of the data received is set by RX_PW_P0. This needs
 * to be the same the amount of data being sent.
 * TX_FIFO length = RX_PW_P0 length for static payload length.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"
#include "spi1.h"
#include "nrf24l01.h"

#define CSN                 PORTD,1
#define CHIP_ENABLE         PORTD,6

// Register                 Addr    Bit Fields
#define CONFIG              0x00
#define EN_CRC                      0x08    // Enable CRC
#define PWR_UP                      0x02    // Powers up device
#define PRIM_RX                     0x01    // Sets the device in Receive mode
#define EN_AA               0x01
#define ENAA_P0                     0x01
#define EN_RXADDR           0x02
#define ERX_P0                      0x01
#define SETUP_AW            0x03            // Sets the width of the address
#define SETUP_RETR          0x04
#define RF_CH               0x05
#define RF_SETUP            0x06
#define RF_DR                       0x08
#define RF_PWR_0DBM                 0x06
#define LNA_HCURR                   0x01
#define STATUS              0x07
#define RX_P_NO                     0x07
#define OBSERVE_TX          0x08
#define CD                  0x09
#define RX_PW_P0            0x11
#define RX_PW_P1            0x12
#define RX_PW_P2            0x13
#define RX_PW_P3            0x14
#define RX_PW_P4            0x15
#define RX_PW_P5            0x16
#define FIFO_STATUS         0x17
#define TX_FULL                     0x20
#define TX_EMPTY                    0x10
#define RX_FULL                     0x02
#define RX_EMPTY                    0x01
#define DYNPD               0x1C
#define DPL_P0                      0x01
#define FEATURE             0x1D
#define EN_DPL                      0x04

#define MAX_ADDRESS_LENGTH  4

void chipEnable()
{
    setPinValue(CHIP_ENABLE, 1);
}

void chipDisable()
{
    setPinValue(CHIP_ENABLE, 0);
}

void rfCsOff()
{
    setPinValue(CSN, 0);
    _delay_cycles(4);                    // allow line to settle
}

void rfCsOn()
{
    setPinValue(CSN, 1);
}

void rfWriteRegister(uint8_t reg, uint8_t data)
{
    rfCsOff();
    // Select register
    writeSpi1Data(W_REGISTER | reg);
    // Read dummy data to clear buffer
    readSpi1Data();
    // Write dummy data
    writeSpi1Data(data);
    // Read dummy data to clear buffer
    readSpi1Data();
    rfCsOn();
}

uint8_t rfReadRegister(uint8_t reg)
{
    uint8_t data = 0;
    rfCsOff();
    // Select register
    writeSpi1Data(R_REGISTER | reg);
    // Read dummy data to clear buffer
    readSpi1Data();
    // Write dummy data
    writeSpi1Data(NOP);
    // Read register data
    data = readSpi1Data();
    rfCsOn();
    return data;
}

/*
 * Reads n bytes into a buffer
 */
void rfReadIntoBuffer(uint8_t reg, uint8_t buffer[], uint8_t nBytes)
{
    rfCsOff();
    // Select register
    writeSpi1Data(R_REGISTER | reg);
    // Read dummy data to clear buffer
    readSpi1Data();
    uint8_t i = 0;
    for(i = 0; i < nBytes; i++)
    {
        // Write dummy data
        writeSpi1Data(NOP);
        // Read register data
        buffer[i] = readSpi1Data();
    }
    rfCsOn();
}

/*
 * This function sets the address of the rf module.
 * The max address length allowed is 4 bytes.
 * Refer to page 35 of the datasheet to know more about data pipes
 */
void rfSetAddress(uint8_t pipe, uint32_t address)
{
    // The address width is always 4 bytes - 0x02
    rfWriteRegister(SETUP_AW, 0x02);
    rfCsOff();
    // Select the address (pipe) register
    writeSpi1Data(W_REGISTER | pipe);
    // Read dummy data to clear buffer
    readSpi1Data();
    // Write the address to the register
    uint8_t i = 0;
    for(i = 0; i < MAX_ADDRESS_LENGTH; i++)
    {
        // Write lower LSB first
        writeSpi1Data(((address >> (i << 3)) & 0xFF));
        readSpi1Data();
    }
    rfCsOn();
}

void rfSetMode(mode m, uint8_t frequency)
{
    // This is an error as the frequency cannot be more than 2.525GHz
    if(frequency > 125)
        return;

    chipDisable();
    // Frequency = 2400 + RF_CH [MHz]
    rfWriteRegister(RF_CH, frequency);
    // 1. Use dynamic payload length
    rfWriteRegister(FEATURE, EN_DPL);
    rfWriteRegister(ACTIVATE, 0x73);
    switch(m)
    {
    case RX:
        // Appendix B
        // Enable the receive pipe
        rfWriteRegister(EN_RXADDR, ERX_P0);
        // Set a data rate of 2Mbps
        // Set Low Noise Amplifier gain to reduce current consumption
        // Set output power of power amplifier to 0dBm
        rfWriteRegister(RF_SETUP, RF_DR | RF_PWR_0DBM | LNA_HCURR);
        // 2. Use dynamic payload length
        // Auto acknowledgement is set for all pipes by default
        // Enable DPL for all available pipes 0 - 5
        rfWriteRegister(DYNPD, 0x1F);
        // Power up the device and put it in primary receive mode
        // Not using any interrupts. Disable all interrupts with 0x70
        rfWriteRegister(CONFIG, 0x70 | PWR_UP | PRIM_RX | EN_CRC);
        rfCsOff();
        chipEnable();
        break;
    case TX:
        // Transmit only requires DPL for pipe 0 to be enabled
        // 2. Use dynamic payload length
        // Enable auto acknowledgement for pipe 0
        rfWriteRegister(EN_AA, ENAA_P0);
        // Enable DPL for pipe 0
        rfWriteRegister(DYNPD, DPL_P0);
        // Set reset count 0 to disable auto retransmit
        // Re-transmit 3 times with a 250us delay
        // Set a data rate of 2Mbps
        rfWriteRegister(RF_SETUP, RF_DR | RF_PWR_0DBM | LNA_HCURR);
        // Power up the device
        // Not using any interrupts. Disable all interrupts with 0x70
        rfWriteRegister(CONFIG, 0x70 | PWR_UP | EN_CRC);
        break;
    }
}

void initNrf24l01()
{
    initSpi1(USE_SSI1_RX);
    selectPinPushPullOutput(CSN);
    selectPinPushPullOutput(CHIP_ENABLE);
    // Run at 8Mbps
    setSpi1BaudRate(5e6, 40e6);
    setSpi1Mode(0, 0);
}

/*
 * Return true if any of the data pipes have received data.
 */
bool rfIsDataAvailable()
{
    return (((rfReadRegister(STATUS) >> 1) & RX_P_NO) != RX_P_NO);
}

uint32_t rfReceiveBuffer(uint8_t buffer[])
{
    uint32_t receivedBytes = 0;

    rfCsOff();
    // Read in 2 bytes of data for the length of payload
    writeSpi1Data(R_RX_PL_WID);
    readSpi1Data();
    writeSpi1Data(NOP);
    receivedBytes = readSpi1Data();
    rfCsOn();

    rfCsOff();
    writeSpi1Data(R_RX_PAYLOAD);
    readSpi1Data();
    uint8_t i = 0;
    for(i = 0; i < receivedBytes; i++)
    {
        writeSpi1Data(NOP);
        buffer[i] = readSpi1Data();
    }

    // Clear the receive buffer
    writeSpi1Data(FLUSH_RX);
    readSpi1Data();
    rfCsOn();

    return receivedBytes;
}

void rfSendBuffer(uint8_t buffer[], uint32_t nBytes)
{
    // Clear the transmit buffer
    rfCsOff();
    writeSpi1Data(FLUSH_TX);
    readSpi1Data();
    rfCsOn();

    // Clear the MAX_RT interrupt flag
    // This allows further transmission
    rfWriteRegister(STATUS, 0x10);

    chipDisable();
    rfCsOff();
    writeSpi1Data(W_TX_PAYLOAD);
    readSpi1Data();
    uint8_t i = 0;
    for(i = 0; i < nBytes; i++)
    {
        writeSpi1Data(buffer[i]);
        readSpi1Data();
    }
    rfCsOn();

    chipEnable();
    // Pulse CE for more than 10us
    _delay_cycles(20000);
    chipDisable();
}
