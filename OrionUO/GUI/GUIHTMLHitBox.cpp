// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIHTMLHitBox.h"
#include "GUIHTMLGump.h"

CGUIHTMLHitBox::CGUIHTMLHitBox(
    CGUIHTMLGump *htmlGump, int serial, int x, int y, int width, int height, bool callOnMouseUp)
    : CGUIHitBox(serial, x, y, width, height, callOnMouseUp)
    , m_HTMLGump(htmlGump)
{
}

CGUIHTMLHitBox::~CGUIHTMLHitBox()
{
}

void CGUIHTMLHitBox::Scroll(bool up, int delay)
{
    DEBUG_TRACE_FUNCTION;
    if (m_HTMLGump != nullptr)
    {
        m_HTMLGump->Scroll(up, delay);
    }
}
