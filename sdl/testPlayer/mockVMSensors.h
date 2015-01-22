#ifndef __MOCKVM_SENSORS_H_
#define __MOCKVM_SENSORS_H_

#define MAX_CLIENTS 1000
#include "sensors.h"
#include "sensors_packet.pb-c.h"
#include "logger.h"

typedef struct s_sensor_params_acc
{
    SensorsPacket__SensorAccelerometerPayload* payload;
    uint32_t toread;
    uint32_t nbevent;
} sensor_params_acc;

void* mock_vm_recv_poll(void* args);

#endif
