// MIT License
// Copyright (C) August 2016 Hotride

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
    virtual ~CGUIBulletinBoardObject();

    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
