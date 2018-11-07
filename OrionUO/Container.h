// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CContainerOffsetRect
{
public:
    int MinX = 0;
    int MinY = 0;
    int MaxX = 0;
    int MaxY = 0;

    CContainerOffsetRect() {}
    CContainerOffsetRect(int minX, int minY, int maxX, int maxY)
        : MinX(minX)
        , MinY(minY)
        , MaxX(maxX)
        , MaxY(maxY)
    {
    }
    ~CContainerOffsetRect() {}
};

class CContainerOffset
{
public:
    uint16_t Gump = 0;
    uint16_t OpenSound = 0;
    uint16_t CloseSound = 0;
    CContainerOffsetRect Rect = CContainerOffsetRect();

    CContainerOffset() {}
    CContainerOffset(
        uint16_t gump, uint16_t openSound, uint16_t closeSound, const CContainerOffsetRect &rect)
        : Gump(gump)
        , OpenSound(openSound)
        , CloseSound(closeSound)
        , Rect(rect)
    {
    }
    ~CContainerOffset() {}
};

class CContainerRect
{
public:
    short DefaultX = 40;
    short DefaultY = 40;

    short X = 40;
    short Y = 40;

    CContainerRect() {}
    ~CContainerRect() {}

    void Calculate(uint16_t gumpID);
    void MakeDefault()
    {
        X = DefaultX;
        Y = DefaultY;
    }
};

extern vector<CContainerOffset> g_ContainerOffset;
extern CContainerRect g_ContainerRect;
