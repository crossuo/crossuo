// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Platform.h"
#include "BaseScreen.h"
#include "../Gumps/GumpScreenServer.h"

class CServerScreen : public CBaseScreen
{
public:
    int SelectionServerTempValue = 0;

private:
    CGumpScreenServer m_ServerGump;

public:
    CServerScreen();
    virtual ~CServerScreen();

    static const uint8_t ID_SMOOTH_SS_QUIT = 1;
    static const uint8_t ID_SMOOTH_SS_SELECT_SERVER = 2;
    static const uint8_t ID_SMOOTH_SS_GO_SCREEN_MAIN = 3;

    virtual void Init() override;
    virtual void ProcessSmoothAction(uint8_t action = 0xFF) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CServerScreen g_ServerScreen;
