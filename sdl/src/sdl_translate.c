/**
 * \file sdl_translate.c
 * \brief SDL scancode to uinput
 */
#include <sdl_translate.h>
#include <linux/input.h>

#include "logger.h"
#define LOG_TAG "sdl_translate"

// clang-format off
/* Keep it consistent with linux/input.h */
typedef enum {
    kKeyCodeEscape                  = KEY_ESC,
    kKeyCodeHome                    = KEY_HOME,
    kKeyCodeBack                    = KEY_BACK,
    kKeyCodeCall                    = KEY_SEND,
    kKeyCodeEndCall                 = KEY_END,
    kKeyCode0                       = KEY_0,
    kKeyCode1                       = KEY_1,
    kKeyCode2                       = KEY_2,
    kKeyCode3                       = KEY_3,
    kKeyCode4                       = KEY_4,
    kKeyCode5                       = KEY_5,
    kKeyCode6                       = KEY_6,
    kKeyCode7                       = KEY_7,
    kKeyCode8                       = KEY_8,
    kKeyCode9                       = KEY_9,
    kKeyCodeStar                    = KEY_KPASTERISK,
    kKeyCodeDpadUp                  = KEY_UP,
    kKeyCodeDpadDown                = KEY_DOWN,
    kKeyCodeDpadLeft                = KEY_LEFT,
    kKeyCodeDpadRight               = KEY_RIGHT,
    kKeyCodeVolumeUp                = KEY_VOLUMEUP,
    kKeyCodeVolumeDown              = KEY_VOLUMEDOWN,
    kKeyCodePower                   = KEY_POWER,
    kKeyCodeCamera                  = KEY_CAMERA,
    kKeyCodeClear                   = KEY_CLEAR,
    kKeyCodePageUp                  = KEY_PAGEUP,
    kKeyCodePageDown                = KEY_PAGEDOWN,
    kKeyCodeA                       = KEY_A,
    kKeyCodeB                       = KEY_B,
    kKeyCodeC                       = KEY_C,
    kKeyCodeD                       = KEY_D,
    kKeyCodeE                       = KEY_E,
    kKeyCodeF                       = KEY_F,
    kKeyCodeG                       = KEY_G,
    kKeyCodeH                       = KEY_H,
    kKeyCodeI                       = KEY_I,
    kKeyCodeJ                       = KEY_J,
    kKeyCodeK                       = KEY_K,
    kKeyCodeL                       = KEY_L,
    kKeyCodeM                       = KEY_M,
    kKeyCodeN                       = KEY_N,
    kKeyCodeO                       = KEY_O,
    kKeyCodeP                       = KEY_P,
    kKeyCodeQ                       = KEY_Q,
    kKeyCodeR                       = KEY_R,
    kKeyCodeS                       = KEY_S,
    kKeyCodeT                       = KEY_T,
    kKeyCodeU                       = KEY_U,
    kKeyCodeV                       = KEY_V,
    kKeyCodeW                       = KEY_W,
    kKeyCodeX                       = KEY_X,
    kKeyCodeY                       = KEY_Y,
    kKeyCodeZ                       = KEY_Z,

    kKeyCodeComma                   = KEY_COMMA,
    kKeyCodePeriod                  = KEY_DOT,
    kKeyCodeAltLeft                 = KEY_LEFTALT,
    kKeyCodeAltRight                = KEY_RIGHTALT,
    kKeyCodeCapLeft                 = KEY_LEFTSHIFT,
    kKeyCodeCapRight                = KEY_RIGHTSHIFT,
    kKeyCodeTab                     = KEY_TAB,
    kKeyCodeSpace                   = KEY_SPACE,
    kKeyCodeSym                     = KEY_COMPOSE,
    kKeyCodeExplorer                = KEY_WWW,
    kKeyCodeEnvelope                = KEY_MAIL,
    kKeyCodeNewline                 = KEY_ENTER,
    kKeyCodeDel                     = KEY_BACKSPACE,
    kKeyCodeGrave                   = 399,
    kKeyCodeMinus                   = KEY_MINUS,
    kKeyCodeEquals                  = KEY_EQUAL,
    kKeyCodeLeftBracket             = KEY_LEFTBRACE,
    kKeyCodeRightBracket            = KEY_RIGHTBRACE,
    kKeyCodeBackslash               = KEY_BACKSLASH,
    kKeyCodeSemicolon               = KEY_SEMICOLON,
    kKeyCodeApostrophe              = KEY_APOSTROPHE,
    kKeyCodeSlash                   = KEY_SLASH,
    kKeyCodeAt                      = KEY_EMAIL,
    kKeyCodeSearch                  = KEY_SEARCH,
    kKeyCodePrevious                = KEY_PREVIOUS,
    kKeyCodeNext                    = KEY_NEXT,
    kKeyCodePlay                    = KEY_PLAY,
    kKeyCodePause                   = KEY_PAUSE,
    kKeyCodeStop                    = KEY_STOP,
    kKeyCodeRewind                  = KEY_REWIND,
    kKeyCodeFastForward             = KEY_FASTFORWARD,
    kKeyCodeBookmarks               = KEY_BOOKMARKS,
    kKeyCodeCycleWindows            = KEY_CYCLEWINDOWS,

} AndroidKeyCode;


