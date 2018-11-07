// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGUIResizepic : public CGUIPolygonal
{
public:
    CGUIResizepic(int serial, uint16_t graphic, int x, int y, int width, int height);
    virtual ~CGUIResizepic();
    virtual Wisp::CSize GetSize() { return Wisp::CSize(Width, Height); }
    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
