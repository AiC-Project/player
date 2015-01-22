#include <errno.h>    // for errno, EINTR
#include <pthread.h>  // for pthread_mutex_lock, pthread_mutex_unlock
#include <stdint.h>   // for uint8_t
#include <stdio.h>    // for NULL
#include <stdlib.h>   // for free
#include <string.h>
#include <sys/select.h>  // for FD_ISSET, FD_SET, select, FD_ZERO, fd_set
#include <sys/socket.h>  // for MSG_WAITALL
#include <unistd.h>      // for close, usleep

#include "socket.h"
#include "logger.h"

#include "host_gl.h"

#define LOG_TAG "gl"

static pthread_mutex_t mtx;
static uint8_t start_conn_thread = 0;

static int copy_socket(int fd_read, int fd_write, char* buff)
{
    int rsize, towrite, wsize;

    rsize = read(fd_read, buff, BUFF_SIZE);
    if (rsize <= 0)
    {
        close(fd_read);
        close(fd_write);
        return -1;
    }

    towrite = rsize;

    while ((wsize = write(fd_write, buff, towrite)) > 0)
        towrite -= wsize;
    if (towrite > 0)
    {
        close(fd_read);
        close(fd_write);
        return -1;
    }

    return 0;
}

static void* conn_thread(void* arg)
{
    struct conn_duo* cd = (struct conn_duo*) arg;
    char* buff = (char*) calloc(BUFF_SIZE, sizeof(char));
    if (!buff)
        LOGE("Socket copy thread %u: Unable to alloc %d bytes", pthread_self(), BUFF_SIZE);

    while (1)
    {
        fd_set set_read;
        int nfds;

        FD_ZERO(&set_read);
        FD_SET(cd->local_socket, &set_read);
        FD_SET(cd->host_socket, &set_read);

        if (cd->local_socket > cd->host_socket)
            nfds = cd->local_socket + 1;
        else
            nfds = cd->host_socket + 1;

        if (select(nfds + 1, &set_read, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        if ((FD_ISSET(cd->local_socket, &set_read)) &&
            (copy_socket(cd->local_socket, cd->host_socket, buff) < 0))
            break;

        if ((FD_ISSET(cd->host_socket, &set_read)) &&
            (copy_socket(cd->host_socket, cd->local_socket, buff) < 0))
            break;
    }

    LOGI("Socket copy thread %u: Stopping", pthread_self());

    free(buff);
    close(cd->local_socket);
    close(cd->host_socket);
    free(cd);

    return NULL;
}

static void* sync_conn_thread(void* arg)
{
    socket_t main_socket = *((socket_t*) arg);

    int nop, written, cmd;

    do
    {
        recv(main_socket, &nop, sizeof(nop), MSG_WAITALL);
        switch (nop)
        {
        case 1:
            // 1: start a new copy_socket thread
            pthread_mutex_lock(&mtx);
            start_conn_thread = 1;
            pthread_mutex_unlock(&mtx);
            break;
        case OPENGL_PING:
            // PING: answer with a PONG
            cmd = OPENGL_PONG;
            written = write(main_socket, &cmd, sizeof(cmd));
            if (written != sizeof(cmd))
                LOGW("write() returned %d", written);
            break;
        default:
            break;
        }

        LOGD("sync thread: received %d ", nop);
    } while (1);

    return NULL;
}

int manage_socket_gl(void* arg)
{
    pthread_mutex_init(&mtx, NULL);
    char* vmip = arg;

    socket_t hw_socket;
    socket_t main_socket;

    int rc;
    pthread_t sync_thread_id;
    pthread_t new_thread_id;

    do
    {
        main_socket = open_socket_reuseaddr(vmip, 25000);

        if (main_socket == SOCKET_ERROR)
        {
            LOGW("connect() error: %s", strerror(errno));
            sleep(5);
        }
    } while (main_socket == SOCKET_ERROR);
    LOGI("OpenGL management connected to socket %d", main_socket);

    unsigned int cmd = OPENGL_START_COMMAND;
    if (write(main_socket, &cmd, sizeof(cmd)) == -1)
        LOGW("Unable to write data port to main connection - error %d (%s)", errno,
             strerror(errno));

    // Create the opengl socket monitoring thread
    rc = pthread_create(&sync_thread_id, NULL, sync_conn_thread, &main_socket);

    if (rc)
        LOGE("pthread_create returned %d", rc);

    while (1)
    {
        // connect for hw_socket connection
        do
        {
            hw_socket = open_socket_nodelay(vmip, 22468);

            if (hw_socket == SOCKET_ERROR)
            {
                LOGW("connect() error: %s", strerror(errno));
                sleep(5);
            }
        } while (hw_socket == SOCKET_ERROR);
        LOGI(" Connected to the VM with socket %d", hw_socket);

        socket_t render_socket = open_socket_nodelay("127.0.0.1", 22468);

        struct conn_duo* new_cd = (struct conn_duo*) malloc(sizeof(struct conn_duo));
        if (!new_cd)
        {
            LOGE("Cannot allocate memory");
            return 0;
        }

        new_cd->host_socket = hw_socket;
        new_cd->local_socket = render_socket;

        rc = pthread_create(&new_thread_id, NULL, conn_thread, (void*) new_cd);

        if (rc)
        {
            close(new_cd->local_socket);
            close(new_cd->host_socket);
            free(new_cd);
            LOGE("pthread_create returned %d", rc);
        }

        LOGI("New gl thread created");

        while (!start_conn_thread)
        {
            struct timespec duration = {0, 100000};
            nanosleep(&duration, NULL);
        }

        pthread_mutex_lock(&mtx);
        start_conn_thread = 0;
        pthread_mutex_unlock(&mtx);
    }

    return 0;
}
