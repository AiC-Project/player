/**
 * \file socket.c
 * \brief Utilities for opening sockets
 */
#include <errno.h>        // for errno
#include <netinet/in.h>   // for IPPROTO_TCP
#include <netdb.h>        // for addrinfo
#include <netinet/tcp.h>  // for TCP_NODELAY
#include <stdio.h>        // for NULL
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>  // for AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR

#include "socket.h"
#include "logger.h"

#define LOG_TAG "socket"

typedef enum open_type
{
    NODELAY,
    REUSEADDR,
    NONE
} open_type_t;

static socket_t open_socket_switch(const char* ip, short port, open_type_t type);

socket_t open_socket(const char* ip, short port)
{
    return open_socket_switch(ip, port, NONE);
}

socket_t open_socket_nodelay(const char* ip, short port)
{
    return open_socket_switch(ip, port, NODELAY);
}

socket_t open_socket_reuseaddr(const char* ip, short port)
{
    return open_socket_switch(ip, port, REUSEADDR);
}

static socket_t open_socket_switch(const char* ip, short port, open_type_t type)
{
    socket_t sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int yes = 1;
    char sport[6];

    snprintf(sport, 6, "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(ip, sport, &hints, &servinfo)) != 0)
    {
        LOGW("Error in getaddrinfo: %s\n", gai_strerror(rv));
        return SOCKET_ERROR;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            LOGW("Socket connect error: %s", strerror(errno));
            continue;
        }

        if (type == NODELAY)
            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));  // 2
        else if (REUSEADDR)
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));  // 3

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            LOGW("Socket connect error: %s", strerror(errno));
            close(sockfd);
            continue;
        }

        break;  // if we get here, we must have connected successfully
    }

    if (p == NULL)
    {
        // looped off the end of the list with no connection
        LOGW("Failed to connect to %s:%d", ip, port);
        return SOCKET_ERROR;
    }

    freeaddrinfo(servinfo);  // all done with this structure

    return sockfd;
}
