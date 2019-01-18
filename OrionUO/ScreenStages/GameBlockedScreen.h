// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseScreen.h"
#include "../Platform.h"

class CGameBlockedScreen : public CBaseScreen
{
public:
    uint8_t Code = 0;
    CGump m_GameBlockedScreenGump;

    CGameBlockedScreen();
    ~CGameBlockedScreen();

    virtual void Init();
    virtual void Render() override;
    virtual void SelectObject() override;
    virtual void OnLeftMouseButtonDown();
    virtual void OnLeftMouseButtonUp();
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CGameBlockedScreen g_GameBlockedScreen;
