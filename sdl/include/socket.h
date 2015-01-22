/**
 * \file socket.h
 * \brief Define socket utilities to simplify networking
 */
#ifndef __SOCKET_H_
#define __SOCKET_H_
#include <sys/socket.h>  // for recv, send

/** \brief Alias for the recv() return value in case of error */
#define SOCKET_ERROR -1

/** \brief Alias to differenciate between regular ints and socket fds */
typedef int socket_t;

/** \brief Connect to a host:port couple
 * \param ip IP address to connect to
 * \param port TCP port to connect to
 * \returns The open socket
 */
socket_t open_socket(const char* ip, short port);

/** \brief Open a socket with TCP_NODELAY */
socket_t open_socket_nodelay(const char* ip, short port);

/** \brief Open a socket with SO_REUSEADDR */
socket_t open_socket_reuseaddr(const char* ip, short port);
#endif
