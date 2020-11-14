// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../Globals.h" // SCREEN_EFFECT_*, SET_TO_WHITE_THEN_BLACK, g_Ticks, SET_TO_BLACK, SEM_*

class CScreenEffectManager
{
public:
    SCREEN_EFFECT_MODE Mode = SEM_NONE;
    SCREEN_EFFECT_TYPE Type = SET_TO_BLACK;
    bool Enabled = false;
    float ColorR = 0.0f;
    float ColorG = 0.0f;
    float ColorB = 0.0f;
    float Alpha = 1.0f;
    float Step = 0.03f;

    CScreenEffectManager();
    virtual ~CScreenEffectManager();

    int Process();
    void Draw();
    bool
    Use(const SCREEN_EFFECT_MODE &mode,
        const SCREEN_EFFECT_TYPE &type = SET_TO_BLACK,
        bool ignoreEnabled = false);
    bool UseSunrise();
    bool UseSunset();
};

extern CScreenEffectManager g_ScreenEffectManager;
