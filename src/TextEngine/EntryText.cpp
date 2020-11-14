// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <SDL_clipboard.h>
#include <common/str.h>
#include "EntryText.h"
#include "../TextEngine/GameConsole.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/GumpManager.h"
#include "../Gumps/Gump.h"

CEntryText *g_EntryPointer = nullptr;

CEntryText::CEntryText(int maxLength, int width, int maxWidth, bool numberOnly)
    : MaxLength(maxLength)
    , Width(width)
    , MaxWidth(maxWidth)
    , NumberOnly(numberOnly)
{
}

CEntryText::~CEntryText()
{
    if (g_EntryPointer == this)
    {
        if (g_ConfigManager.GetConsoleNeedEnter())
        {
            g_EntryPointer = nullptr;
        }
        else
        {
            if (g_GameState >= GS_GAME)
            {
                g_EntryPointer = &g_GameConsole;
            }
            else
            {
                g_EntryPointer = nullptr;
            }
        }
    }

    Clear();
}

void CEntryText::OnClick(
    CGump *gump, uint8_t font, bool unicode, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    if (g_EntryPointer != this)
    {
        CGump *gumpEntry = g_GumpManager.GetTextEntryOwner();
        if (gumpEntry != nullptr)
        {
            gumpEntry->FrameCreated = false;
        }

        g_EntryPointer = this;
        Changed = true;
    }
    if (g_EntryPointer == this)
    {
        int oldPos = m_Position;
        if (unicode)
        {
            m_Position = g_FontManager.CalculateCaretPosW(font, WText, x, y, Width, align, flags);
        }
        else
        {
            m_Position =
                g_FontManager.CalculateCaretPosA(font, GetTextA(), x, y, Width, align, flags);
        }

        if (oldPos != m_Position)
        {
            Changed = true;
        }
    }

    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::OnKey(CGump *gump, Keycode key)
{
    switch (key)
    {
        case KEY_HOME:
        {
            SetPos(0, gump);
            break;
        }
        case KEY_END:
        {
            SetPos((int)Length(), gump);
            break;
        }
        case KEY_LEFT:
        {
            AddPos(-1, gump);
            break;
        }
        case KEY_RIGHT:
        {
            AddPos(1, gump);
            break;
        }
        case KEY_BACK:
        {
            Remove(true, gump);
            break;
        }
        case KEY_DELETE:
        {
            Remove(false, gump);
            break;
        }
        default:
            break;
    }
}

int CEntryText::GetLinesCountA(uint8_t font, TEXT_ALIGN_TYPE align, uint16_t flags, int width)
{
    if (width == 0)
    {
        width = Width;
    }

    auto *info = g_FontManager.GetInfoA(font, GetTextA(), align, flags, width);
    int count = 0;
    while (info != nullptr)
    {
        auto *next = info->m_Next;
        delete info;
        info = next;
        count++;
    }

    return count;
}

int CEntryText::GetLinesCountW(uint8_t font, TEXT_ALIGN_TYPE align, uint16_t flags, int width)
{
    if (width == 0)
    {
        width = Width;
    }

    auto *info = g_FontManager.GetInfoW(font, GetTextW(), align, flags, width);
    int count = 0;
    while (info != nullptr)
    {
        auto *next = info->m_Next;
        delete info;
        info = next;
        count++;
    }

    return count;
}

bool CEntryText::Insert(char16_t ch, CGump *gump)
{
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (m_Position > (int)WText.length())
    {
        m_Position = (int)WText.length();
    }

    if (MaxLength > 0)
    {
        if (NumberOnly)
        {
            auto str = WText;
            str.insert(str.begin() + m_Position, ch);
            if (str_to_int(str) >= MaxLength)
            {
                return false;
            }
        }
        else if ((int)WText.length() >= MaxLength)
        {
            return false;
        }
    }

    WText.insert(WText.begin() + m_Position, ch);
    m_Position++;
    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }

    return true;
}

