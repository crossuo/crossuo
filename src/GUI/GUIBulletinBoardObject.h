// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIBulletinBoardObject : public CBaseGUI
{
    wstring Text = {};

private:
    CGLTextTexture m_Texture;

public:
    CGUIBulletinBoardObject(int serial, int x, int y, const wstring &text);
    virtual ~CGUIBulletinBoardObject();

    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
