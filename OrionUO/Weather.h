// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "WeatherEffect.h"

class CWeather
{
public:
    uint8_t Type = 0;
    uint8_t Count = 0;
    uint8_t CurrentCount = 0;
    uint8_t Temperature = 0;
    uint32_t Timer = 0;
    char Wind = 0;
    uint32_t WindTimer = 0;
    uint32_t LastTick = 0;
    float SimulationRatio = 37.0f;

    CWeather();
    ~CWeather() { m_Effects.clear(); }

    deque<CWeatherEffect> m_Effects;

    void Reset();
    void Generate();
    void Draw(int x, int y);
};

extern CWeather g_Weather;
