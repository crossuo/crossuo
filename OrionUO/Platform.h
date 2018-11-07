// GPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#pragma once

#include <string>
#include <stdio.h>
#include <SDL_events.h>

#if USE_WISP

struct UserEvent
{
    uint32_t code;
    WPARAM data1;
    LPARAM data2;
};

struct PaintEvent
{
    WPARAM wParam;
    LPARAM lParam;
};

struct InputEvent
{
    WPARAM wParam;
    LPARAM lParam;
};

typedef InputEvent KeyEvent;
typedef InputEvent TextEvent;

#define Keycode WPARAM
#define TextChar WPARAM
#define EvKey(x) ((Keycode)x.wParam)
#define EvChar(x) ((wchar_t)x.wParam)
#define IsPrintable(x) iswprint(x)
#define AsKeyEvent(x) (static_cast<const KeyEvent>(x))

#define KEY_SHIFT VK_SHIFT
#define KEY_CONTROL VK_CONTROL
#define KEY_CAPITAL VK_CAPITAL

#define KEY_RETURN VK_RETURN
#define KEY_RETURN2 VK_RETURN
#define KEY_ESCAPE VK_ESCAPE
#define KEY_TAB VK_TAB
#define KEY_MENU VK_MENU
#define KEY_RMENU VK_RMENU
#define KEY_LEFT VK_LEFT
#define KEY_RIGHT VK_RIGHT
#define KEY_DOWN VK_DOWN
#define KEY_UP VK_UP
#define KEY_END VK_END
#define KEY_HOME VK_HOME
#define KEY_PAGEDOWN VK_NEXT
#define KEY_PAGEUP VK_PRIOR
#define KEY_DELETE VK_DELETE
#define KEY_PAUSE VK_PAUSE
#define KEY_SPACE VK_SPACE
#define KEY_BACK VK_BACK
#define KEY_SCROLL VK_SCROLL
#define KEY_PRINTSCREEN 0x2C

#define KEY_F1 VK_F1
#define KEY_F2 VK_F2
#define KEY_F3 VK_F3
#define KEY_F4 VK_F4
#define KEY_F5 VK_F5
#define KEY_F6 VK_F6
#define KEY_F7 VK_F7
#define KEY_F8 VK_F8
#define KEY_F9 VK_F9
#define KEY_F10 VK_F10
#define KEY_F11 VK_F11
#define KEY_F12 VK_F12

#define KEY_NUMPAD0 VK_NUMPAD0
#define KEY_NUMPAD1 VK_NUMPAD1
#define KEY_NUMPAD2 VK_NUMPAD2
#define KEY_NUMPAD3 VK_NUMPAD3
#define KEY_NUMPAD4 VK_NUMPAD4
#define KEY_NUMPAD5 VK_NUMPAD5
#define KEY_NUMPAD6 VK_NUMPAD6
#define KEY_NUMPAD7 VK_NUMPAD7
#define KEY_NUMPAD8 VK_NUMPAD8
#define KEY_NUMPAD9 VK_NUMPAD9

typedef CRITICAL_SECTION ProtectedSection;
#define LOCK(x) EnterCriticalSection(&x)
#define UNLOCK(x) LeaveCriticalSection(&x)
#define CREATE_MUTEX(x) InitializeCriticalSection(&x);
#define RELEASE_MUTEX(x) DeleteCriticalSection(&x);

#else

#include <SDL_keycode.h>

typedef SDL_KeyboardEvent KeyEvent;
typedef SDL_TextInputEvent TextEvent;
typedef SDL_Event PaintEvent;
typedef SDL_UserEvent UserEvent;
#define Keycode SDL_Keycode
#define TextChar char *
#define EvKey(x) ((Keycode)x.keysym.sym)
#define EvChar(x) (x.text[0])
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

#define KEY_RETURN SDLK_RETURN
#define KEY_RETURN2 SDLK_RETURN2
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

#endif