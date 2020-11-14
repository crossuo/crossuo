// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include "Platform.h"
#include <SDL_thread.h>
#include <SDL_stdinc.h>
#include <stdlib.h>

#define XUO_M_PI 3.14159265358979323846264338327950288

extern SDL_threadID g_MainThread;
const int PACKET_VARIABLE_SIZE = 0;

int CalculatePercents(int max, int current, int maxValue);

inline float deg2radf(float degr)
{
    return degr * (float)(XUO_M_PI / 180.0f);
}

inline float rad2degf(float radians)
{
    return (float)(radians * 180.0f / XUO_M_PI);
}

inline int RandomInt(int n)
{
    return (rand() % n);
}

inline int RandomIntMinMax(int n, int m)
{
    return (rand() % (m - n) + n);
}

inline uint32_t unpack32(uint8_t *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

inline uint16_t unpack16(uint8_t *buf)
{
    return (buf[0] << 8) | buf[1];
}

inline void pack32(uint8_t *buf, uint32_t x)
{
    buf[0] = uint8_t(x >> 24);
    buf[1] = uint8_t((x >> 16) & 0xff);
    buf[2] = uint8_t((x >> 8) & 0xff);
    buf[3] = uint8_t(x & 0xff);
}

inline void pack16(uint8_t *buf, uint16_t x)
{
    buf[0] = x >> 8;
    buf[1] = x & 0xff;
}
