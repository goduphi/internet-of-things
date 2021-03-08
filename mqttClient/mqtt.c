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

void assembleMqttConnectPacket(uint8_t* packet, uint8_t flags, char* clientId, uint16_t cliendIdLength, uint8_t* packetLength)
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
