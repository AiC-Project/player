/** \file amqp_listen.c
 * \brief Listen on AMQP queues and consume messages */
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "logger.h"
#include "config_env.h"
#include "amqp_listen.h"

#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>

#define LOG_TAG "amqp_listen"

int amqp_listen_retry(const char* hostname, int port, const char* bindingkey,
                      amqp_connection_state_t* conn, const unsigned int tries)
{
/* Macro to retry connecting to the AMQP server */
#define RETRY                                                                                      \
    do                                                                                             \
    {                                                                                              \
        sleep(backoff);                                                                            \
        backoff *= 2;                                                                              \
        current_try++;                                                                             \
    } while (0)
    uint8_t success = 0;
    unsigned int current_try = 0;
    unsigned int backoff = 1;
    amqp_rpc_reply_t reply;

    *conn = amqp_new_connection();

    while (current_try < tries && !success)
    {
        amqp_socket_t* socket = amqp_tcp_socket_new(*conn);
        int status = amqp_socket_open(socket, hostname, port);

        if (status)
        {
            LOGC("AMQP error opening socket: %s", (char*) amqp_error_string2(status));
            RETRY;
            continue;
        }

        reply = amqp_login(*conn, "/", AMQP_DEFAULT_MAX_CHANNELS, AMQP_DEFAULT_FRAME_SIZE,
                           AMQP_DEFAULT_HEARTBEAT, AMQP_SASL_METHOD_PLAIN,
                           configvar_string("AIC_PLAYER_AMQP_USERNAME"),
                           configvar_string("AIC_PLAYER_AMQP_PASSWORD"));

        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            LOGC("AMQP login error");
            RETRY;
            continue;
        }

        amqp_channel_open(*conn, 1);
        reply = amqp_get_rpc_reply(*conn);

        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            LOGC("AMQP Channel error");
            amqp_channel_close(*conn, 1, AMQP_REPLY_SUCCESS);
            RETRY;
            continue;
        }

        amqp_basic_consume(*conn, 1, amqp_cstring_bytes(bindingkey), amqp_empty_bytes, 0, 0, 0,
                           amqp_empty_table);

        reply = amqp_get_rpc_reply(*conn);

        if (reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            LOGC("AMQP consume error");
            amqp_channel_close(*conn, 1, AMQP_REPLY_SUCCESS);
            RETRY;
            continue;
        }
        success = 1;
    }
    if (!success)
        LOGE("Could not login to AMQP after %d tries, quitting...", current_try);
    return 0;
#undef RETRY
}

int amqp_consume(amqp_connection_state_t* conn, amqp_envelope_t* envelope)
{
    amqp_rpc_reply_t res;
    amqp_maybe_release_buffers(*conn);
    res = amqp_consume_message(*conn, envelope, NULL, 0);

    if (AMQP_RESPONSE_NORMAL != res.reply_type)
    {
        return -1;
    }
    return 0;
}
