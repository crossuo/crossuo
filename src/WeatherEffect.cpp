// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "WeatherEffect.h"
#include "Misc.h"

CWeatherEffect::CWeatherEffect()
{
    ID = RandomInt(2000);
    ScaleRatio = (float)(ID % 20) / 13.0f;
}
