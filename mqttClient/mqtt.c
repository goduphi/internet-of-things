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
#include "cli.h"

// The offset determines how many byte we have encoded
uint32_t encodeMqttRemainingLength(uint32_t X, uint8_t* offset)
{
    uint32_t encodedByte = 0;
    uint8_t i = 0;
    do
    {
        encodedByte |= (X & 127) << (i << 3);
        X = X / 128;
        if(X > 0)
            encodedByte |= (128 << (i << 3));
        else
        {
            *offset = i + 1;
            return encodedByte;
        }
        i++;
    }
    while(X > 0);
    return encodedByte;
}

uint32_t decodeMqttRemainingLength(uint32_t X)
{
    uint32_t size = 0;
    uint8_t i = 0;
    while(X)
    {
        size |= (X & 0x0000007F) << (i++ * 7);
        if(X & 128)
            X >>= 8;
        else
        {
            return size;
        }
    }
    return size;
}

void assembleMqttConnectPacket(uint8_t* packet, uint8_t flags, char* clientId, uint16_t cliendIdLength, uint16_t* packetLength)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;

    uint8_t offset = 0;
    uint32_t remainingLength = encodeMqttRemainingLength((sizeof(connectVariableHeader) + sizeof(cliendIdLength) + cliendIdLength), &offset);

    connectVariableHeader* variableHeader = (connectVariableHeader*)(mqttFixedHeader->remainingLength + offset);
    uint8_t* payload = (uint8_t*)variableHeader->data;

    mqttFixedHeader->controlHeader = (uint8_t)MQTT_CONNECT;
    uint8_t i = 0;
    for(i = 0; i < offset; i++)
        mqttFixedHeader->remainingLength[i] = (remainingLength >> (i << 3)) & 0xFF;
    *(packetLength) = 2 + remainingLength;
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
    // For now, a default value of 100s is enough
    variableHeader->keepAlive = htons(100);

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

    uint8_t offset = 0;
    // Since we are using utf-8 encoding for the payload, we need to add the size of the uint16_t for the length
    uint32_t remainingLength = (sizeof(uint16_t) + strLen(topicName) + sizeof(uint16_t) + strLen(payload)) + ((packetIdentifier > 0) ? sizeof(uint16_t) : 0);
    remainingLength = encodeMqttRemainingLength(remainingLength, &offset);

    uint8_t i = 0;
    for(i = 0; i < offset; i++)
        mqttFixedHeader->remainingLength[i] = (remainingLength >> (i << 3)) & 0xFF;

    // The size of the fixed header plus the remaining length
    *(packetLength) = 2 + remainingLength;
    encodeUtf8((mqttFixedHeader->remainingLength + offset), strLen(topicName), topicName);

    uint8_t* tmp = 0;
    if(packetIdentifier > 0)
    {
        // Get the offset to the packet identifier field
        tmp = (mqttFixedHeader->remainingLength + offset) + sizeof(uint16_t) + strLen(topicName);
        encodeUtf8(tmp, packetIdentifier, 0);
    }

    // This is the payload section
    tmp = (mqttFixedHeader->remainingLength + offset) + (sizeof(uint16_t) + strLen(topicName)) + ((packetIdentifier > 0) ? sizeof(uint16_t) : 0);
    encodeUtf8(tmp, strLen(payload), payload);
}

void assembleMqttSubscribePacket(uint8_t* packet, uint16_t packetIdentifier, char* topic, uint8_t qos, uint16_t* packetLength)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    mqttFixedHeader->controlHeader = (uint8_t)SUBSCRIBE;

    uint8_t offset = 0;
    // Since we are using utf-8 encoding for the payload, we need to add the size of the uint16_t for the length
    uint32_t remainingLength = sizeof(packetIdentifier) + (sizeof(uint16_t) + strLen(topic)) + sizeof(qos);
    *packetLength = 2 + remainingLength;
    remainingLength = encodeMqttRemainingLength(remainingLength, &offset);

    uint8_t i = 0;
    for(i = 0; i < offset; i++)
        mqttFixedHeader->remainingLength[i] = (remainingLength >> (i << 3)) & 0xFF;

    // Variable header only has the packet identifier
    uint8_t* tmp = mqttFixedHeader->remainingLength + offset;
    encodeUtf8(tmp, packetIdentifier, 0);
    tmp += sizeof(uint16_t);

    // Add the payload
    encodeUtf8(tmp, strLen(topic), topic);
    tmp += sizeof(uint16_t) + strLen(topic);
    *tmp = qos;
}

void getTopicData(uint8_t* packet, subscription* data)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    data->remainingLength = decodeMqttRemainingLength(mqttFixedHeader->remainingLength[0]);
    uint8_t* tmp = mqttFixedHeader->remainingLength + 1;
    uint16_t topicNameLength = *tmp | *(tmp + 1);
    tmp += 2;
    uint8_t i = 0;
    for(i = 0; i < topicNameLength; i++)
        data->topicName[i] = *(tmp++);
    data->topicName[i] = '\0';
    // The message would be at an offset of 2 bytes + length of topic name
    uint8_t messageLength = data->remainingLength - (sizeof(uint16_t) + topicNameLength);
    for(i = 0; i <  messageLength; i++)
        data->message[i] = *(tmp++);
    data->message[i] = '\0';
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

bool mqttIsPublishPacket(uint8_t* packet)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    if(mqttFixedHeader->controlHeader == (uint8_t)PUBLISH)
        return true;
    return false;
}

bool mqttIsPuback(uint8_t* packet, uint16_t packetIdentifier)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    if(mqttFixedHeader->controlHeader != (uint8_t)PUBACK || mqttFixedHeader->remainingLength[0] != 2)
        return false;
    uint16_t receivedPacketIdentifier = *(mqttFixedHeader->remainingLength + 1) | *(mqttFixedHeader->remainingLength + 2);
    if(packetIdentifier != receivedPacketIdentifier)
        return false;
    return true;
}

uint8_t getSubackPayload(uint8_t* packet)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    // Payload offset = sizeof(uint8_t) + sizeof(uint16_t)
    uint8_t* payload = (mqttFixedHeader->remainingLength + 1) + sizeof(uint16_t);
    // Format: X 0 0 0 0 0 X X
    return (((*payload) >> 2) & 31);
}

bool mqttIsSuback(uint8_t* packet, uint16_t packetIdentifier, uint8_t numberOfTopics)
{
    fixedHeader* mqttFixedHeader = (fixedHeader*)packet;
    // Remaining length = variable header length (2 bytes) + payload length (numberOfTopics)
    if(mqttFixedHeader->controlHeader != (uint8_t)SUBACK || mqttFixedHeader->remainingLength[0] != 2 + numberOfTopics)
        return false;
    uint16_t receivedPacketIdentifier = *(mqttFixedHeader->remainingLength + 1) | *(mqttFixedHeader->remainingLength + 2);
    if(packetIdentifier != receivedPacketIdentifier)
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
