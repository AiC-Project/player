/**
 * \file protobuf_framing.h
 * \brief Utility to convert framing to varint32 for google’s parser
 */
#ifndef __PROTOBUF_FRAMING_H_
#define __PROTOBUF_FRAMING_H_

#include <stdint.h>
#include <amqp.h>

#include "socket.h"

/**
 * \brief Convert an unsigned int to varint32 so that google’s C++ protobuf
 * libs can read it.
 *
 * \param len the length to convert to varint32
 * \param bytes a preallocated buffer to store the varint
 * \returns the number of bytes written in the buffer
 *
 *\p bytes must be preallocated with at list 4 elements.
 */
uint32_t convert_framing_size(uint32_t len, uint8_t* bytes);

/**
 * \brief Write a protobuf, with a varint32 framing, and padding at the end
 * from an envelope.
 *
 * \param sock the socket to use
 * \param envelope the envelope containing the bytes to write on the socket
 */
int write_protobuf(socket_t sock, amqp_envelope_t* envelope);

#endif
