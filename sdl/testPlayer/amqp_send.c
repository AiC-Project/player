#include <stdio.h>
#include <amqp.h>
#include <unistd.h>
#include "amqp_send.h"
#include "logger.h"

#define LOG_TAG "amqp_send"

static int amqp_connect(const char* hostname, int port, amqp_connection_state_t* conn,
                        const unsigned int tries)
{
/** \brief Macro to retry connecting to the AMQP server */
#define RETRY                                                                                      \
    do                                                                                             \
    {                                                                                              \
        sleep(backoff);                                                                            \
        backoff *= 2;                                                                              \
        current_try++;                                                                             \
    } while (0)
    int status;
    uint8_t success = 0;
    unsigned int current_try = 0;
    unsigned int backoff = 1;
    amqp_socket_t* socket = NULL;
    amqp_rpc_reply_t reply;

    *conn = amqp_new_connection();

    while (current_try < tries && !success)
    {
        socket = amqp_tcp_socket_new(*conn);
        status = amqp_socket_open(socket, hostname, port);

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
        success = 1;
    }
    if (!success)
        LOGE("Could not login to AMQP after %d tries, quitting...", current_try);
    return 0;
#undef RETRY
}

int amqp_send(char const* hostname, int port, char const* exchange, char const* bindingkey,
              int size, unsigned char* messagebody)
{
    amqp_bytes_t messageByte;
    amqp_basic_properties_t props;
    messageByte.len = size;  // sizeof(messagebody);
    messageByte.bytes = messagebody;

    amqp_connection_state_t conn;
    amqp_connect(hostname, port, &conn, 3);

    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("text/text");
    props.delivery_mode = 2;  // persistent delivery mode
    props.content_encoding = amqp_cstring_bytes("UTF-8");

    amqp_exchange_declare(conn, 1, amqp_cstring_bytes(exchange), amqp_cstring_bytes("fanout"), 0, 0,
                          0, 0, amqp_empty_table);

    amqp_queue_declare_ok_t* r =
        amqp_queue_declare(conn, 1, amqp_cstring_bytes(bindingkey), 0, 1, 0, 0, amqp_empty_table);
    amqp_get_rpc_reply(conn);
    if (&(r->queue) == NULL)
    {
        LOGE("Unable to declare the queue");
        return 0;
    }
    amqp_bytes_t queuename = amqp_bytes_malloc_dup(r->queue);
    if (queuename.bytes == NULL)
        LOGE("Out of memory");

    amqp_queue_bind(conn, 1, queuename, amqp_cstring_bytes(exchange),
                    amqp_cstring_bytes(bindingkey), amqp_empty_table);

    amqp_basic_publish(conn, 1, amqp_cstring_bytes(exchange), queuename, 0, 0, &props, messageByte);
    amqp_destroy_connection(conn);
    return 0;
}
