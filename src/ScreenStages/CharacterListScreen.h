// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../Platform.h"
#include "BaseScreen.h"
#include "../Gumps/GumpScreenCharacterList.h"

class CCharacterListScreen : public CBaseScreen
{
    CGumpScreenCharacterList m_CharacterListGump;

public:
    enum : uint8_t
    {
        Quit = 1,
        Connect = 2,
        SelectCharacter = 3,
        GotoScreenProfession = 4,
        GotoScreenDelete = 5,
        LogOut = 6,
    };

    CCharacterListScreen();
    virtual ~CCharacterListScreen() override = default;
    virtual void Init() override;
    virtual void PrepareContent() override;
    virtual void ProcessSmoothAction(uint8_t action = 0xff) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;

private:
    bool m_AutoLoginFired = false;
};

extern CCharacterListScreen g_CharacterListScreen;
