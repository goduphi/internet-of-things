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

typedef struct _socket
{
    uint8_t ip[4];
    uint16_t port;
    uint8_t mac[6];
} socket;

void sendTcp(etherHeader* ether, socket* s, socket* d, uint16_t flags);

#endif /* TCP_H_ */
