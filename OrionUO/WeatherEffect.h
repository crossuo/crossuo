// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CWeatherEffect
{
public:
    float SpeedX = 0.0f;
    float SpeedY = 0.0f;
    float X = 0.0f;
    float Y = 0.0f;
    uint32_t ID = 0;
    float ScaleRatio = 0.0f;
    float SpeedAngle = 0.0f;
    float SpeedMagnitude = 0.0f;

    CWeatherEffect();
    virtual ~CWeatherEffect() {}
};
