// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseScreen.h"
#include "../Gumps/GumpScreenSelectTown.h"

class CCityItem;

class CSelectTownScreen : public CBaseScreen
{
private:
    CGumpScreenSelectTown m_SelectTownGump;

public:
    CSelectTownScreen();
    ~CSelectTownScreen();

    static const uint8_t ID_SMOOTH_STS_QUIT = 1;
    static const uint8_t ID_SMOOTH_STS_GO_SCREEN_CHARACTER = 2;
    static const uint8_t ID_SMOOTH_STS_GO_SCREEN_GAME_CONNECT = 3;

    CCityItem *m_City{ nullptr };

    void Init();
    void ProcessSmoothAction(uint8_t action = 0xFF);
};

extern CSelectTownScreen g_SelectTownScreen;
