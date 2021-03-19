/*
 * tcp.h
 *
 *  Created on: Feb 27, 2021
 *      Author: afrid
 */

#ifndef TCP_H_
#define TCP_H_

#include <stdint.h>
#include <stdbool.h>
#include "eth0.h"

#define TIMEOUT_2MS         2000

#define TCP_WINDOW_SIZE     1460

#define SYN                 0x0002
#define ACK                 0x0010
#define PSH                 0x0008
#define FIN                 0x0001

typedef struct _socket
{
    uint8_t ip[4];
    uint16_t port;
    uint8_t mac[6];
} socket;

typedef enum _sendTcpArgs
{
    NO_OPTIONS = 0,
    ZERO_LENGTH = 0,
} sendTcpArgs;

uint16_t getPayloadSize(etherHeader* ether);
void sendTcp(etherHeader* ether, socket* s, socket* d, uint16_t flags, uint32_t sequenceNumber, uint32_t acknowledgementNumber,
             uint8_t options[], uint8_t optionLength, uint16_t dataLength);
bool etherIsTcp(etherHeader* ether);

#endif /* TCP_H_ */
