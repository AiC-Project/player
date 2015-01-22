/**
 * \file sensors.h
 * \brief Defines ports and structures for sensor threads
*/

#ifndef __SENSORS_H_
#define __SENSORS_H_

/** \brief Port for the "sensors" command in the VM */
#define PORT_SENSORS 22471
/** \brief Port for the battery command in the VM */
#define PORT_BAT 22473
/** \brief Port for the GPS command in the VM */
#define PORT_GPS 22475
/** \brief Port for the GSM command in the VM */
#define PORT_GSM 6704
/** \brief Port for the NFC command in the VM */
#define PORT_NFC 22800

#define FREQ_SENSORS 100000       // frequency sending data sensors in micro seconds
#define FREQ_BAT 2 * 1000000      // frequency sending data battery in micro seconds
#define FREQ_GPS 2 * 1000000      // frequency sending data GPS  in micro seconds
#define FREQ_DEFAULT 1 * 1000000  // frequency default in micro seconds

#include <stdint.h>
#include <pthread.h>

#include "buffer_sizes.h"

/** \brief Parameter for sensor threads */
typedef struct s_sensor_params
{
    /** \brief Remote port */
    int32_t port;
    /** \brief Sensor name */
    char sensor[BUF_SIZE];
    /** \brief Exchange name */
    char exchange[BUF_SIZE];
    /** \brief Queue name */
    char queue[BUF_SIZE];
    /** \brief VM IP */
    const char* gvmip;
    /** \brief AMQP host */
    const char* amqp_host;
    /** \brief Sensor throttling */
    int32_t frequency;
    /** \brief Grabber-specific ?? */
    int8_t flagRecording;
} sensor_params;

/** \brief Creates the data structure for sensor threads
 * \param vmip IP address of the VM
 * \param vmid Identifier of the VM
 * \param sensor_name Name of the sensor
 * \param amqp_host Host of the RabbitMQ server
 * \returns An initialized data structure.
 */
sensor_params* ParamEventsWorker(const char* vmip, const char* vmid, const char* sensor_name,
                                 const char* amqp_host);

/** \brief Start the sensor listener thread
 * \param params The sensor parameter struct
 * \param thread a reference to the thread to start
 */
void start_sensor(sensor_params* params, pthread_t* thread);

#endif
