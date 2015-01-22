/*! \mainpage AiCPlayer is the interface between the AiC VM and the message broker
\section my-intro Introduction
* The AiC-Player has to:
* - Render the screen output,
* - Send keyboard and mouse events,
* - Catch rotation events from the AiC VM
* - Stimulate the sensors,
* - Record videos and snapshots of the output,
* - Grab audio from vm and redirect to a local ffserver
*
* The AiC Player receives AMQP messages and forwards data to the AiC VM
* through TCP connections.
*
* Beware that this is the architecture for local developpements;
* the complete architecture uses OpenStack and http services to manage
* and orchestrate a fleet of virtual machines and containers.
\image html Local_Dev_Architecture.png "Architecture for local developpements"
*/

/*!
 * \file main.c
 * \brief Main loop and graphical rendering
 * \author Alexandre BRUXELLE for ZENIKA
 *
 * The main will start the opengl proxy thread, initialize the AOSP rendering
 * libs on the port 22468, run the SDL input translator that connects to the
 * VM on port 22469 that will send raw uinput events; and finally runs the sdl
 * event loop.
 *
 * Libs required to run the OpenGL remote rendering:
 * - lib64OpenglRender.so
 * - lib64GLES_V2_translator.so
 * - lib64GLES_CM_translator.so
 * - lib64EGL_translator.so.
 *
 *  They can be obtained by building the sdk in the AOSP tree with our specific
 *  patch to add a callback function in case of screen rotation.
 */
#include <SDL2/SDL.h>           // for SDL_Init, SDL_INIT_VIDEO, SDL_Quit
#include <SDL2/SDL_error.h>     // for SDL_GetError
#include <SDL2/SDL_events.h>    // for SDL_Event, SDL_UserEvent, SDL_MouseMo..
#include <SDL2/SDL_keyboard.h>  // for SDL_Keysym
#include <SDL2/SDL_mouse.h>     // for SDL_GetMouseState, SDL_BUTTON_LEFT
#include <SDL2/SDL_render.h>    // for SDL_CreateRenderer, SDL_RenderClear
#include <SDL2/SDL_surface.h>   // for SDL_CreateRGBSurface, SDL_FreeSurface
#include <SDL2/SDL_syswm.h>     // for SDL_SysWMinfo, SDL_GetWindowWMInfo
#include <SDL2/SDL_version.h>   // for SDL_VERSION
#include <SDL2/SDL_video.h>     // for SDL_Window, SDL_SetWindowSize, SDL_WI..
#include <X11/Xlib.h>           // for XInitThreads, XOpenDisplay, Display
#include <pthread.h>            // for pthread_t
#include <signal.h>             // for signal, SIGPIPE, SIGSEGV, SIG_IGN
#include <stdint.h>             // for int32_t
#include <stdio.h>              // for NULL, snprintf
#include <stdlib.h>             // for atexit, exit, free, malloc
#include <string.h>             // for strncmp
#include <unistd.h>             // for close

#include "buffer_sizes.h"
#include "config_env.h"
#include "dump_trace.h"
#include "grabber.h"
#include "host_gl.h"
#include "logger.h"
#include "render_api.h"
#include "sdl_events.h"
#include "sensors.h"
#include "socket.h"

#define LOG_TAG "main"
/** Custom SDL Event: Rotation event from the VM */
#define USER_EVENT_ROTATION 1
/** Custom SDL Event: new input daemon connection */
#define USER_EVENT_NEWCLIENT 2

/** Port used by the AiC VM for virtual input */
#define INPUT_PORT 22469

static SDL_Surface* s_window_surface = NULL;
static SDL_Window* s_window = NULL;
static char* s_vmip = NULL;
static int input_in_progress = 0;

/** Height of the window (can be resized at runtime)
 *
 * Shared with the grabber.
 */
int g_height;

/** Width of the window (can be resized at runtime)
 *
 * Shared with the grabber.
 */
int g_width;

/** Rotation angle of the VM
 *
 * Shared width sdl events
 *
 */
float g_rotation = 0.0;

/** X Window ID */
void* g_window_id = NULL;

