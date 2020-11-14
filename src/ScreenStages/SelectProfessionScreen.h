// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseScreen.h"
#include "../Gumps/GumpScreenSelectProfession.h"

class CSelectProfessionScreen : public CBaseScreen
{
protected:
    int m_SkillSelection = 0;

public:
    int GetSkillSelection() { return m_SkillSelection; };
    void SetSkillSelection(int val);

private:
    CGumpScreenSelectProfession m_SelectProfessionGump;

public:
    CSelectProfessionScreen();
    virtual ~CSelectProfessionScreen();

    static const uint8_t ID_SMOOTH_SPS_QUIT = 1;
    static const uint8_t ID_SMOOTH_SPS_GO_SCREEN_CHARACTER = 2;
    static const uint8_t ID_SMOOTH_SPS_GO_SCREEN_GAME_CONNECT = 3;
    static const uint8_t ID_SMOOTH_SPS_GO_SCREEN_CREATE = 4;

    virtual void Init() override;
    virtual void ProcessSmoothAction(uint8_t action = 0xFF) override;
};

extern CSelectProfessionScreen g_SelectProfessionScreen;
