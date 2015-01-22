#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "sensors.h"
#include "nfc.pb-c.h"
#include "buffer_sizes.h"
#include "socket.h"
#include "amqp_listen.h"
#include "protobuf_framing.h"
#include "logger.h"
#include "player_nfc.h"

#define LOG_TAG "player_nfc"

void* listen_NFC(void* args)
{
    sensor_params* params = (sensor_params*) args;

    int err_amqlisten = 0;

    amqp_envelope_t envelope;
    amqp_connection_state_t conn;
    LOGM("listen_NFC - %s %s %s %s", params->exchange, params->queue, params->sensor,
         params->queue);

    amqp_listen_retry(params->amqp_host, 5672, params->queue, &conn, 5);
    socket_t sock = open_socket_reuseaddr(params->gvmip, params->port);

    int size = 0;
    while (1)
    {
        LOGM("FREQ - %s sock=%d", params->sensor, sock);
        if (sock != SOCKET_ERROR)
        {
            LOGM(" NFC ready waiting data from amqp");
            err_amqlisten = amqp_consume(&conn, &envelope);
            if (err_amqlisten == 0)
            {
                size = write_protobuf(sock, &envelope);
                LOGM("NFC send to nfcd size=%d, on socket=%d", size, sock);

                close(sock);
#ifdef WITH_TESTING
                pthread_exit(0);
#endif
            }
        }
        else
        {
            LOGW("Unable to connect to hardware device %s (:%d)", params->sensor, params->port);
            close(sock);
            sleep(3);
            sock = open_socket_reuseaddr(params->gvmip, params->port);
        }
        struct timespec duration = {0, params->frequency * 1000};
        nanosleep(&duration, NULL);
    }
    return NULL;
}
