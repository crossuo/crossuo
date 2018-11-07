// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGUIPolygonal : public CBaseGUI
{
public:
    int Width = 0;
    int Height = 0;
    bool CallOnMouseUp = false;

    CGUIPolygonal(
        GUMP_OBJECT_TYPE type, int x, int y, int width, int height, bool callOnMouseUp = false);
    virtual ~CGUIPolygonal();
    virtual bool Select();
    virtual Wisp::CSize GetSize() { return Wisp::CSize(Width, Height); }
};
