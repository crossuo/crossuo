// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
struct CSprite;

// how a CGUIExternalTexture places its sprite inside the widget rectangle
enum class EXTERNAL_TEXTURE_DRAW_MODE
{
    Native,  // draw at the sprite's own size
    Stretch, // stretch the sprite to DrawWidth x DrawHeight
    Tile,    // repeat the sprite to fill DrawWidth x DrawHeight (clipped)
    Fit,     // aspect-fit the sprite into DrawWidth x DrawHeight, centered
};

class CGUIExternalTexture : public CBaseGUI
{
    bool DeleteTextureOnDestroy = false;
    short DrawWidth = 0;
    short DrawHeight = 0;

public:
    // Native scales with DrawWidth/DrawHeight as before when they are set
    EXTERNAL_TEXTURE_DRAW_MODE DrawMode = EXTERNAL_TEXTURE_DRAW_MODE::Native;

    CGUIExternalTexture(
        CSprite *sprite,
        bool deleteTextureOnDestroy,
        int x,
        int y,
        int drawWidth = 0,
        int drawHeight = 0);
    virtual ~CGUIExternalTexture() override;

    CSprite *m_Sprite = nullptr;
    virtual CSize GetSize() override;

    virtual void SetShaderMode() override;
    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;
};
