// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Platform.h"
#include "BaseScreen.h"
#include "../Gumps/GumpScreenCharacterList.h"

class CCharacterListScreen : public CBaseScreen
{
private:
    CGumpScreenCharacterList m_CharacterListGump;

public:
    CCharacterListScreen();
    virtual ~CCharacterListScreen();

    static const uint8_t ID_SMOOTH_CLS_QUIT = 1;
    static const uint8_t ID_SMOOTH_CLS_CONNECT = 2;
    static const uint8_t ID_SMOOTH_CLS_SELECT_CHARACTER = 3;
    static const uint8_t ID_SMOOTH_CLS_GO_SCREEN_PROFESSION_SELECT = 4;
    static const uint8_t ID_SMOOTH_CLS_GO_SCREEN_DELETE = 5;

    void Init();
    void ProcessSmoothAction(uint8_t action = 0xFF);

    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CCharacterListScreen g_CharacterListScreen;
