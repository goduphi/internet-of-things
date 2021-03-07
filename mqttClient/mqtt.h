/*
 * mqtt.h
 *
 *  Created on: Mar 2, 2021
 *      Author: Sarker Nadir Afridi Azmi
 */

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_LEVEL_V311     0x04

// 3.1.2.3 Connect flags
#define CLEAN_SESSION           2
#define WILL_FLAGS              4
#define WILL_RETAIN             32
#define PASSWORD_FLAG           64
#define USER_NAME_FLAG          128

typedef enum _packetType
{
    MQTT_CONNECT = 0x10,
    CONNACK = 0x20
} packetType;

typedef struct _fixedHeader
{
    uint8_t controlHeader;
    uint8_t remainingLength[4];
    uint8_t data[0];
} fixedHeader;

typedef struct _connectVariableHeader
{
    uint16_t length;
    char connectMessage[4];
    uint8_t protocolLevel;
    uint8_t connectFlags;
    uint16_t keepAlive;
    uint8_t data[0];
} connectVariableHeader;

typedef struct _connackVariableHeader
{
    uint8_t connectAcknowledgementFlags;
    uint8_t connectReturnCode;
} connackVariableHeader;

void assembleMqttConnectPacket(uint8_t* packet, uint8_t flags, char* clientId, uint16_t cliendIdLength, uint8_t* packetLength);
bool mqttIsConnack(uint8_t* packet);

#endif /* MQTT_H_ */
