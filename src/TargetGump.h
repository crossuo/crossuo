// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGameCharacter;

class CTargetGump
{
public:
    int X = 0;
    int Y = 0;
    int Hits = 0;
    uint16_t Color = 0;
    uint16_t HealthColor = 0;
    CGameCharacter *TargetedCharacter = nullptr;
    CTargetGump();
    ~CTargetGump();

    void Draw();
};

class CNewTargetSystem
{
public:
    uint32_t Serial = 0;
    int X = 0;
    int TopY = 0;
    int BottomY = 0;
    int GumpX = 20;
    int GumpY = 20;
    int Hits = 0;
    uint16_t GumpTop = 0;
    uint16_t GumpBottom = 0;
    uint16_t ColorGump = 0;
    uint16_t HealthColor = 0;
    CGameCharacter *TargetedCharacter = nullptr;
    CNewTargetSystem();
    ~CNewTargetSystem();

    void Draw();
};

extern CTargetGump g_TargetGump;
extern CTargetGump g_AttackTargetGump;
extern CNewTargetSystem g_NewTargetSystem;
