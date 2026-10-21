// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../Sprite.h"

class CGUIBulletinBoardObject : public CBaseGUI
{
    wstr_t Text;

private:
    CTextSprite m_Texture;

public:
    CGUIBulletinBoardObject(int serial, int x, int y, const wstr_t &text);
    virtual ~CGUIBulletinBoardObject() override;

    virtual void PrepareTextures() override;
    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;
};
