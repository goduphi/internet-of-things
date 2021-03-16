/*
 * mqtt.c
 *
 *  Created on: Mar 6, 2021
 *      Author: Sarker Nadir Afridi Azmi
 */

#include <stdint.h>
#include <stdbool.h>
#include "eth0.h"
#include "utils.h"
#include "mqtt.h"

// The offset determines how many byte we have encoded
uint32_t encodeMqttRemainingLength(uint32_t X, uint8_t* offset)
{
    unsigned int encodedByte = 0;
    uint8_t i = 0;
    do
    {
        encodedByte |= (X % 128) << (i++ << 3);
        X = X / 128;
        if(X > 0)
            encodedByte |= 128;
        else
        {
            *(offset) = i;
            return encodedByte;
        }
    }
    while(X > 0);
    return encodedByte;
}

uint32_t decodeMqttRemainingLength(uint32_t X)
{
    return 0;
}

void assembleMqttConnectPacket(uint8_t* packet, uint8_t flags, char* clientId, uint16_t cliendIdLength, uint16_t* packetLength)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    connectVariableHeader* variableHeader = (connectVariableHeader*)(mqttFixedHeader->remainingLength + 1);
    uint8_t* payload = (uint8_t*)variableHeader->data;

    mqttFixedHeader->controlHeader = (uint8_t)MQTT_CONNECT;
    mqttFixedHeader->remainingLength[0] = sizeof(connectVariableHeader) + sizeof(cliendIdLength) + cliendIdLength;
    *(packetLength) = 2 + mqttFixedHeader->remainingLength[0];
    // As a test, let's only fill up the connect message
    // The length and protocol name are fixed
    variableHeader->length = htons(4);
    variableHeader->connectMessage[0] = 'M';
    variableHeader->connectMessage[1] = 'Q';
    variableHeader->connectMessage[2] = 'T';
    variableHeader->connectMessage[3] = 'T';
    // v3.1.1
    variableHeader->protocolLevel = PROTOCOL_LEVEL_V311;
    variableHeader->connectFlags = flags;
    // For now, a default value of 10s is enough
    variableHeader->keepAlive = htons(10);

    encodeUtf8(payload, cliendIdLength, clientId);
}

void assembleMqttPacket(uint8_t* packet, packetType type, uint16_t* packetLength)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    mqttFixedHeader->controlHeader = (uint8_t)type;
    mqttFixedHeader->remainingLength[0] = 0;
    // This packet only contains the control header length
    *(packetLength) = 2 + mqttFixedHeader->remainingLength[0];
}

void assembleMqttPublishPacket(uint8_t* packet, char* topicName, uint16_t packetIdentifier, uint8_t qos, char* payload, uint16_t* packetLength)
{
    // For me, a packet identifier of 0 is invalid
    if(packetIdentifier > 0 && qos < 1)
    {
        // Insert some error handling function
        return;
    }

    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    mqttFixedHeader->controlHeader = (uint8_t)PUBLISH | qos;

    publishVariableHeader* variableHeader = (publishVariableHeader*)(mqttFixedHeader->remainingLength + 1);

    // Since we are using utf-8 encoding for the payload, we need to add the size of the uint16_t for the length
    mqttFixedHeader->remainingLength[0] =  sizeof(uint16_t) + strLen(topicName) + sizeof(uint16_t) + strLen(payload);

    // The size of the fixed header plus the remaining length
    *(packetLength) = 2 + mqttFixedHeader->remainingLength[0];
    encodeUtf8(variableHeader, strLen(topicName), topicName);

    uint8_t* p = (mqttFixedHeader->remainingLength + 1) + (sizeof(uint16_t) + strLen(topicName));
    encodeUtf8(p, strLen(payload), payload);
}

bool mqttIsConnack(uint8_t* packet)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    connackVariableHeader* variableHeader = (connackVariableHeader*)(mqttFixedHeader->remainingLength + 1);
    if(mqttFixedHeader->controlHeader != (uint8_t)CONNACK || mqttFixedHeader->remainingLength[0] != 2)
        return false;
    if(variableHeader->connectAcknowledgementFlags != 0 || variableHeader->connectReturnCode != 0)
        return false;
    return true;
}

bool mqttIsPingResponse(uint8_t* packet)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    if(mqttFixedHeader->controlHeader == (uint8_t)PINGRESP && mqttFixedHeader->remainingLength[0] == 0)
        return true;
    return false;
}
