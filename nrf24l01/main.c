/**
 * Author: Sarker Nadir Afridi Azmi
 *
 * Description: A simple protocol less application which can
 * send information wirelessly using the RF24L01 module.
 *
 * Target Board: Tiva C Series (TM4C123GH6PM)
 * Stack Size: 4096
 */

#include <stdint.h>
#include <stdbool.h>
#include "clock.h"
#include "gpio.h"
#include "spi1.h"
#include "uart0.h"
#include "common_terminal_interface.h"
#include "nrf24l01.h"
#include "wait.h"
#include "timer0.h"
#include <stdio.h>

#define ADDRESS0    0xACCE55

//#define REC

#define SEND_LED    PORTF,2
#define RECV_LED    PORTF,1

void initHw()
{
    enablePort(PORTF);
    selectPinPushPullOutput(SEND_LED);
    selectPinPushPullOutput(RECV_LED);
}

uint8_t l = 0;

void timer0Isr()
{
#ifndef REC
    setPinValue(SEND_LED, getPinValue(SEND_LED) ^ 1);
    char msg[] = "Id:0";
    msg[3] = (l++%10) + '0';
    rfSendBuffer((uint8_t*)&msg, strLen(msg));
#endif
    TIMER0_ICR_R = TIMER_ICR_TATOCINT;
}

int main(void)
{
    initSystemClockTo40Mhz();
    initNrf24l01();

    // Only use one data pipe to transmit data
    rfSetAddress(RX_ADDR_P0, ADDRESS0);
    rfSetAddress(TX_ADDR, ADDRESS0);

#ifdef REC
    rfSetMode(RX, 45);
#else
    rfSetMode(TX, 45);
#endif

    initUart0();
    setUart0BaudRate(115200, 40e6);

#ifdef REC
    putsUart0("This board is in RX mode\n");
#else
    putsUart0("This board is in TX mode\n");
    USER_DATA data;
#endif

    // These are used for testing only
    char out[50];
    uint8_t buffer[32];

    initHw();
    initTimer0(40e6);

	while(true)
	{
#ifndef REC

#else
	    if(rfIsDataAvailable())
	    {
	        setPinValue(RECV_LED, getPinValue(RECV_LED) ^ 1);
	        putsUart0("There is data in the receive FIFO\n");
	        uint32_t n = rfReceiveBuffer(buffer);
	        buffer[n] = '\0';
	        sprintf(out, "Received %d bytes of data\n", n);
            putsUart0(out);
            putsUart0((char*)buffer);
	    }
#endif
	}
}
