// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../Sprite.h"

class CGUIShopItem : public CBaseGUI
{
public:
    uint32_t Count = 0;
    uint32_t Price = 0;
    std::string Name = "";
    bool Selected = false;
    bool NameFromCliloc = false;

private:
    CTextSprite m_NameText;
    CTextSprite m_CountText;
    int m_ImageOffset{ 0 };
    int m_TextOffset{ 0 };
    int m_MaxOffset{ 0 };

public:
    CGUIShopItem(
        int serial,
        uint16_t graphic,
        uint16_t color,
        int count,
        int price,
        const std::string &name,
        int x,
        int y);
    virtual ~CGUIShopItem();

    void OnClick();
    void CreateNameText();
    void UpdateOffsets();
    void CreateCountText(int lostCount);
    virtual CSize GetSize() { return CSize(200, m_MaxOffset + 20); }
    virtual void PrepareTextures();
    virtual void SetShaderMode();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
