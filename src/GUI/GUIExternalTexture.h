// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
struct CSprite;

class CGUIExternalTexture : public CBaseGUI
{
    bool DeleteTextureOnDestroy = false;
    short DrawWidth = 0;
    short DrawHeight = 0;

public:
    CGUIExternalTexture(
        CSprite *sprite,
        bool deleteTextureOnDestroy,
        int x,
        int y,
        int drawWidth = 0,
        int drawHeight = 0);
    virtual ~CGUIExternalTexture();

    CSprite *m_Sprite = nullptr;
    virtual CSize GetSize();

    virtual void SetShaderMode();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
