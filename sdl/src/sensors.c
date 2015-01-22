/**
 * \file sensors.c
 * \brief Sensor packets forwarders
 */

#include <pthread.h>  // for pthread_join, pthread_t
#include <stdint.h>   // for int32_t
#include <stdio.h>    // for NULL
#include <string.h>   // for strncmp
#include <stdlib.h>   // for calloc
#include <signal.h>   // for SIGPIPE, SIG_IGN, signal
#include <unistd.h>   // for sleep, close
#include <time.h>     // for nanosleep

#include "amqp_listen.h"
#include "config_env.h"
#include "logger.h"
#include "buffer_sizes.h"
#include "player_nfc.h"
#include "protobuf_framing.h"
#include "sensors.h"
#include "socket.h"

#define LOG_TAG "sensors"

#ifdef WITH_TESTING
/**
 * Write a protobuf
 */
static int write_protobuf_for_test(socket_t sock, amqp_envelope_t* envelope)
{
    return send(sock, envelope->message.body.bytes, envelope->message.body.len, 0);
}
#endif

static void* amqp_generic(void* args)
{
    amqp_envelope_t envelope;
    amqp_connection_state_t conn;
    int err_amqlisten = 0;
    int IS_SENSORS = 0;
    socket_t sock = -1;

    const sensor_params* params = (sensor_params*) args;
    if (!strncmp(params->sensor, "sensors", 7))
        IS_SENSORS = 1;
    else
        IS_SENSORS = 0;

    LOGM("listen_GPS_or_BATT_app - %s %s %s %s", params->exchange, params->queue, params->sensor,
         params->queue);

    amqp_listen_retry(params->amqp_host, 5672, params->queue, &conn, 5);
    if (IS_SENSORS)
        sock = open_socket_reuseaddr(params->gvmip, params->port);

    while (1)
    {
        if (!IS_SENSORS)
            sock = open_socket(params->gvmip, params->port);
        LOGD("FREQ - %s sock=%d", params->sensor, sock);
        if (sock != SOCKET_ERROR)
        {
            err_amqlisten = amqp_consume(&conn, &envelope);
            if (err_amqlisten == 0)
            {
#ifndef WITH_TESTING
                LOGM("Sending %d bytes to %s hardware device (:%d)", envelope.message.body.len + 4,
                     params->sensor, params->port);
                unsigned int size = write_protobuf(sock, &envelope);
                if (size != envelope.message.body.len + 4)
                    LOGW("Failed to send %d bytes to %s hardware device (:%d), error %d",
                         envelope.message.body.len + 4, params->sensor, params->port, size);
#else
                unsigned int size = write_protobuf_for_test(sock, &envelope);
                LOGM("Sending %d bytes ", size);
#endif
            }
        }
        else
        {
            LOGW("Unable to connect to hardware device %s (:%d)", params->sensor, params->port);

            if (IS_SENSORS)
            {
                close(sock);
                sleep(3);
                sock = open_socket_reuseaddr(params->gvmip, params->port);
            }
            else
            {
                sleep(3);
            }
        }

        struct timespec duration = {0, params->frequency * 1000};
        nanosleep(&duration, NULL);
        if (!IS_SENSORS)
            close(sock);
    }

    return NULL;
}

sensor_params* ParamEventsWorker(const char* vmip, const char* vmid, const char* sensor_name,
                                 const char* amqp_host)
{
    const int32_t str_length = BUF_SIZE;
    sensor_params* paramListener = (sensor_params*) calloc(sizeof(sensor_params), 1);
    if (!paramListener)
        LOGE("ParamEventsWorker: out of memory");

    paramListener->amqp_host = amqp_host;
    paramListener->gvmip = vmip;

    g_strlcpy(paramListener->sensor, sensor_name, str_length);
    g_strlcpy(paramListener->exchange, sensor_name, str_length);
    snprintf(paramListener->queue, str_length, "android-events.%s.%s", vmid, sensor_name);

    if (!strncmp(sensor_name, "battery", 7))
    {
        paramListener->port = PORT_BAT;
        paramListener->frequency = FREQ_BAT;
    }
    else if (!strncmp(sensor_name, "sensors", 7))
    {
        paramListener->port = PORT_SENSORS;
        paramListener->frequency = FREQ_SENSORS;
    }
    else if (!strncmp(sensor_name, "gps", 3))
    {
        paramListener->port = PORT_GPS;
        paramListener->frequency = FREQ_GPS;
    }
    else if (!strncmp(sensor_name, "gsm", 3))
    {
        paramListener->port = PORT_GSM;
        paramListener->frequency = FREQ_DEFAULT;
    }
    else if (!strncmp(sensor_name, "nfc", 3))
    {
        paramListener->port = PORT_NFC;
        paramListener->frequency = FREQ_DEFAULT;
    }
    else
        LOGE("Unkwnown sensor type: %s", sensor_name);

    LOGD("ParamEventsWorker - %d ; %s ; %s ; %s ", paramListener->port, paramListener->gvmip,
         paramListener->exchange, paramListener->queue);

    return paramListener;
}

void start_sensor(sensor_params* params, pthread_t* thread)
{
    pthread_create(thread, 0, &amqp_generic, params);
}

#ifndef WITH_TESTING
int main()
{
    char* amqp_host = NULL;
    char* vmid = NULL;
    char* vmip = NULL;

    int gps = 0;
    int gsm = 0;
    int nfc = 0;
    int battery = 0;
    int sensors = 0;

    pthread_t threadbat;
    pthread_t threadsens;
    pthread_t threadgps;
    pthread_t threadgsm;

    signal(SIGPIPE, SIG_IGN);
    LOGI("Starting sensor listening");

    amqp_host = configvar_string("AIC_PLAYER_AMQP_HOST");
    vmid = configvar_string("AIC_PLAYER_VM_ID");
    vmip = configvar_string("AIC_PLAYER_VM_HOST");
    sensors = configvar_bool("AIC_PLAYER_ENABLE_SENSORS");
    battery = configvar_bool("AIC_PLAYER_ENABLE_BATTERY");
    gps = configvar_bool("AIC_PLAYER_ENABLE_GPS");
    gsm = configvar_bool("AIC_PLAYER_ENABLE_GSM");
    nfc = configvar_bool("AIC_PLAYER_ENABLE_NFC");

    // ensure the variables are there
    configvar_string("AIC_PLAYER_AMQP_PASSWORD");
    configvar_string("AIC_PLAYER_AMQP_USERNAME");

    if (battery)
        start_sensor(ParamEventsWorker(vmip, vmid, "battery", amqp_host), &threadbat);
    if (sensors)
        start_sensor(ParamEventsWorker(vmip, vmid, "sensors", amqp_host), &threadsens);
    if (gps)
        start_sensor(ParamEventsWorker(vmip, vmid, "gps", amqp_host), &threadgps);
    if (gsm)
        start_sensor(ParamEventsWorker(vmip, vmid, "gsm", amqp_host), &threadgsm);
    if (nfc)
        listen_NFC(ParamEventsWorker(vmip, vmid, "nfc", amqp_host));

    if (battery)
        pthread_join(threadbat, NULL);
    if (sensors)
        pthread_join(threadsens, NULL);
    if (gps)
        pthread_join(threadgps, NULL);
    if (gsm)
        pthread_join(threadgsm, NULL);

    return 0;
}
#endif  // UNIT_TESTING
