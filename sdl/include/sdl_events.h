/**
 * \file sdl_events.h
 * \brief Produce virtual input events from SDL events
 *
 * The strings will be parsed by the daemon on the VM side and
 * dispatched as relevant uinput events.
 */
#ifndef __SDL_EVENTS_H__
#define __SDL_EVENTS_H__

#include <SDL2/SDL_events.h>
#include "socket.h"

/** \brief Produce a mouse motion event to the virtual input */
int sdl_mouse_motion(SDL_Event* event, socket_t input_socket);

/** \brief Produce a mouse wheel event to the virtual input */
int sdl_mouse_wheel(SDL_Event* event, socket_t input_socket);

/** \brief Produce a mouse click event to the virtual input */
int sdl_mouse_button(SDL_Event* event, socket_t input_socket);

/** \brief Produce a key press/release event to the virtual input */
int sdl_key(SDL_Event* event, socket_t input_socket);

#endif
