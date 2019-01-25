// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpShop : public CGump
{
    int m_ContinueCounter = 0;

    void UpdateTotalPrice();

public:
    bool IsBuyGump = false;

    CGumpShop(uint32_t serial, bool isBuyGump, short x, short y);
    virtual ~CGumpShop();

    CGUIHTMLGump *m_ItemList[2];
    CGUIText *m_TotalPriceText{ nullptr };
    CGUIText *m_TotalGoldText{ nullptr };
    CGUIText *m_NameText{ nullptr };

    void SendList();
    virtual void PrepareContent() override;
    virtual bool CanBeDisplayed() override { return Visible; }

    GUMP_BUTTON_EVENT_H;
    GUMP_SCROLL_BUTTON_EVENT_H;

    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
};
