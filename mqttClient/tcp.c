/*
 * tcp.c
 *
 *  Created on: Feb 28, 2021
 *      Author: afrid
 */

#include <stdint.h>
#include <stdbool.h>
#include "eth0.h"
#include "tcp.h"
#include "utils.h"

void sendTcp(etherHeader* ether, socket* s, socket* d, uint16_t flags, uint16_t dataLength)
{
    ipHeader* ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)ip->data;

    // Fill up the ethernet frame
    copyUint8Array(s->mac, ether->sourceAddress, 6);
    copyUint8Array(d->mac, ether->destAddress, 6);
    // For IP, use 0x0800 and convert to network byte order
    ether->frameType = htons(0x0800);
    // Fill up the ip header
    // The upper 4 bits represents the IP version which is IPv4
    // The lower 4 bits represents the length of the header
    // which is 5 which gets multiplied by 4 to get 20
    ip->revSize = 0x45;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = 0x06;                           // TCP has a value of 6
    copyUint8Array(s->ip, ip->sourceIp, 4);
    copyUint8Array(d->ip, ip->destIp, 4);

    // Fill up the tcp frame
    tcp->sourcePort = htons(s->port);
    tcp->destPort = htons(d->port);
    tcp->sequenceNumber = htonl(200);
    tcp->acknowledgementNumber = htonl(0);
    tcp->offsetFields = htons(flags);
    tcp->windowSize = htons(1500);                 // This is just a random size I put in
    tcp->checksum = 0;
    tcp->urgentPointer = 0;

    // Calculate all the checksums
    // The 16 bit checksum includes the Pseudo-Header, TCP Header & TCP data
    uint32_t sum = 0;
    /*
     * Pseudo-header of IP : 12 bytes
     * Source IP address
     * Destination IP address
     * Zero : Protocol
     * TCP length
     */
    etherSumWords(ip->sourceIp, 8, &sum);
    uint16_t zeroProtocol16 = ip->protocol;
    zeroProtocol16 = htons(zeroProtocol16);
    etherSumWords(&zeroProtocol16, 2, &sum);
    // The upper 4 bits of the flags field give us the length of the tcp header
    uint16_t tcpLength = htons((flags >> 12) << 2);
    etherSumWords(&tcpLength, 2, &sum);

    // There may be data that we need to find the checksum for here

    // Sum the words of the TCP header
    etherSumWords(tcp, ((flags >> 12) << 2) + dataLength, &sum);
    tcp->checksum = getEtherChecksum(sum);

    // Calculate the IP checksum now
    ip->length = htons(40);
    etherCalcIpChecksum(ip);

    etherPutPacket(ether, 54);
}
