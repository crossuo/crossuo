// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "RenderWorldObject.h"

class CGameEffect : public CRenderWorldObject
{
public:
    EFFECT_TYPE EffectType = EF_MOVING;

    uint32_t DestSerial = 0;
    uint16_t DestX = 0;
    uint16_t DestY = 0;
    char DestZ = 0;
    uint8_t Speed = 0;
    uint32_t Duration = 0;
    bool FixedDirection = false;
    bool Explode = false;
    uint32_t RenderMode = 0;
    int AnimIndex = 0;
    int Increment = 0;
    uint32_t LastChangeFrameTime = 0;

    CGameEffect();
    virtual ~CGameEffect();

    virtual void Draw(int x, int y);
    uint16_t GetCurrentGraphic();
    uint16_t CalculateCurrentGraphic();
    void ApplyRenderMode();
    void RemoveRenderMode();
    bool IsEffectObject() { return true; }
    virtual void Update(class CGameObject *parent);
};
