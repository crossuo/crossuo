// MIT License
// Copyright (C) August 2016
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>

struct MulStaticTile2;

struct CObjectOnCursor
{
    MulStaticTile2 *TiledataPtr = nullptr;
    uint32_t Serial = 0;
    uint32_t Container = 0;
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    uint16_t Count = 0;
    uint16_t TotalCount = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    int8_t Z = 0;
    uint8_t Layer = 0;
    uint8_t Flags = 0;
    bool IsGameFigure = false;
    bool UpdatedInWorld = false;
    bool Enabled = false;
    bool Dropped = false;

    void Clear();
    uint16_t GetDrawGraphic(bool &doubleDraw);
};

extern CObjectOnCursor g_ObjectInHand;
