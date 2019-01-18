// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpMinimap : public CGump
{
public:
    uint16_t LastX = 0;
    uint16_t LastY = 0;

private:
    const int ID_GMM_LOCK_MOVING = 1;

    uint8_t m_Count{ 0 };

    CGLTexture m_Texture{ CGLTexture() };

    CGUIGumppic *m_Body{ nullptr };
    CGUIDataBox *m_DataBox{ nullptr };

    void GenerateMap();

    void CreatePixels(
        vector<uint16_t> &data,
        int color,
        int x,
        int y,
        int width,
        int height,
        const CPoint2Di *table,
        int count);

protected:
    virtual void CalculateGumpState();

public:
    CGumpMinimap(short x, short y, bool minimized);
    virtual ~CGumpMinimap();

    virtual void PrepareContent();

    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};