/** Create the SDL2 window and rendering surface */
static SDL_Window* open_window(int width, int height)
{
    char title[BUF_SIZE];
    SDL_Window* window;
    SDL_Renderer* renderer;

    if (SDL_Init(SDL_INIT_VIDEO))
        LOGE("SDL_Init failed: %s\n", SDL_GetError());

    atexit(SDL_Quit);
    snprintf(title, sizeof(title), "AiC Player (%i * %i)", width, height);
    window =
        SDL_CreateWindow(title, 0, 0, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        LOGE("Can't create window");

    grabber_set_display(XOpenDisplay(NULL));
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == NULL)
        LOGE("Can't create renderer");

    s_window_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0, 0, 0, 0);
    SDL_SetRenderDrawColor(renderer, 3, 169, 244, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return window;
}

/** Get the X window ID from SDL */
static void* get_window_id(SDL_Window* window)
{
    SDL_SysWMinfo wminfo;
    void* winhandle;

    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(window, &wminfo);
    winhandle = (void*) wminfo.info.x11.window;
    return winhandle;
}

/** Create a new rotation event and push it to the event loop */
static void callback_rotation(float angle)
{
    float* angle_alloc = (float*) malloc(sizeof(float));
    if (!angle_alloc)
        LOGE("callback_rotation(): out of memory");
    SDL_Event rotation_event;
    rotation_event.type = SDL_USEREVENT;
    rotation_event.user.code = USER_EVENT_ROTATION;
    LOGI("Rotation callback from the VM: %f ", angle);
    *angle_alloc = angle;
    rotation_event.user.data2 = angle_alloc;
    SDL_PushEvent(&rotation_event);
}

/** Rotate the native window */
static void recreate_subwindow(int width, int height, float rotation)
{
    setOpenGLDisplayRotation(rotation);
    destroyOpenGLSubwindow();
    SDL_FreeSurface(s_window_surface);
    s_window_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0, 0, 0, 0);
    if (!s_window_surface)
        LOGE("Unable to recreate Window surface");

    g_window_id = get_window_id(s_window);
    createOpenGLSubwindow(g_window_id, 0, 0, width, height, rotation);
    SDL_SetWindowSize(s_window, width, height);
}

/** Rotate the window to the appropriate angle */
static void do_rotation(float rotation)
{
    LOGI("Rotating the screen to: %f°", rotation);
    switch ((int) rotation)
    {
    case 0:
    case 180:
        recreate_subwindow(g_width, g_height, rotation);
        break;
    case 90:
    case 270:
        recreate_subwindow(g_height, g_width, rotation);
        break;
    default:
        LOGE("Unknown rotation value: %f°", rotation);
        break;
    }

    repaintOpenGLDisplay();
    g_rotation = rotation;
}

static void rotation_received(float rotation)
{
    if (rotation == 0.0 || rotation == 90.0 || rotation == 180.0 || rotation == 270.0)
        do_rotation(rotation);
    else
        LOGW("Unexpected rotation value: %f°", rotation);
}

/** Connect to the uinput daemon on the AiC VM */
static void* connect_input(void* arg)
{
    (void) arg;
    input_in_progress = 1;

    socket_t* input_socket = (socket_t*) malloc(sizeof(socket_t));
    if (!input_socket)
        LOGE("connect_input thread: out of memory");
    do
    {
        *input_socket = open_socket(s_vmip, INPUT_PORT);
        if (*input_socket == SOCKET_ERROR)
        {
            LOGW("Could not connect to input daemon (:%d): %s", INPUT_PORT, strerror(errno));
            sleep(1);
        }
    } while (*input_socket == SOCKET_ERROR);
    LOGI("input client connected with socket %d", *input_socket);

    SDL_Event conn_event;
    conn_event.type = SDL_USEREVENT;
    conn_event.user.code = USER_EVENT_NEWCLIENT;
    conn_event.user.data1 = input_socket;
    SDL_PushEvent(&conn_event);
    input_in_progress = 0;

    return 0;
}

