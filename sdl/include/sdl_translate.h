/**
 * \file sdl_translate.h
 * \brief Convert sdl events to uinput codes
 *
 * The uinput codes will be read by the input daemon on the virtual
 * machine side.
 */
#ifndef __SDL_TRANSLATE_H__
#define __SDL_TRANSLATE_H__

#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>

/** \brief Convert an SDL scancode to a value read by the virtual input daemon
 */
int sdl_translate_event(SDL_Scancode scancode, SDL_Keycode keysym);

#endif
