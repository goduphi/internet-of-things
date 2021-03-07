/*
 * tcp.c
 * This file contains implementation of the TCP protocol
 *
 *  Created on: Feb 28, 2021
 *      Author: Sarker Nadir Afridi Azmi
 */

#include <stdint.h>
#include <stdbool.h>
#include "eth0.h"
#include "tcp.h"
#include "utils.h"
#include "mqtt.h"

// Checks to see if the ip payload is TCP
bool etherIsTcp(etherHeader* ether)
{
    ipHeader* ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)ip->data;

    uint16_t tcpLength = ntohs(ip->length) - (ip->revSize & 0xF) * 4;
    bool ok = (ip->protocol == 0x06);
    // Calculate the checksum to see if it is correct
    uint32_t sum = 0;
    if(ok)
    {
        /*
         * Pseudo-header of IP : 12 bytes
         * Source IP address
         * Destination IP address
         * Zero, Protocol : temp16
         * TCP length
         */
        etherSumWords(ip->sourceIp, 8, &sum);
        uint16_t temp16 = ip->protocol;
        temp16 = htons(temp16);
        etherSumWords(&temp16, 2, &sum);
        temp16 = htons(tcpLength);
        etherSumWords(&temp16, 2, &sum);
        etherSumWords(tcp, tcpLength, &sum);
        ok = (getEtherChecksum(sum) == 0);
    }
    return ok;
}

void sendTcp(etherHeader* ether, socket* s, socket* d, uint16_t flags, uint32_t sequenceNumber, uint32_t acknowledgementNumber,
             uint8_t* options, uint8_t optionsLength, uint16_t dataLength)
{
    ipHeader* ip = (ipHeader*)ether->data;
    tcpHeader* tcp = (tcpHeader*)ip->data;
    uint8_t* payload = (uint8_t*)tcp->data + optionsLength;

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
    // TCP has a value of 6
    ip->protocol = 0x06;
    copyUint8Array(s->ip, ip->sourceIp, 4);
    copyUint8Array(d->ip, ip->destIp, 4);

    // Fill up the tcp frame
    tcp->sourcePort = htons(s->port);
    tcp->destPort = htons(d->port);
    tcp->sequenceNumber = htonl(sequenceNumber);
    tcp->acknowledgementNumber = htonl(acknowledgementNumber);
    tcp->offsetFields = htons(flags);
    // This is just a random size I put in
    tcp->windowSize = htons(TCP_WINDOW_SIZE);
    tcp->checksum = 0;
    tcp->urgentPointer = 0;

    // Copy over the options to the buffer
    // The options begin at the end of our TCP structure
    if(options != 0)
        copyUint8Array(options, (uint8_t*)(tcp->data), optionsLength);

    // Calculate all the checksums
    // The 16 bit checksum includes the Pseudo-Header, TCP Header & TCP data
    uint32_t sum = 0;
    // The upper 4 bits of the flags field give us the length of the tcp header
    // The upper 4 bits should be multiplied by 4 to give the length of the header
    uint16_t tcpHeaderLength = ((flags >> 12) << 2), temp16 = 0;
    /*
     * Pseudo-header of IP : 12 bytes
     * Source IP address
     * Destination IP address
     * Zero, Protocol : temp16
     * TCP length
     */
    etherSumWords(ip->sourceIp, 8, &sum);
    temp16 = ip->protocol;
    temp16 = htons(temp16);
    etherSumWords(&temp16, 2, &sum);
    temp16 = htons(tcpHeaderLength + dataLength);
    etherSumWords(&temp16, 2, &sum);

    // There may be data that we need to find the checksum for here

    // Sum the words of the TCP header & data
    etherSumWords(tcp, tcpHeaderLength + dataLength, &sum);
    tcp->checksum = getEtherChecksum(sum);

    // Calculate the IP checksum now
    ip->length = htons(sizeof(ipHeader) + tcpHeaderLength + dataLength);
    etherCalcIpChecksum(ip);

    etherPutPacket(ether, sizeof(etherHeader) + sizeof(ipHeader) + tcpHeaderLength + dataLength);
}
