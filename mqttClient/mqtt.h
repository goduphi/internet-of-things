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

#define MAX_TOPIC_NAME_SIZE     10
#define MAX_MESSAGE_SIZE        60

#define DEFAULT_KEEP_ALIVE      100

// 3.1.2.3 Connect flags
#define CLEAN_SESSION           2
#define WILL_FLAGS              4
#define WILL_RETAIN             32
#define PASSWORD_FLAG           64
#define USER_NAME_FLAG          128

#define QOS0                    0
#define QOS1                    2
#define QOS2                    4

#define SUBACK_FAILURE          0x80

typedef enum _packetType
{
    MQTT_CONNECT = 0x10,
    CONNACK = 0x20,
    PUBLISH = 0x30,
    SUBSCRIBE = 0x82,
    SUBACK = 0x90,
    UNSUBSCRIBE = 0xA2,
    UNSUBACK = 0xB0,
    PUBACK = 0x40,
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


typedef struct _subscription
{
    char topicName[MAX_TOPIC_NAME_SIZE];
    char message[MAX_MESSAGE_SIZE];
    uint32_t remainingLength;
} subscription;

void assembleMqttConnectPacket(uint8_t* packet, uint8_t flags, uint16_t keepAlive, char* clientId, uint16_t cliendIdLength, uint16_t* packetLength);
void assembleMqttPacket(uint8_t* packet, packetType type, uint16_t* packetLength);
void assembleMqttPublishPacket(uint8_t* packet, char* topicName, uint16_t packetIdentifier, uint8_t qos, char* payload, uint16_t* packetLength);
void assembleMqttSubscribeUnsubscribePacket(uint8_t* packet, packetType type, uint16_t packetIdentifier, char* topic, uint32_t totalLength, uint8_t numberOfTopics, uint8_t qos, uint16_t* packetLength);
void getTopicData(uint8_t* packet, subscription* data);
bool mqttIsConnack(uint8_t* packet);
bool mqttIsPublishPacket(uint8_t* packet);
bool mqttIsPuback(uint8_t* packet, uint16_t packetIdentifier);
uint8_t getSubackPayload(uint8_t* packet);
bool mqttIsAck(uint8_t* packet, packetType type, uint16_t packetIdentifier, uint8_t numberOfTopics);
bool mqttIsPingResponse(uint8_t* packet);

#endif /* MQTT_H_ */
