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

#endif /* UTILS_H_ */
