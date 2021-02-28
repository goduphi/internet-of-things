/*
 * tcp.h
 *
 *  Created on: Feb 27, 2021
 *      Author: afrid
 */

#ifndef TCP_H_
#define TCP_H_

typedef struct _socket
{
    uint8_t ip[4];
    uint16_t port;
    uint8_t mac[6];
} socket;

#endif /* TCP_H_ */
