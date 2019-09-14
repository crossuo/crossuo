// MIT License
// Copyright (C) August 2017 Hotride

#pragma once

#include <math.h> // M_PI

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <string.h>
#include <wchar.h>
#include <algorithm>
#include <stdint.h>

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

#define UNUSED(x) (void)x
#define ToColorR(x) ((x)&0xff)
#define ToColorG(x) ((x >> 8) & 0xff)
#define ToColorB(x) ((x >> 16) & 0xff)
#define ToColorA(x) ((x >> 24) & 0xff)

struct SoundInfo;
typedef SoundInfo *SoundHandle;
#define SOUND_NULL nullptr

#if defined(XUO_WINDOWS)

#else // XUO_WINDOWS

#include <unistd.h>
#include <chrono>
#include <thread>
#include <limits.h> // INT_MAX

#endif // XUO_WINDOWS

#include "Globals.h"
