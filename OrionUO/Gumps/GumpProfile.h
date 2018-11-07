// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Platform.h"

class CGumpProfile : public CGumpBaseScroll
{
private:
    static const int ID_GP_APPLY = 1;
    static const int ID_GP_TEXT_FIELD = 2;

    CGUITextEntry *m_Entry{ nullptr };
    CGUIHitBox *m_HitBox{ nullptr };
    CBaseGUI *m_BottomData[4];

    void RecalculateHeight();

public:
    CGumpProfile(
        uint32_t serial, short x, short y, wstring topText, wstring bottomText, wstring dataText);
    virtual ~CGumpProfile();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};
