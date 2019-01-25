// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpMenu : public CGump
{
public:
    string Text = "";
    bool TextChanged = false;

protected:
    virtual void CalculateGumpState();

public:
    CGumpMenu(uint32_t serial, uint32_t id, short x, short y);
    virtual ~CGumpMenu();

    static const int ID_GM_HTMLGUMP = 1000;

    CGUIText *m_TextObject{ nullptr };

    void SendMenuResponse(int index);

    virtual void PrepareContent();

    virtual bool OnLeftMouseButtonDoubleClick();
};
