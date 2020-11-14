// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "TextData.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/FontsManager.h"

CTextData::CTextData(CTextData *obj)
    : Unicode(obj->Unicode)
    , Type(obj->Type)
    , Font(obj->Font)
    , Timer(obj->Timer)
    , Alpha(obj->Alpha)
{
    RealDrawX = obj->RealDrawX;
    RealDrawY = obj->RealDrawY;
    Color = obj->Color;
    Text = obj->Text;
    UnicodeText = obj->UnicodeText;
}

CTextData::~CTextData()
{
    m_TextSprite.Clear();
    Owner = nullptr;
}

bool CTextData::CanBeDrawedInJournalGump()
{
    bool result = true;

    switch (Type)
    {
        case TT_SYSTEM:
            result = g_JournalShowSystem;
            break;
        case TT_OBJECT:
            result = g_JournalShowObjects;
            break;
        case TT_CLIENT:
            result = g_JournalShowClient;
            break;
        default:
            break;
    }

    return result;
}

void CTextData::GenerateTexture(
    int maxWidth, uint16_t flags, TEXT_ALIGN_TYPE align, uint8_t cell, int font)
{
    if (Unicode)
    {
        if (font == -1)
        {
            font = Font;
        }

        g_FontManager.GenerateW(
            (uint8_t)font, m_TextSprite, UnicodeText, Color, cell, maxWidth, align, flags);
    }
    else
    {
        g_FontManager.GenerateA((uint8_t)Font, m_TextSprite, Text, Color, maxWidth, align, flags);
    }

    if (!m_TextSprite.Empty())
    {
        if (g_ConfigManager.ScaleSpeechDelay)
        {
            Timer += (((4000 * m_TextSprite.LinesCount) * g_ConfigManager.SpeechDelay) / 100);
        }
        else
        {
            uint32_t delay =
                ((int64_t)((int64_t)5497558140000 * g_ConfigManager.SpeechDelay) >> 32) >> 5;
            Timer += (uint32_t)((delay >> 31) + delay);
        }
    }
}
