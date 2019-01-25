// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Platform.h"
#include "BaseScreen.h"
#include "../Gumps/GumpScreenCreateCharacter.h"

class CCreateCharacterScreen : public CBaseScreen
{
protected:
    int m_StyleSelection = 0;

public:
    int GetStyleSelection() { return m_StyleSelection; };
    void SetStyleSelection(int val);

protected:
    int m_ColorSelection = 0;

public:
    int GetColorSelection() { return m_ColorSelection; };
    void SetColorSelection(int val);
    string Name = "";

private:
    CGumpScreenCreateCharacter m_CreateCharacterGump;

public:
    CCreateCharacterScreen();
    virtual ~CCreateCharacterScreen();

    static const uint8_t ID_SMOOTH_CCS_QUIT = 1;
    static const uint8_t ID_SMOOTH_CCS_GO_SCREEN_CHARACTER = 2;
    static const uint8_t ID_SMOOTH_CCS_GO_SCREEN_CONNECT = 3;
    static const uint8_t ID_SMOOTH_CCS_GO_SCREEN_SELECT_TOWN = 4;

    void Init();
    void ProcessSmoothAction(uint8_t action = 0xFF);
    virtual void OnLeftMouseButtonDown();
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CCreateCharacterScreen g_CreateCharacterScreen;
