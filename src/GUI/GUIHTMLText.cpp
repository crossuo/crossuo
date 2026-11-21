// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIHTMLText.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../Utility/PerfMarker.h"
#include <cstdint>

CGUIHTMLText::CGUIHTMLText(
    int index,
    uint8_t font,
    uint16_t color,
    int x,
    int y,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t textFlags,
    int htmlStartColor)
    : CBaseGUI(GOT_HTMLTEXT, 0, 0, color, x, y)
    , TextID(index)
    , HTMLStartColor(htmlStartColor)
    , Text({})
    , Font(font)
    , Align(align)
    , TextFlags(textFlags)
    , Width(width)
{
}

CGUIHTMLText::~CGUIHTMLText()
{
    m_Sprite.Clear();
}

void CGUIHTMLText::Create(bool backgroundCanBeColored)
{
    g_FontManager.SetUseHTML(true, HTMLStartColor, backgroundCanBeColored);
    const bool ok =
        g_FontManager.GenerateW(Font, m_Sprite, Text, Color, 30, Width, Align, TextFlags);
    g_FontManager.SetUseHTML(false);
    if (!ok)
    {
        Warning(Client, "html text: could not render '%s'", str_from(Text).c_str());
    }
}

void CGUIHTMLText::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    m_Sprite.Draw(m_X, m_Y, checktrans);
}

bool CGUIHTMLText::Select()
{
    // only the link rects are clickable, not the whole text box: the gump
    // yields to the first item that selects, so a full-box html text steals
    // presses from anything sharing its area (a login theme places the
    // connect button under its link line). WebLinkUnderMouse takes the
    // absolute mouse position and shifts by the text origin itself; it
    // returns 0 when the pointer is outside every link.
    return m_Sprite.WebLinkUnderMouse(m_X, m_Y) != 0;
}
