// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIButton.h"

class CGUIButtonTileart : public CGUIButton
{
    uint16_t TileGraphic = 0;
    int TileX = 0;
    int TileY = 0;

public:
    CGUIButtonTileart(
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y,
        uint16_t tileGraphic,
        uint16_t tileColor,
        int tileX,
        int tileY);
    virtual ~CGUIButtonTileart() override;

    virtual CSize GetSize() override;

    virtual void PrepareTextures() override;

    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;
};
