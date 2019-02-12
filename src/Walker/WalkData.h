// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CWalkData
{
public:
    short X = 0;
    short Y = 0;
    char Z = 0;
    uint8_t Direction = 0;
    uint16_t Graphic = 0;
    uint8_t Flags = 0;

    CWalkData() {}
    CWalkData(short x, short y, char z, uint8_t direction, uint16_t graphic, uint8_t flags)
        : X(x)
        , Y(y)
        , Z(z)
        , Direction(direction)
        , Graphic(graphic)
        , Flags(flags)
    {
    }
    ~CWalkData() {}

    bool Run() { return (Direction & 0x80) != 0; } // FIXME
    void GetOffset(float &x, float &y, float &steps);
};
