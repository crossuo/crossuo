// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIHTMLResizepic.h"
#include "GUIHTMLGump.h"

CGUIHTMLResizepic::CGUIHTMLResizepic(
    class CGUIHTMLGump *htmlGump, int serial, uint16_t graphic, int x, int y, int width, int height)
    : CGUIResizepic(serial, graphic, x, y, width, height)
    , m_HTMLGump(htmlGump)
{
}

CGUIHTMLResizepic::~CGUIHTMLResizepic()
{
}

void CGUIHTMLResizepic::Scroll(bool up, int delay)
{
    if (m_HTMLGump != nullptr)
    {
        m_HTMLGump->Scroll(up, delay);
    }
}
