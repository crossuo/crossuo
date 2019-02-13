// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpSelectFont.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ConfigManager.h"

CGumpSelectFont::CGumpSelectFont(uint32_t serial, short x, short y, SELECT_FONT_GUMP_STATE state)
    : CGump(GT_SELECT_FONT, serial, x, y)
    , m_State(state)
{
}

CGumpSelectFont::~CGumpSelectFont()
{
}

void CGumpSelectFont::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    Clear();

    CGUIResizepic *background = (CGUIResizepic *)Add(new CGUIResizepic(0, 0x0A28, 0, 0, 200, 70));

    CGUIText *text = (CGUIText *)Add(new CGUIText(0, 60, 22));
    text->CreateTextureW(0, L"Select font");

    auto selected = 0;
    switch (m_State)
    {
        case SFGS_OPT_TOOLTIP:
        {
            selected = g_OptionsConfig.ToolTipsTextFont;
            break;
        }
        case SFGS_OPT_CHAT:
        {
            selected = g_OptionsConfig.ChatFont;
            break;
        }
        case SFGS_OPT_MISCELLANEOUS:
        {
            selected = g_OptionsConfig.SpeechFont;
            break;
        }
        default:
            break;
    }

    int drawY = 46;

    Add(new CGUIGroup(1));
    int count = 0;

    for (int i = 0; i < 20; i++)
    {
        if (g_FontManager.UnicodeFontExists(i))
        {
            CGUIRadio *radio = (CGUIRadio *)Add(
                new CGUIRadio((int)i + ID_GSF_FONTS, 0x00D0, 0x00D1, 0x00D0, 50, drawY));
            radio->Checked = (i == selected);
            radio->SetTextParameters((uint8_t)i, L"This font", 0);
            drawY += 22;
            count++;
        }
    }

    background->Height = 70 + (count * 22);

    Add(new CGUIGroup(0));
}

void CGumpSelectFont::GUMP_RADIO_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (!state)
    {
        return;
    }

    int realFont = -1;
    int count = serial - ID_GSF_FONTS;

    for (int i = 0; i < 20; i++)
    {
        if (g_FontManager.UnicodeFontExists(i))
        {
            if (count == 0)
            {
                realFont = i;
                break;
            }

            count--;
        }
    }

    if (realFont == -1)
    {
        return;
    }

    switch (m_State)
    {
        case SFGS_OPT_TOOLTIP:
        {
            g_OptionsConfig.ToolTipsTextFont = realFont;
            RemoveMark = true;

            break;
        }
        case SFGS_OPT_CHAT:
        {
            g_OptionsConfig.ChatFont = realFont;
            RemoveMark = true;

            break;
        }
        case SFGS_OPT_MISCELLANEOUS:
        {
            g_OptionsConfig.SpeechFont = realFont;
            RemoveMark = true;

            break;
        }
        default:
            break;
    }
}
