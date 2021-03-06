#include <stdint.h>
#include <stdbool.h>
#include "uart0.h"
#include "cli.h"
#include "utils.h"

/*
 * Field number 2 3 4 5 should be integers representing
 * the IPv4 address.
 * The format is www.xxx.yyy.zzz
 */
bool isIpv4Address(USER_DATA* data, uint8_t currentOffsetFromCommand)
{
    /* Since the command is set IP www.xxx.yyy.zzz, there should be
     * a total of 6 fields
     */
    if(data->fieldCount != 6)
        return false;
    uint8_t i = currentOffsetFromCommand + 1;
    for(; i <= currentOffsetFromCommand + 4; i++)
        if(getFieldInteger(data, i) > 255)
            return false;
    return true;
}

/*
 * This function should be called in conjunction with the isIpv4Address function
 */
uint32_t getIpv4Address(USER_DATA* data, uint8_t currentOffsetFromCommand)
{
    uint32_t ip = 0;
    uint8_t i = currentOffsetFromCommand + 1;
    for(; i <= currentOffsetFromCommand + 4; i++)
        ip |= getFieldInteger(data, i) << (24 - ((i - currentOffsetFromCommand - 1) << 3));
    return ip;
}

/*
 * This function takes the ip encoded using the getIpv4Address
 * and puts that inside of an array of bytes. The most significant
 * octet is in position 0 of the array.
 */
void convertEncodedIpv4ToArray(uint8_t ipv4[], uint32_t encodedIpv4)
{
    uint8_t i = 0;
    for(i = 0; i < 4; i++)
        ipv4[i] = (encodedIpv4 >> (24 - (i << 3))) & 0xFF;
}

void printIpv4(uint8_t ipv4[])
{
    uint8_t i = 0;
    for(i = 0; i < 4; i++)
    {
        printUint8InDecimal(ipv4[i]);
        if(i != 3)
            putcUart0('.');
    }
}

void printMac(uint8_t mac[])
{
    uint8_t i = 0;
    for(i = 0; i < 6; i++)
    {
        printUint8InHex(mac[i]);
        if(i != 5)
            putcUart0(':');
    }
}

void copyUint8Array(uint8_t src[], uint8_t dest[], uint8_t size)
{
    uint8_t i = 0;
    for(i = 0; i < size; i++)
        dest[i] = src[i];
}

// Encodes a string as utf-8
void encodeUtf8(void* packet, uint16_t length, char* string)
{
    uint8_t* tmp = (uint8_t*)packet;
    *(tmp++) = (length >> 8) & 0xFF;
    *(tmp++) = length & 0xFF;
    if(length <= 0 || string == 0)
        return;
    while(*string)
        *(tmp++) = *(string++);
}

uint16_t strLen(const char* str)
{
    uint8_t i = 0;
    while(str[i])
        i++;
    return i;
}

uint16_t middleSquareMethodRand(uint8_t digits, uint16_t seed)
{
    uint16_t n = seed * seed;
    uint8_t tmp[digits << 1];
    int8_t i = 0, start = 0, end = (digits << 1) - 1;
    for(i = end; i >= 0; i--)
    {
        tmp[i] = (n % 10);
        n /= 10;
    }
    start = digits >> 1;
    end = start + digits;
    n = 0;
    for(i = start; i < end; i++)
    {
        n = (n * 10) + tmp[i];
    }
    return n;
}

void copySubscribeArguments(USER_DATA* data, char* buffer, uint32_t* totalLength)
{
    uint8_t i = 0;
    for(i = 1; i < data->fieldCount; i++)
    {
        copyUint8Array(getFieldString(data, i), buffer, strLen(getFieldString(data, i)));
        buffer[strLen(getFieldString(data, i))] = '\0';
        buffer += strLen(getFieldString(data, i)) + 1;
        *totalLength += strLen(getFieldString(data, i));
    }
}
