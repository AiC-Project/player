/** \file protobuf_framing.c
 * \brief Provide utilities for sending protobufs
 */
#include <amqp.h>    // for amqp_envelope_t
#include <stdint.h>  // for uint8_t, uint32_t
#include <string.h>  // for bzero, memcpy

#include "protobuf_framing.h"

/**
 * Convert unsigned int to varint32
 */
uint32_t convert_framing_size(uint32_t len, uint8_t* bytes)
{
    uint32_t size = 0;
    uint32_t value = len;

    while (value > 0x7F)
    {
        bytes[size++] = (((uint8_t)(value)) & 0x7F) | 0x80;
        value >>= 7;
    }
    bytes[size++] = (uint8_t) value;

    return size;
}

/**
 * Write a protobuf, with a varint32 framing, and padding at the end
 */
int write_protobuf(socket_t sock, amqp_envelope_t* envelope)
{
    size_t amqp_size = envelope->message.body.len;
    uint32_t size = amqp_size + 4;
    uint8_t framing[4] = {0};
    uint8_t buf[size];
    memset(buf, 0, size);

    uint8_t size_framing = convert_framing_size(amqp_size, framing);
    memcpy(buf, framing, size_framing);
    memcpy(buf + size_framing, envelope->message.body.bytes, amqp_size);
    return send(sock, buf, size, 0);
}
