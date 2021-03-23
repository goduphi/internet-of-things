/*
 * utils.h
 *
 *  Created on: Feb 26, 2021
 *      Author: Sarker Nadir Afridi Azmi
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include <stdbool.h>
#include "cli.h"

bool isIpv4Address(USER_DATA* data, uint8_t currentOffsetFromCommand);
uint32_t getIpv4Address(USER_DATA* data, uint8_t currentOffsetFromCommand);
void convertEncodedIpv4ToArray(uint8_t ipv4[], uint32_t encodedIpv4);
void printIpv4(uint8_t ipv4[]);
void printMac(uint8_t mac[]);
void copyUint8Array(uint8_t src[], uint8_t dest[], uint8_t size);
void encodeUtf8(void* packet, uint16_t length, char* string);
uint16_t strLen(const char* str);
uint16_t middleSquareMethodRand(uint8_t digits, uint16_t seed);
void copySubscribeArguments(USER_DATA* data, char* buffer, uint32_t* totalLength);

#endif /* UTILS_H_ */
