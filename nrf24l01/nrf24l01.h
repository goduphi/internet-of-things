/*
 * nrf24l01.h
 *
 *  Created on: Mar 25, 2021
 *      Author: Sarker Nadir Afridi Azmi
 *
 * Target Board: TM4C123GH6PM @40MHz
 *
 * Pinout (SPI1)
 * ------
 * CE       (PD6)
 * CSN      (PD1)
 * SCK      (PD1)
 * MOSI     (PD3)
 * MISO     (PD2)
 */

#include <stdint.h>
#include <stdbool.h>

#ifndef NRF24L01_H_
#define NRF24L01_H_

// Commands
#define R_REGISTER          0x00
#define W_REGISTER          0x20
#define R_RX_PAYLOAD        0x61
#define W_TX_PAYLOAD        0xA0
#define FLUSH_TX            0xE1
#define FLUSH_RX            0xE2
#define ACTIVATE            0x50
#define R_RX_PL_WID         0x60
#define W_TX_PAYLOAD_NO_ACK 0xB0
#define NOP                 0xFF

// Registers
#define RX_ADDR_P0          0x0A
#define RX_ADDR_P1          0x0B
#define RX_ADDR_P2          0x0C
#define RX_ADDR_P3          0x0D
#define RX_ADDR_P4          0x0E
#define RX_ADDR_P5          0x0F
#define TX_ADDR             0x10

typedef enum _mode
{
    RX, TX
} mode;

void initNrf24l01();
void rfReadIntoBuffer(uint8_t reg, uint8_t buffer[], uint8_t nBytes);
void rfSetAddress(uint8_t pipe, uint32_t address);
void rfSetMode(mode m, uint8_t frequency);
bool rfIsDataAvailable();
void rfSendBuffer(uint8_t buffer[], uint32_t nBytes);
uint32_t rfReceiveBuffer(uint8_t buffer[]);

#endif /* NRF24L01_H_ */
