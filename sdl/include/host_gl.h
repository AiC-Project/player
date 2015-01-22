/**
 * \file host_gl.h
 * \brief OpenGL proxy management.
 */
#ifndef __HOST_GL_H_
#define __HOST_GL_H_

/** \brief Port used for initial communication */
#define MAIN_PORT 25000
/** \brief Port used for OpenGL marshalling transfer */
#define OPENGL_DATA_PORT 22468

/** \brief Data structure to hold the two connections */
struct conn_duo
{
    /** Socket connected to the render libs locally */
    int local_socket;
    /** Socket connected to the VM */
    int host_socket;
};

/** \brief Max size of opengl reads */
#define BUFF_SIZE (4 * 1024 * 1024)

/** \brief Command sent to initiate the remote graphics exchange */
#define OPENGL_START_COMMAND 1001
/** \brief Command sent by the VM to check if we’re still alive */
#define OPENGL_PING 1002
/** \brief Reply to make tell the VM we’re still alive */
#define OPENGL_PONG 1003

/** \brief Manage the remote OpenGL to the VM
 * \param arg Virtual Machine IP (char*)
 *
 * It first contacts the VM on port MAIN_PORT to establish
 * a connection to make sure everything is ready on the VM
 * side, then loops using the remote OpenGL protocol from
 * AOSP on the DATA_PORT, with proxies on both ends in
 * order to listen() on the VM side and not on the Player
 * side.
 */
int manage_socket_gl(void* arg);

#endif
