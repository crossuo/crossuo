// GPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#pragma once

#if defined(__GNUC__) && __GNUC__ <= 5 && !__clang__
#define USE_PCH 0
#else
#define USE_PCH 1
#endif

#include <math.h> // M_PI

#if defined(XUO_WINDOWS)
#include "../../Dependencies/include/glew.h"
#include "../../Dependencies/include/wglew.h"
#else
#define NO_SDL_GLEXT
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <unistd.h>
#endif // XUO_WINDOWS

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <string.h>
#include <wchar.h>
#include <algorithm>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <deque>
#include <map>
#include <condition_variable>
#include <unordered_map>
#include <locale>
#include <sstream>
#include <istream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cassert>

using std::deque;
using std::map;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;
using std::wstring;

#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))
#if 0
#define DEBUG_TRACE_FUNCTION                                                                       \
    do                                                                                             \
    {                                                                                              \
        fprintf(stdout, "CALL: %s:%d: %s\n", __FILE__, __LINE__, __FUNCTION__);                    \
    } while (0)
#else
#define DEBUG_TRACE_FUNCTION
#endif

template <typename T, typename U>
static inline T checked_cast(U value)
{
    auto result = static_cast<T>(value);
    assert(static_cast<U>(result) == value && "Type conversion loses information");
    return result;
}

template <typename T, typename U>
static inline T checked_cast(U *value)
{
    auto result = checked_cast<T>((intptr_t)value);
    assert(static_cast<intptr_t>(result) == (intptr_t)value && "Type conversion loses information");
    return result;
}

extern string g_dumpUopFile;

struct SoundInfo;
typedef SoundInfo *SoundHandle;
#define SOUND_NULL nullptr

#if USE_PCH

#include "api/mulstruct.h"
#include "api/enumlist.h"
#include "api/uodata.h"

#endif // USE_PCH
