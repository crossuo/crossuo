// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGUITilepic : public CGUIDrawObject
{
public:
    CGUITilepic(uint16_t graphic, uint16_t color, int x, int y);
    virtual ~CGUITilepic();

    virtual Wisp::CSize GetSize();

    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
