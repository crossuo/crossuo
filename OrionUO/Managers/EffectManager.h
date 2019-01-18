// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../BaseQueue.h"

class CGameEffect;

class CEffectManager : public CBaseQueue
{
public:
    CEffectManager();
    virtual ~CEffectManager() {}

    void AddEffect(CGameEffect *effect);
    void RemoveEffect(CGameEffect *effect);
    void CreateExplodeEffect(CGameEffect *effect, const EFFECT_TYPE &type);
    void UpdateEffects();
    void RemoveRangedEffects();
};

extern CEffectManager g_EffectManager;
