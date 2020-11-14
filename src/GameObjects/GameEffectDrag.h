// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GameEffect.h"

class CGameObject;

class CGameEffectDrag : public CGameEffect
{
public:
    int OffsetX = 0;
    int OffsetY = 0;
    uint32_t LastMoveTime = 0;
    uint8_t MoveDelay = 20;

    CGameEffectDrag();
    virtual ~CGameEffectDrag();
    virtual void Update(CGameObject *parent);
};
