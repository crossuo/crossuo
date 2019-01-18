// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIComboboxText.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"

CGUIComboboxText::CGUIComboboxText(
    uint16_t color,
    uint8_t font,
    const string &str,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
    : CGUIText(color, 0, 0)
{
    DEBUG_TRACE_FUNCTION;
    Serial = 0xFFFFFFFE;
    CreateTextureA(font, str, width, align, flags);
}

CGUIComboboxText::CGUIComboboxText(
    uint16_t color,
    uint8_t font,
    const wstring &str,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
    : CGUIText(color, 0, 0)
{
    DEBUG_TRACE_FUNCTION;
    Serial = 0xFFFFFFFE;
    CreateTextureW(font, str, 30, width, align, flags);
}

CGUIComboboxText::~CGUIComboboxText()
{
}

void CGUIComboboxText::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUIComboboxText::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}
