// GPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#pragma once

#if !defined(ORION_WINDOWS)

#include <unistd.h>

#include <chrono>
#include <thread>

#define NO_SDL_GLEXT
#include <GL/glew.h>
#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <zlib.h>

using namespace std;

#if defined(__clang__)
// Enable these incrementally to cleanup bad code
#pragma clang diagnostic ignored "-Wint-to-pointer-cast" // FIXME: CGLTextTexture
#pragma clang diagnostic ignored                                                                   \
    "-Wtautological-constant-out-of-range-compare" // FIXME: always true expression
#pragma clang diagnostic ignored                                                                   \
    "-Winconsistent-missing-override" // FIXME: OnTextInput, OnKeyDown, OnLeftMouse... etc.
#pragma clang diagnostic ignored                                                                   \
    "-Woverloaded-virtual" // FIXME: CGameItem::GetLightID, CTextContainer::Add, CJournal::Add etc.
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wshadow"  // FIXME: shadowing local variables
#pragma clang diagnostic ignored "-Wreorder" // FIXME: Initialization order in class fields
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wformat"               // %li
#pragma clang diagnostic ignored "-Wmultichar"            // FIXME: 'ENU'
#pragma clang diagnostic ignored "-Wchar-subscripts"      // FIXME: [' ']
#pragma clang diagnostic ignored "-Wunused-private-field" // FIXME: m_FakeInsertionPin
#pragma clang diagnostic ignored "-Wcomment"
#elif defined(__GNUC__)
// GCC warnings
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wmultichar"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wcomment"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#pragma GCC diagnostic ignored "-Wconversion-null"
#pragma GCC diagnostic ignored "-Wmultichar"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Waggressive-loop-optimizations"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

typedef void *HDC;
typedef void *HGLRC;

#define WM_MOUSEWHEEL 1
#define WM_MBUTTONUP 2
#define WM_MBUTTONDOWN 3
#define WM_CLOSE 10
#define WM_XBUTTONDOWN 11
#define WM_SETTEXT 13
#define WM_SHOWWINDOW 14
#define WM_NCACTIVATE 15
#define SW_SHOWNORMAL 0
#define LANG_RUSSIAN 0
#define LANG_FRENCH 1
#define LANG_GERMAN 2
#define LANG_SPANISH 3
#define LANG_JAPANESE 4
#define LANG_KOREAN 5

// Bad and very ugly "API" stuff
int DefWindowProc(void *, unsigned int, uintptr_t, uintptr_t);
int GetSystemDefaultLangID();
int GetProfileStringA(const char *, const char *, const char *, char *, int);

// cmd line
void *ShellExecuteA(void *, const char *, const char *, const char *, const char *, int);

// Thread
void KillTimer(void *, unsigned int);
void SetTimer(void *, unsigned int, unsigned int, void *);

// String
#define strncpy_s strncpy
#define lstrlenW wcslen
#define sprintf_s sprintf
#define sscanf_s sscanf
#define vsprintf_s vsprintf
#define vswprintf_s(a, b, c) vswprintf(a, 0, b, c)

#endif
