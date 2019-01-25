// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIResizepic.h"

class CGUIHTMLResizepic : public CGUIResizepic
{
private:
    class CGUIHTMLGump *m_HTMLGump{ nullptr };

public:
    CGUIHTMLResizepic(
        class CGUIHTMLGump *htmlGump,
        int serial,
        uint16_t graphic,
        int x,
        int y,
        int width,
        int height);
    virtual ~CGUIHTMLResizepic();

    void Scroll(bool up, int delay);
    virtual bool IsControlHTML() { return true; }
};