int sdl_translate_event(SDL_Scancode scancode, SDL_Keycode keysym)
{
    (void) keysym;
    switch(scancode) {
    case SDL_SCANCODE_ESCAPE:     return kKeyCodeEscape;
    case SDL_SCANCODE_1:          return kKeyCode1;
    case SDL_SCANCODE_2:          return kKeyCode2;
    case SDL_SCANCODE_3:          return kKeyCode3;
    case SDL_SCANCODE_4:          return kKeyCode4;
    case SDL_SCANCODE_5:          return kKeyCode5;
    case SDL_SCANCODE_6:          return kKeyCode6;
    case SDL_SCANCODE_7:          return kKeyCode7;
    case SDL_SCANCODE_8:          return kKeyCode8;
    case SDL_SCANCODE_9:          return kKeyCode9;
    case SDL_SCANCODE_0:          return kKeyCode0;

    case SDL_SCANCODE_Q:          return kKeyCodeQ;
    case SDL_SCANCODE_W:          return kKeyCodeW;
    case SDL_SCANCODE_E:          return kKeyCodeE;
    case SDL_SCANCODE_R:          return kKeyCodeR;
    case SDL_SCANCODE_T:          return kKeyCodeT;
    case SDL_SCANCODE_Y:          return kKeyCodeY;
    case SDL_SCANCODE_U:          return kKeyCodeU;
    case SDL_SCANCODE_I:          return kKeyCodeI;
    case SDL_SCANCODE_O:          return kKeyCodeO;
    case SDL_SCANCODE_P:          return kKeyCodeP;
    case SDL_SCANCODE_A:          return kKeyCodeA;
    case SDL_SCANCODE_S:          return kKeyCodeS;
    case SDL_SCANCODE_D:          return kKeyCodeD;
    case SDL_SCANCODE_F:          return kKeyCodeF;
    case SDL_SCANCODE_G:          return kKeyCodeG;
    case SDL_SCANCODE_H:          return kKeyCodeH;
    case SDL_SCANCODE_J:          return kKeyCodeJ;
    case SDL_SCANCODE_K:          return kKeyCodeK;
    case SDL_SCANCODE_L:          return kKeyCodeL;
    case SDL_SCANCODE_Z:          return kKeyCodeZ;
    case SDL_SCANCODE_X:          return kKeyCodeX;
    case SDL_SCANCODE_C:          return kKeyCodeC;
    case SDL_SCANCODE_V:          return kKeyCodeV;
    case SDL_SCANCODE_B:          return kKeyCodeB;
    case SDL_SCANCODE_N:          return kKeyCodeN;
    case SDL_SCANCODE_M:          return kKeyCodeM;
    case SDL_SCANCODE_COMMA:      return kKeyCodeComma;
    case SDL_SCANCODE_PERIOD:     return kKeyCodePeriod;
    case SDL_SCANCODE_SPACE:      return kKeyCodeSpace;
    case SDL_SCANCODE_SLASH:      return kKeyCodeSlash;
    case SDL_SCANCODE_RETURN:     return kKeyCodeNewline;
    case SDL_SCANCODE_BACKSPACE:  return kKeyCodeDel;

/* these are qwerty keys not on a device keyboard  */
    case SDL_SCANCODE_TAB:        return kKeyCodeTab;
    case SDL_SCANCODE_GRAVE    :  return kKeyCodeGrave;
    case SDL_SCANCODE_MINUS:      return kKeyCodeMinus;
    case SDL_SCANCODE_EQUALS:     return kKeyCodeEquals;
    case SDL_SCANCODE_LEFTBRACKET: return kKeyCodeLeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return kKeyCodeRightBracket;
    case SDL_SCANCODE_BACKSLASH:  return kKeyCodeBackslash;
    case SDL_SCANCODE_SEMICOLON:  return kKeyCodeSemicolon;
    case SDL_SCANCODE_APOSTROPHE:      return kKeyCodeApostrophe;

    case SDL_SCANCODE_UP:         return kKeyCodeDpadUp;
    case SDL_SCANCODE_DOWN:       return kKeyCodeDpadDown;
    case SDL_SCANCODE_LEFT:       return kKeyCodeDpadLeft;
    case SDL_SCANCODE_RIGHT:      return kKeyCodeDpadRight;
    case SDL_SCANCODE_PAGEUP:     return kKeyCodePageUp;
    case SDL_SCANCODE_PAGEDOWN:   return kKeyCodePageDown;

    case SDL_SCANCODE_RSHIFT:     return kKeyCodeCapRight;
    case SDL_SCANCODE_LSHIFT:     return kKeyCodeCapLeft;
    case SDL_SCANCODE_RALT:       return kKeyCodeAltRight;
    case SDL_SCANCODE_LALT:       return kKeyCodeAltLeft;
    case SDL_SCANCODE_RCTRL:      return kKeyCodeSym;
    case SDL_SCANCODE_LCTRL:      return kKeyCodeSym;

    default:
        LOGW("unknown sdl scancode %d", scancode);
        return -1;
    }
}
// clang-format on
