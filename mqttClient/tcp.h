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

#define SYN           0x0002
#define ACK           0x0010

typedef struct _socket
{
    uint8_t ip[4];
    uint16_t port;
    uint8_t mac[6];
} socket;

typedef struct _tcpPseudoHeader
{
    uint8_t sourceIp[4];
    uint8_t destIp[4];
    uint8_t zero;
    uint8_t protocol;
    uint16_t tcpLength;
} tcpPseudoHeader;

void sendTcp(etherHeader* ether, socket* s, socket* d, uint16_t flags, uint16_t dataLength);

#endif /* TCP_H_ */
