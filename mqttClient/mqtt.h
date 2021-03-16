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

#define QOS0                    0
#define QOS1                    2
#define QOS2                    4

typedef enum _packetType
{
    MQTT_CONNECT = 0x10,
    CONNACK = 0x20,
    PUBLISH = 0x30,
    PINGERQ = 0xC0,
    PINGRESP = 0xD0,
    DISCONNECT = 0xE0
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

#define MAX_TOPIC_NAME_LENGTH       80

typedef struct _publishVariableHeader
{
    uint16_t topicLength;
    char topicName[MAX_TOPIC_NAME_LENGTH];
    uint16_t packetIdentifier;
} publishVariableHeader;

void assembleMqttConnectPacket(uint8_t* packet, uint8_t flags, char* clientId, uint16_t cliendIdLength, uint16_t* packetLength);
void assembleMqttPacket(uint8_t* packet, packetType type, uint16_t* packetLength);
void assembleMqttPublishPacket(uint8_t* packet, char* topicName, uint16_t packetIdentifier, uint8_t qos, char* payload, uint16_t* packetLength);
bool mqttIsConnack(uint8_t* packet);
bool mqttIsPingResponse(uint8_t* packet);

#endif /* MQTT_H_ */
