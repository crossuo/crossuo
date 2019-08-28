﻿// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIShopResult : public CBaseGUI
{
public:
    uint32_t Price = 0;
    string Name = "";

private:
    CTextSprite m_NameText;

public:
    CGUIShopResult(class CGUIShopItem *shopItem, int x, int y);
    virtual ~CGUIShopResult();

    class CGUIMinMaxButtons *m_MinMaxButtons{ nullptr };
    CBaseGUI *SelectedItem();
    virtual CSize GetSize() { return CSize(200, m_NameText.Height); }
    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
