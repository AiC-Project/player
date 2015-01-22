/**
 * \file amqp_send.h
 * \brief Send AMQP messages */
#ifndef __AMQP_SEND_H_
#define __AMQP_SEND_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include "config_env.h"

#include <assert.h>

#include "socket.h"

/**
 * \brief Send a payload to an AMQP exchange
 */
int amqp_send(char const* hostname, int port, char const* exchange, char const* bindingkey,
              int size, unsigned char* messagebody);

#endif
