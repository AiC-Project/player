/** \file sdl_events.c
 * \brief Produce virtual input events from SDL events
 */
#include <pthread.h>
#include <sys/time.h>  // for timeval, gettimeofday
#include <time.h>      // for time_t
#include <linux/input.h>

#include "buffer_sizes.h"
#include "logger.h"
#include "grabber.h"
#include "sdl_events.h"
#include "sdl_translate.h"

#define LOG_TAG "sdl_events"

extern int g_width;
extern int g_height;
extern float g_rotation;

/** Fill a char array with a timestamp
 * The array is assumed big enough. */
static void grab_time(char* stringtime)
{
    char T[30];
    time_t curtime;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;

    struct tm cur_localtime;
    localtime_r(&curtime, &cur_localtime);
    strftime(T, 30, "%Y-%d-%mT%H:%M:%S", &cur_localtime);
    snprintf(stringtime, 30, "%s.%ld", T, tv.tv_usec);
}

/**
 * Convert the mouse position to the one it should have on the device
 */
static void convert_mouse_position(int* x, int* y)
{
    int tmp;

    switch ((int) g_rotation)
    {
    case 90:
        tmp = *x;
        *x = g_width - *y;
        *y = tmp;
        break;
    case 180:
        *x = g_width - *x;
        break;
    case 270:
        tmp = *x;
        *x = *y;
        *y = g_height - tmp;
        break;
    default:
        break;
    }
}

int sdl_key(SDL_Event* event, socket_t input_socket)
{
    static int moreFrames = 0;
    int xtkey, ret = 0;

    xtkey = sdl_translate_event(event->key.keysym.scancode, event->key.keysym.sym);
    LOGI("Got SDL_KEY with state=%d code=%d keysym=%d xtkey=%d input_socket=%d", event->key.state,
         event->key.keysym.scancode, event->key.keysym.sym, xtkey, input_socket);

    static struct thread_args grab_args;
    static pthread_t pgrab_Thread;
    char strTime[BUF_SIZE];
    if (xtkey == KEY_F7 && moreFrames == 0) /* F7, start recording */
    {
        char strPrefix[] = "log/video_F7_";
        memset(&grab_args, 0, sizeof(struct thread_args));
        grab_time(strTime);
        snprintf(grab_args.record_filename, BUF_SIZE, "%s%s.mp4", strPrefix, strTime);
        moreFrames = 1;
        pthread_mutex_init(&grab_args.mtx, NULL);
        pthread_mutex_lock(&grab_args.mtx);
        pthread_create(&pgrab_Thread, NULL, (void*) &ffmpeg_grabber, &grab_args);
    }
    else if (xtkey == KEY_F8 && moreFrames == 1) /* F8, stop recording */
    {
        moreFrames = 0;
        pthread_mutex_unlock(&grab_args.mtx);
        pthread_join(pgrab_Thread, NULL);
    }
    else if (event->type == SDL_KEYDOWN && xtkey == KEY_F6) /* F6, take screenshot */
    {
        char snap_filename[BUF_SIZE];
        grab_time(strTime);
        snprintf(snap_filename, sizeof(snap_filename), "log/snap_F6_%s.bmp", strTime);
        LOGI("Saving snapshot %s", snap_filename);
        grab_snapshot(snap_filename);
    }

    if (input_socket)
    {
        char buffer[BUF_SIZE];
        snprintf(buffer, BUF_SIZE, "%s:%d:%d\n",
                 (event->key.state == SDL_PRESSED) ? "KBDPR" : "KBDRL", xtkey, event->key.state);
        ret = send(input_socket, buffer, strlen(buffer), 0);
    }
    return ret;
}

int sdl_mouse_motion(SDL_Event* event, socket_t input_socket)
{
    int x = event->motion.x;
    int y = event->motion.y;
    convert_mouse_position(&x, &y);
    if (input_socket)
    {
        char buffer[BUF_SIZE];
        snprintf(buffer, BUF_SIZE, "MOUSE:%d:%d\n", x, y);
        return send(input_socket, buffer, strlen(buffer), 0);
    }
    return 0;
}

int sdl_mouse_wheel(SDL_Event* event, socket_t input_socket)
{
    char buffer[BUF_SIZE];
    int x, y;
    SDL_GetMouseState(&x, &y);
    convert_mouse_position(&x, &y);
    if (event->wheel.y > 0)
        snprintf(buffer, BUF_SIZE, "WHEEL:%d:%d:1:0\n", x, y);
    else if (event->wheel.y < 0)
        snprintf(buffer, BUF_SIZE, "WHEEL:%d:%d:-1:0\n", x, y);
    else
        return 0;
    return send(input_socket, buffer, strlen(buffer), 0);
}

int sdl_mouse_button(SDL_Event* event, socket_t input_socket)
{
    int x = event->motion.x;
    int y = event->motion.y;
    convert_mouse_position(&x, &y);
    if (input_socket)
    {
        char buffer[BUF_SIZE];
        switch (event->button.button)
        {
        case SDL_BUTTON_LEFT:
            if (event->button.state == SDL_PRESSED)
                snprintf(buffer, BUF_SIZE, "MSBPR:%d:%d\n", x, y);
            else
                snprintf(buffer, BUF_SIZE, "MSBRL:%d:%d\n", x, y);
            break;
        default:
            return 0;
        }
        return send(input_socket, buffer, strlen(buffer), 0);
    }
    return SOCKET_ERROR;
}
