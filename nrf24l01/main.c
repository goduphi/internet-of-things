/**
 * Author: Sarker Nadir Afridi Azmi
 *
 * Description: A simple protocol less application which can
 * send information wirelessly using the RF24L01 module.
 */

#include <stdint.h>
#include <stdbool.h>
#include "clock.h"
#include "gpio.h"
#include "spi1.h"
#include "uart0.h"
#include "common_terminal_interface.h"
#include "nrf24l01.h"
#include <stdio.h>

#define ADDRESS0    0xACCE55

//#define REC

int main(void)
{
    initSystemClockTo40Mhz();
    initNrf24l01();

    // Only use one data pipe to transmit data
    rfSetAddress(RX_ADDR_P0, ADDRESS0);
    rfSetAddress(TX_ADDR, ADDRESS0);

#ifdef REC
    rfSetMode(RX, 20);
#else
    rfSetMode(TX, 20);
#endif

    initUart0();
    setUart0BaudRate(115200, 40e6);

#ifdef REC
    putsUart0("This board is in RX mode\n");
#else
    putsUart0("This board is in TX mode\n");
#endif

    USER_DATA data;

    // These are used for testing only
    char out[50];
    uint8_t buffer[32];

	while(true)
	{
#ifndef REC
	    getsUart0(&data);
	    parseField(&data);
	    if(isCommand(&data, "status", 0))
	    {
	        sprintf(out, "Status = %d\n", rfReadRegister(getInteger(&data, data.fieldPosition[1])));
	        putsUart0(out);
	    }
	    if(isCommand(&data, "addr", 0))
	    {
            rfReadIntoBuffer(0x0A, buffer, 4);
            sprintf(out, "RX = %x-%x-%x-%x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
            putsUart0(out);
            rfReadIntoBuffer(0x10, buffer, 4);
            sprintf(out, "TX = %x-%x-%x-%x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
            putsUart0(out);
	    }
	    if(isCommand(&data, "send", 0))
	    {
	        uint8_t i = 0;
	        for(i = 0; i < data.fieldCount - 1; i++)
	        {
	            buffer[i] = getInteger(&data, data.fieldPosition[1 + i]);
	        }
	        rfSendBuffer(buffer, data.fieldCount - 1);
	    }
#else
	    if(rfIsDataAvailable())
	    {
	        putsUart0("There is data in the receive FIFO\n");
	        uint32_t n = rfReceiveBuffer(buffer);
	        sprintf(out, "Received %d bytes of data\n", n);
            putsUart0(out);
	        uint8_t i = 0;
	        for(i = 0; i < n; i++)
	        {
	            sprintf(out, "Data[%d] = %d\n", i, buffer[i]);
                putsUart0(out);
	        }
	    }
#endif
	}
}
