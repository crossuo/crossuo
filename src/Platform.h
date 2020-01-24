// AGPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#pragma once

#include <wctype.h> // REMOVE
#include <assert.h> // REMOVE
#include <common/str.h>
#include <stdio.h>
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_thread.h>

#if _WIN32

#ifndef HAVE_M_PI
#define HAVE_M_PI // SDL conflicting with winsdk10
#endif

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#if !defined(_CRT_NON_CONFORMING_SWPRINTFS)
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "Glu32.lib")

#else
typedef void *HDC;
typedef void *HGLRC;

// Plugin Event
#define WM_MOUSEWHEEL 1
#define WM_MBUTTONUP 2
#define WM_MBUTTONDOWN 3
#define WM_CLOSE 10
#define WM_XBUTTONDOWN 11
#define WM_SETTEXT 13
#define WM_SHOWWINDOW 14
#define WM_NCACTIVATE 15

#define strncpy_s strncpy
#define lstrlenW wcslen
#define sprintf_s sprintf
#define sscanf_s sscanf
#define vsprintf_s vsprintf
#define vswprintf_s(a, b, c) vswprintf(a, 0, b, c)
#endif

// ^Need cleanup everything inside this block

typedef SDL_KeyboardEvent KeyEvent;
typedef SDL_TextInputEvent TextEvent;
typedef SDL_Event PaintEvent;
typedef SDL_UserEvent UserEvent;
#define Keycode SDL_Keycode
#define TextChar char *
#define EvKey(x) ((Keycode)x.keysym.sym)
#define EvChar(x) (wstr_from_utf8_input(x.text)[0])
#define IsPrintable(x) iswprint(x)

inline KeyEvent AsKeyEvent(const TextEvent &ev)
{
    const auto ch = EvChar(ev);
    assert(IsPrintable(ch));
    KeyEvent e;
    e.keysym.sym = ch;
    return e;
}

#define KEY_SHIFT (SDLK_LSHIFT | SDLK_RSHIFT)
#define KEY_CONTROL (SDLK_LCTRL | SDLK_RCTRL)
#define KEY_CAPITAL SDLK_CAPSLOCK

#define KEY_CLEAR SDLK_CLEAR
#define KEY_RETURN SDLK_RETURN
#define KEY_RETURN2 SDLK_KP_ENTER
#define KEY_ESCAPE SDLK_ESCAPE
#define KEY_TAB SDLK_TAB
#define KEY_MENU SDLK_MENU
#define KEY_RMENU SDLK_RMENU
#define KEY_LEFT SDLK_LEFT
#define KEY_RIGHT SDLK_RIGHT
#define KEY_DOWN SDLK_DOWN
#define KEY_UP SDLK_UP
#define KEY_END SDLK_END
#define KEY_HOME SDLK_HOME
#define KEY_PAGEDOWN SDLK_PAGEDOWN
#define KEY_PAGEUP SDLK_PAGEUP
#define KEY_DELETE SDLK_DELETE
#define KEY_PAUSE SDLK_PAUSE
#define KEY_SPACE SDLK_SPACE
#define KEY_BACK SDLK_BACKSPACE
#define KEY_SCROLL SDLK_SCROLLLOCK
#define KEY_PRINTSCREEN SDLK_PRINTSCREEN

#define KEY_F1 SDLK_F1
#define KEY_F2 SDLK_F2
#define KEY_F3 SDLK_F3
#define KEY_F4 SDLK_F4
#define KEY_F5 SDLK_F5
#define KEY_F6 SDLK_F6
#define KEY_F7 SDLK_F7
#define KEY_F8 SDLK_F8
#define KEY_F9 SDLK_F9
#define KEY_F10 SDLK_F10
#define KEY_F11 SDLK_F11
#define KEY_F12 SDLK_F12

#define KEY_NUMPAD0 SDLK_KP_0
#define KEY_NUMPAD1 SDLK_KP_1
#define KEY_NUMPAD2 SDLK_KP_2
#define KEY_NUMPAD3 SDLK_KP_3
#define KEY_NUMPAD4 SDLK_KP_4
#define KEY_NUMPAD5 SDLK_KP_5
#define KEY_NUMPAD6 SDLK_KP_6
#define KEY_NUMPAD7 SDLK_KP_7
#define KEY_NUMPAD8 SDLK_KP_8
#define KEY_NUMPAD9 SDLK_KP_9

typedef SDL_mutex *ProtectedSection;
#define LOCK(x) SDL_LockMutex(x)
#define UNLOCK(x) SDL_UnlockMutex(x)
#define CREATE_MUTEX(x) (x = SDL_CreateMutex())
#define RELEASE_MUTEX(x)                                                                           \
    if (!x)                                                                                        \
        SDL_DestroyMutex(x);                                                                       \
    x = nullptr;

namespace Platform
{
void SetLanguageFromSystemLocale();
void OpenBrowser(const astr_t &url);
} // namespace Platform
