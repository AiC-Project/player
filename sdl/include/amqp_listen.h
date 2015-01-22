/**
 * \file amqp_listen.h
 * \brief Utilities for consuming RabbitMQ messages.
 */
#ifndef __AMQP_LISTEN_H_
#define __AMQP_LISTEN_H_

#include <amqp.h>

/** \brief Setup a consumer for a specific queue.
    \param hostname The host of the RabbitMQ server
    \param port The port of the RabbitMQ server
    \param bindingkey the queue
    \param conn the connection object to initialize
    \param tries the number of tries

 * This function will try to connect \p tries number of times, with a
 * doubled delay everytimes something fails in the setup. In case of failure,
 * it will log an error and terminate the program.
 */
int amqp_listen_retry(const char* hostname, int port, const char* bindingkey,
                      amqp_connection_state_t* conn, const unsigned int tries);

/** \brief Consume one message from a connection object.
 * \param conn The connection object to use
 * \param envelope a preallocated envelope to store the message
 * \returns -1 on failure
 * \returns 0 on success
 */
int amqp_consume(amqp_connection_state_t* conn, amqp_envelope_t* envelope);

#endif
