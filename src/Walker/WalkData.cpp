// MIT License
// Copyright (C) August 2016 Hotride

#include <cstdlib>
#include "WalkData.h"

void CWalkData::GetOffset(float &x, float &y, float &steps)
{
    float step_NESW_D = 44.0f / steps; //NW NE SW SE
    float step_NESW = 22.0f / steps;   //N E S W

    int checkX = 22;
    int checkY = 22;

    switch (Direction & 7)
    {
        case 0: //W
        {
            x *= step_NESW;
            y *= -step_NESW;
            break;
        }
        case 1: //NW
        {
            x *= step_NESW_D;
            checkX = 44;
            y = 0.0f;
            break;
        }
        case 2: //N
        {
            x *= step_NESW;
            y *= step_NESW;
            break;
        }
        case 3: //NE
        {
            x = 0.0f;
            y *= step_NESW_D;
            checkY = 44;
            break;
        }
        case 4: //E
        {
            x *= -step_NESW;
            y *= step_NESW;
            break;
        }
        case 5: //SE
        {
            x *= -step_NESW_D;
            checkX = 44;
            y = 0.0f;
            break;
        }
        case 6: //S
        {
            x *= -step_NESW;
            y *= -step_NESW;
            break;
        }
        case 7: //SW
        {
            x = 0.0f;
            y *= -step_NESW_D;
            checkY = 44;
            break;
        }
        default:
            break;
    }

    int valueX = (int)x;

    if (abs(valueX) > checkX)
    {
        if (valueX < 0)
        {
            x = -(float)checkX;
        }
        else
        {
            x = (float)checkX;
        }
    }

    int valueY = (int)y;

    if (abs(valueY) > checkY)
    {
        if (valueY < 0)
        {
            y = -(float)checkY;
        }
        else
        {
            y = (float)checkY;
        }
    }
}