/** SDL event loop */
static void main_loop(int width, int height)
{
    SDL_Event event;
    socket_t input_socket = 0;
    pthread_t input_thread;

reconnect:
    if (input_socket > 0)
    {
        close(input_socket);
        input_socket = 0;
        if (!input_in_progress)
        {
            if (pthread_create(&input_thread, NULL, connect_input, NULL) != 0)
                LOGE("Unable to start input thread, exiting...");
        }
    }

    while (SDL_WaitEvent(&event))
    {
        switch (event.type)
        {
        case SDL_USEREVENT:
            switch (event.user.code)
            {
            case USER_EVENT_ROTATION:
                rotation_received(*(float*) event.user.data2);
                free(event.user.data2);
                break;
            case USER_EVENT_NEWCLIENT:
                if (input_socket)
                    close(input_socket);
                char buffer[BUF_SIZE];
                input_socket = *(socket_t*) event.user.data1;
                free(event.user.data1);
                LOGI("Got new uinput client, socket=%d", input_socket);
                snprintf(buffer, BUF_SIZE, "CONFIG:%d:%d\n", width, height);
                send(input_socket, buffer, strlen(buffer), 0);
                break;
            }
            break;
        case SDL_MOUSEMOTION:
            if (sdl_mouse_motion(&event, input_socket) == SOCKET_ERROR)
                goto reconnect;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (sdl_mouse_button(&event, input_socket) == SOCKET_ERROR)
                goto reconnect;
            break;
        case SDL_MOUSEWHEEL:
            if (sdl_mouse_wheel(&event, input_socket) == SOCKET_ERROR)
                goto reconnect;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (sdl_key(&event, input_socket) == SOCKET_ERROR)
                goto reconnect;
            break;
        case SDL_QUIT:
            exit(0);
            break;
        }
    }
}

int main()
{
    init_logger();

    // env parameters
    char* amqp_host;
    char* path_results;
    char* vm_id;
    int dpi;
    int enable_record;
    int height;
    int width;

    static char port_gl[] = "22468";

    pthread_t gl_thread;
    pthread_t input_thread;
    pthread_t amqp_grabber_thread;
    pthread_t socket_grabber_thread;

    av_register_all();

    // print stack trace upon segmentation fault

    signal(SIGSEGV, dump_trace);
    signal(SIGPIPE, SIG_IGN);

    // All config values are mandatory, no defaults.

    amqp_host = configvar_string("AIC_PLAYER_AMQP_HOST");
    vm_id = configvar_string("AIC_PLAYER_VM_ID");
    s_vmip = configvar_string("AIC_PLAYER_VM_HOST");
    width = configvar_int("AIC_PLAYER_WIDTH");
    height = configvar_int("AIC_PLAYER_HEIGHT");
    enable_record = configvar_bool("AIC_PLAYER_ENABLE_RECORD");
    path_results = configvar_string("AIC_PLAYER_PATH_RECORD");
    dpi = configvar_int("AIC_PLAYER_DPI");
    g_width = width;
    g_height = height;

    grabber_set_path_results(path_results);

    XInitThreads();

    LOGI("Creating window surface: %dx%d", width, height);
    s_window = open_window(width, height);
    g_window_id = get_window_id(s_window);

    if (!initLibrary())
        LOGE("Unable to initialize Library");

    if (!setStreamMode(STREAM_MODE_TCP))
        LOGE("invalid stream mode for setStreamMode()");

    if (!initOpenGLRenderer(width, height, port_gl, 6))
        LOGE("initOpenGLRenderer failed");

    if (pthread_create(&gl_thread, NULL, (void*) &manage_socket_gl, s_vmip))
        LOGE("Error creating thread");

    if (!createOpenGLSubwindow(g_window_id, 0, 0, width, height, 0))
        LOGE("Unable to setup SubWindow");

    AiC_CallbackRotation(callback_rotation);
    AiC_setDPI(dpi);

    sensor_params param_listener;
    if (enable_record)
    {
        char sensor_name[] = "recording";

        const int32_t str_length = BUF_SIZE;
        g_strlcpy(param_listener.sensor, sensor_name, str_length);
        param_listener.gvmip = s_vmip;
        g_strlcpy(param_listener.exchange, sensor_name, str_length);

        snprintf(param_listener.queue, str_length, "android-events.%s.%s", vm_id, sensor_name);

        if (strncmp(amqp_host, "0", 1))
            pthread_create(&amqp_grabber_thread, 0, &grab_handler_amqp, &param_listener);
        else
            LOGI("Working without AMQP");

        pthread_create(&socket_grabber_thread, 0, &grab_handler_sock, &param_listener);
    }

    if (pthread_create(&input_thread, NULL, connect_input, NULL) != 0)
        LOGE("Unable to start input thread");

    main_loop(width, height);

    return 0;
}
