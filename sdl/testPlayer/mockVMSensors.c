
#include "mockVMSensors.h"
#include "logger.h"
#include "socket.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define LOG_TAG "mockVMSensors"

int acceptNewClient(int serverSock, int* clientsSocks)
{
    int j = 0;
    int clientSock = accept(serverSock, NULL, NULL);
    if (clientSock == -1)
    {
        LOGI("Client connection refused ");
        return -1;
    }

    // Pick a slot in the client sockets list
    for (j = 0; j < MAX_CLIENTS; j++)
    {
        if (clientsSocks[j] == 0)
        {
            clientsSocks[j] = clientSock;
            LOGI("Client connection accepted (%d)", clientSock);
            break;
        }
    }
    // No space left in the client sockets table
    if (j >= MAX_CLIENTS)
    {
        LOGI("Too many clients, connection refused");
        return -1;
    }
    return 0;
}

/* open listen() port on any interface */
int socket_inaddr_any_server(int port, int type)
{
    struct sockaddr_in addr;
    int s, n;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, type, 0);
    if (s < 0)
        return -1;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

    if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0)
    {
        close(s);
        return -1;
    }

    if (type == SOCK_STREAM)
    {
        int ret;

        ret = listen(s, 1);
        LOGI("listen() returned %d", ret);

        if (ret < 0)
        {
            close(s);
            return -1;
        }
    }

    return s;
}

SensorsPacket__SensorAccelerometerPayload* readBody(int csock, uint32_t siz)
{
    int bytecount;

    SensorsPacket__SensorAccelerometerPayload* payloadsens;
    // protobuf_c_message_init(&sensors_packet__sensor_accelerometer_payload__descriptor,payloadsens);

    void* buffer = malloc(siz);
    // Read the entire buffer including the hdr
    if ((bytecount = recv(csock, buffer, siz, MSG_WAITALL)) == -1)
    {
        LOGI("Error receiving data %d", bytecount);
    }

    LOGI(" readBody --  Second read byte count is %d", bytecount);

    payloadsens = (SensorsPacket__SensorAccelerometerPayload*) protobuf_c_message_unpack(
        &sensors_packet__sensor_accelerometer_payload__descriptor, NULL, bytecount, buffer);

    LOGI(" readBody payloadsens->has_x %d", payloadsens->has_x);
    LOGI(" readBody payloadsens->has_y %d", payloadsens->has_y);
    LOGI(" readBody payloadsens->has_z %d", payloadsens->has_z);

    LOGI(" readBody payloadsens->x %f", payloadsens->x);
    LOGI(" readBody payloadsens->y %f", payloadsens->y);
    LOGI(" readBody payloadsens->z %f", payloadsens->z);

    return payloadsens;
}

void* mock_vm_recv_poll(void* args)
{
    int ret;
    int maxfd;
    fd_set readfs;
    struct timeval max_delay;

    max_delay.tv_sec = 0;
    max_delay.tv_usec = 100000;

    sensor_params_acc* params = (sensor_params_acc*) args;

    int serverSock = socket_inaddr_any_server(PORT_SENSORS, SOCK_STREAM);
    if (serverSock == SOCKET_ERROR || serverSock < 0 || serverSock > FD_SETSIZE)
    {
        LOGE("Unable to listen to %d", PORT_SENSORS);
        return 0;
    }
    int clientsSocks[MAX_CLIENTS] = {0};

    while (1)
    {
        maxfd = serverSock;

        FD_ZERO(&readfs);

        FD_SET(serverSock, &readfs);

        // Add connected sockets to the list of sockets to watch
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clientsSocks[i] > 0)
            {
                FD_SET(clientsSocks[i], &readfs);
                // make sure we stored the biggest fd
                if (clientsSocks[i] > maxfd)
                {
                    maxfd = clientsSocks[i];
                }
            }
        }

        ret = select(maxfd + 1, &readfs, NULL, NULL, &max_delay);
        // LOGI("select() returns %d", ret);

        if (ret < 0)
        {
            if (maxfd == serverSock)
            {
                close(serverSock);
                LOGI("Server closed connection, exiting. ");
                exit(EXIT_FAILURE);
            }
            else
            {
                // Something's wrong, disconnect every clients but keep server cnx
                for (int c = 0; c < MAX_CLIENTS; c++)
                {
                    if (clientsSocks[c] > 0)
                    {
                        close(clientsSocks[c]);
                        clientsSocks[c] = 0;
                    }
                }
                LOGI("Select fail, disconnect all clients ");
                // continue;
            }
        }

        if (FD_ISSET(serverSock, &readfs))
        {
            acceptNewClient(serverSock, clientsSocks);
        }

        params->nbevent = 0;
        for (int c = 0; c < MAX_CLIENTS; c++)
        {
            if (FD_ISSET(clientsSocks[c], &readfs))
            {
                params->payload = readBody(clientsSocks[c], params->toread);
                params->nbevent++;
                //                 events_read += readData(&clientsSocks[c],
                //                                       &data[events_read],/* first empty slot */
                //                                       count - events_read /* space left */);
            }
            if (params->nbevent)
            {
                // Stop events read, we have read enough
                LOGI("Stop events read, we have read enough %d %f %f %f", params->nbevent,
                     params->payload->x, params->payload->y, params->payload->z);
                pthread_exit(NULL);
                break;
            }
        }
        // if no client write data, we should lastRead
    }

    // return read events number
}
