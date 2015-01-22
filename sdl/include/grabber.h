/*!
 * \file grabber.h
 * \brief grabber records videos or snapshots from ampq messages
 * \author Alexandre BRUXELLE for ZENIKA
 *
 * The ffmpeg part the code is inspired by the libavformat API example.
 */

#ifndef __GRABBER_H_
#define __GRABBER_H_

#include <libavformat/avformat.h>  // for AVStream
#include <libavutil/frame.h>       // for AVFrame
#include <libswscale/swscale.h>    // for SWS_BICUBIC
#include <pthread.h>               // for pthread_mutex_t, pthread_cond_t
#include <stdint.h>                // for uint8_t
#include <X11/Xlib.h>
#include "buffer_sizes.h"          // for BUF_SIZE
#include "socket.h"                // for socket_t

/** \brief Port open on the VM */
#define PORT_GRAB 32500

/** \brief Frames to record per second */
#define STREAM_DURATION 60.0
/** \brief FPS of the stream */
#define STREAM_FRAME_RATE 60
/** \brief Pixel format of the stream (yuv420p) */
#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P

#define SCALE_FLAGS SWS_BICUBIC

//// ################################################################################
#define CLIP(X) ((X) > 255 ? 255 : (X) < 0 ? 0 : X)

// RGB -> YUV
#define RGB2Y(R, G, B) CLIP(((66 * (R) + 129 * (G) + 25 * (B) + 128) >> 8) + 16)
#define RGB2U(R, G, B) CLIP(((-38 * (R) -74 * (G) + 112 * (B) + 128) >> 8) + 128)
#define RGB2V(R, G, B) CLIP(((112 * (R) -94 * (G) -18 * (B) + 128) >> 8) + 128)

//// ################################################################################

/**
 * \brief Shared structure between recv thread and grabber thread
 */
typedef struct read_args
{
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    uint8_t* buffer;
    socket_t sock;
    int len;
    int flagSnapRec;
    int flagRecording;
} s_read_args;

/**
 * \brief Struct holding the state of a recording (filename/lock)
 */
typedef struct thread_args
{
    pthread_mutex_t mtx;
    char record_filename[BUF_SIZE];
} s_thread_args;

void grab_snapshot(char* snap_filename);

/** \brief A wrapper around a single output AVStream */
typedef struct OutputStream
{
    AVStream* st;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame* frame;
    AVFrame* tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext* sws_ctx;
    struct SwrContext* swr_ctx;
} OutputStream;

/**
    \brief Function passed to a thread in order to detect format encoding - encode frame- and write
   in a file
    \param args parameter for file's name of the video record
*/
int ffmpeg_grabber(void* arg);

unsigned char* xgrabber();

/**
    \brief Start/Stop from socket connection via protobuf message
    \param data->flagRecording to protect of recording from amqp messages
*/
void* grab_handler_sock();

void* grab_handler_amqp(void* args);

/** \brief Set the static path to the screenshots/movies dir
 * \param results a pointer to the string
 */
void grabber_set_path_results(char* results);

/** \brief Set the static X display pointer
 * \param display the X display pointer
 */
void grabber_set_display(Display* display);
#endif