void CEntryText::Remove(bool left, CGump *gump)
{
    if (left)
    {
        if (m_Position < 1)
        {
            return;
        }
        m_Position--;
    }
    else
    {
        if (m_Position >= (int)WText.length())
        {
            return;
        }
    }

    if (m_Position < (int)WText.length())
    {
        WText.erase(WText.begin() + m_Position);
    }
    else
    {
        WText.erase(WText.length() - 1);
    }

    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::Clear()
{
    WText = {};
    m_Position = 0;
    Changed = true;
    DrawOffset = 0;
    m_Texture.Clear();
    CaretPos.Reset();
}

void CEntryText::Paste()
{
    auto chBuffer = SDL_GetClipboardText();
    if (chBuffer != nullptr && (strlen(chBuffer) != 0u))
    {
        auto str = g_EntryPointer->GetTextW() + wstr_from(chBuffer);
        g_EntryPointer->SetTextW(str);
    }
}

void CEntryText::AddPos(int val, CGump *gump)
{
    m_Position += val;
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (m_Position > (int)WText.length())
    {
        m_Position = (int)WText.length();
    }

    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::SetPos(int val, CGump *gump)
{
    m_Position = val;
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (m_Position > (int)WText.length())
    {
        m_Position = (int)WText.length();
    }

    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::SetTextA(const astr_t &text)
{
    SetTextW(wstr_from(text));
}

void CEntryText::SetTextW(const wstr_t &text)
{
    Clear();

    WText = text;
    m_Position = (int)WText.length();
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (MaxLength > 0)
    {
        if (NumberOnly)
        {
            auto str = str_from(WText);
            while (true)
            {
                size_t len = str.length();

                if (str_to_int(str) >= MaxLength && len > 0)
                {
                    str.resize(len - 1);
                }
                else
                {
                    break;
                }
            }
        }
        else if ((int)WText.length() >= MaxLength)
        {
            WText.resize(MaxLength);
        }
    }

    CGump *gump = g_GumpManager.GetTextEntryOwner();
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

astr_t CEntryText::CheckMaxWidthA(uint8_t font, astr_t str)
{
    if (MaxWidth > 0)
    {
        int width = g_FontManager.GetWidthA(font, str);
        size_t len = str.length();
        while (MaxWidth < width && len > 0)
        {
            str.erase(str.begin() + len);
            len--;
            width = g_FontManager.GetWidthA(font, str);
        }
    }

    return str;
}

wstr_t CEntryText::CheckMaxWidthW(uint8_t font, wstr_t str)
{
    if (MaxWidth > 0)
    {
        int width = g_FontManager.GetWidthW(font, str);
        size_t len = str.length();
        while (MaxWidth < width && len > 0)
        {
            str.erase(str.begin() + len);
            len--;
            width = g_FontManager.GetWidthW(font, str);
        }
    }

    return str;
}

void CEntryText::FixMaxWidthA(uint8_t font)
{
    if (MaxWidth <= 0)
    {
        return;
    }

    int width = g_FontManager.GetWidthA(font, GetTextA());
    size_t len = GetTextA().length();
    while (MaxWidth < width && len > 0)
    {
        Remove((m_Position > 0), nullptr);
        len--;
        width = g_FontManager.GetWidthA(font, GetTextA());
    }
}

void CEntryText::FixMaxWidthW(uint8_t font)
{
    if (MaxWidth <= 0)
    {
        return;
    }

    int width = g_FontManager.GetWidthW(font, WText);
    size_t len = WText.length();
    while (MaxWidth < width && len > 0)
    {
        Remove((m_Position > 0), nullptr);
        len--;
        width = g_FontManager.GetWidthW(font, WText);
    }
}

void CEntryText::CreateTextureA(
    uint8_t font,
    const astr_t &str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    if (str.length() == 0u)
    {
        Clear();
        return;
    }
    m_Texture.Clear();
    if (m_Position != 0)
    {
        CaretPos = g_FontManager.GetCaretPosA(font, str, m_Position, width, align, flags);

        if ((flags & UOFONT_FIXED) != 0)
        {
            if (DrawOffset != 0)
            {
                if (CaretPos.X + DrawOffset < 0)
                {
                    DrawOffset = -CaretPos.X;
                }
                else if (Width + -DrawOffset < CaretPos.X)
                {
                    DrawOffset = Width - CaretPos.X;
                }
            }
            else if (Width + DrawOffset < CaretPos.X)
            {
                DrawOffset = Width - CaretPos.X;
            }
            else
            {
                DrawOffset = 0;
            }

            /*if (Width + DrawOffset < CaretPos.x)
                DrawOffset = Width - CaretPos.x;
            else
                DrawOffset = 0;*/
        }
    }
    else
    {
        CaretPos.Reset();
        DrawOffset = 0;
    }
    g_FontManager.GenerateA(font, m_Texture, str, color, Width + abs(DrawOffset), align, flags);
}

void CEntryText::CreateTextureW(
    uint8_t font,
    const wstr_t &str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    if (str.length() == 0u)
    {
        Clear();
        return;
    }

    m_Texture.Clear();
    if (m_Position != 0)
    {
        CaretPos = g_FontManager.GetCaretPosW(font, str, m_Position, width, align, flags);
        if ((flags & UOFONT_FIXED) != 0)
        {
            if (DrawOffset != 0)
            {
                if (CaretPos.X + DrawOffset < 0)
                {
                    DrawOffset = -CaretPos.X;
                }
                else if (Width + -DrawOffset < CaretPos.X)
                {
                    DrawOffset = Width - CaretPos.X;
                }
            }
            else if (Width + DrawOffset < CaretPos.X)
            {
                DrawOffset = Width - CaretPos.X;
            }
            else
            {
                DrawOffset = 0;
            }
        }
    }
    else
    {
        CaretPos.Reset();
        DrawOffset = 0;
    }
    g_FontManager.GenerateW(font, m_Texture, str, color, 30, Width, align, flags);
}

void CEntryText::PrepareToDrawA(uint8_t font, uint16_t color, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    if (Changed || Color != color)
    {
        FixMaxWidthA(font);
        CreateTextureA(font, GetTextA(), color, /*MaxWidth*/ Width, align, flags);
        Changed = false;
        Color = color;
        if (this == g_EntryPointer)
        {
            g_FontManager.GenerateA(font, m_CaretTexture, "_", color);
        }
    }
}

void CEntryText::PrepareToDrawW(uint8_t font, uint16_t color, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    if (Changed || Color != color)
    {
        FixMaxWidthW(font);
        CreateTextureW(font, WText, color, Width, align, flags);
        Changed = false;
        Color = color;
        if (this == g_EntryPointer)
        {
            g_FontManager.GenerateW(font, m_CaretTexture, L"_", color, 30);
        }
    }
}

void CEntryText::DrawA(
    uint8_t font, uint16_t color, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    PrepareToDrawA(font, color, align, flags);
    m_Texture.Draw(x + DrawOffset, y);
    if (this == g_EntryPointer)
    {
        const int offsetTable[] = { 1, 2, 1, 1, 1, 2, 1, 1, 2, 2 };
        const int offsY = offsetTable[font % 10];
        m_CaretTexture.Draw(x + DrawOffset + CaretPos.X, y + offsY + CaretPos.Y);
    }
}

void CEntryText::DrawW(
    uint8_t font, uint16_t color, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    PrepareToDrawW(font, color, align, flags);
    m_Texture.Draw(x + DrawOffset, y);
    if (this == g_EntryPointer)
    {
        m_CaretTexture.Draw(x + DrawOffset + CaretPos.X, y + CaretPos.Y);
    }
}

void CEntryText::DrawMaskA(
    uint8_t font, uint16_t color, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    const auto len = (int)Length();
    astr_t str{};
    for (int i = 0; i < len; i++)
    {
        str += "*";
    }

    if (len != 0)
    {
        g_FontManager.DrawA(font, str, color, x + DrawOffset, y);
    }

    if (this == g_EntryPointer)
    {
        const int offsetTable[] = { 1, 2, 1, 1, 1, 2, 1, 1, 2, 2 };
        const int offsY = offsetTable[font % 10];
        if (m_Position != 0)
        {
            str.resize(m_Position);
            x += g_FontManager.GetWidthA(font, str);
        }
        g_FontManager.DrawA(font, "_", color, x + DrawOffset, y + offsY);
    }
}

void CEntryText::DrawMaskW(
    uint8_t font, uint16_t color, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    const auto len = (int)Length();
    wstr_t str = {};
    for (int i = 0; i < len; i++)
    {
        str += L"*";
    }

    if (len != 0)
    {
        g_FontManager.DrawW(font, str, color, x + DrawOffset, y, 30, 0, TS_LEFT, flags);
    }

    if (this == g_EntryPointer)
    {
        if (m_Position != 0)
        {
            str.resize(m_Position);
            x += g_FontManager.GetWidthW(font, str);
        }
        g_FontManager.DrawW(font, L"_", color, x + DrawOffset, y, 30, 0, TS_LEFT, flags);
    }
}

void CEntryText::RemoveSequence(int startPos, int length)
{
    WText.erase(startPos, length);
}
