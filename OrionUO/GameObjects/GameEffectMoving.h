// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGameEffectMoving : public CGameEffectDrag
{
public:
    float Angle = 0.0f;
    int OffsetZ = 0;

    CGameEffectMoving();
    virtual ~CGameEffectMoving();
    virtual void Update(class CGameObject *parent);
};
