/*
 * mqtt.h
 *
 *  Created on: Mar 2, 2021
 *      Author: Sarker Nadir Afridi Azmi
 */

#ifndef MQTT_H_
#define MQTT_H_

#define PINGREQ     12
#define PINGRES     13
#define DISCONN     14

typedef struct _mqttPayload
{
    uint16_t clientId;
} mqttPayload;

#endif /* MQTT_H_ */
