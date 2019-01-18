// MIT License
// Copyright (C) August 2016 Hotride

#include <SDL_clipboard.h>

#include "EntryText.h"
#include "../DefinitionMacro.h"
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
    DEBUG_TRACE_FUNCTION;
}

CEntryText::~CEntryText()
{
    DEBUG_TRACE_FUNCTION;

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

const char *CEntryText::c_str()
{
    DEBUG_TRACE_FUNCTION;

    m_CText = ToString(Text);
    return m_CText.c_str();
}

void CEntryText::OnClick(
    CGump *gump, uint8_t font, bool unicode, int x, int y, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;

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
            m_Position = g_FontManager.CalculateCaretPosW(font, Text, x, y, Width, align, flags);
        }
        else
        {
            m_Position = g_FontManager.CalculateCaretPosA(font, c_str(), x, y, Width, align, flags);
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
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;

    if (width == 0)
    {
        width = Width;
    }

    MULTILINES_FONT_INFO *info =
        g_FontManager.GetInfoA(font, c_str(), (int)Length(), align, flags, width);

    int count = 0;
    while (info != nullptr)
    {
        MULTILINES_FONT_INFO *next = info->m_Next;
        delete info;
        info = next;
        count++;
    }

    return count;
}

int CEntryText::GetLinesCountW(uint8_t font, TEXT_ALIGN_TYPE align, uint16_t flags, int width)
{
    DEBUG_TRACE_FUNCTION;

    if (width == 0)
    {
        width = Width;
    }

    MULTILINES_FONT_INFO *info =
        g_FontManager.GetInfoW(font, Data(), (int)Length(), align, flags, width);

    int count = 0;
    while (info != nullptr)
    {
        MULTILINES_FONT_INFO *next = info->m_Next;
        delete info;
        info = next;
        count++;
    }

    return count;
}

bool CEntryText::Insert(wchar_t ch, CGump *gump)
{
    DEBUG_TRACE_FUNCTION;

    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (m_Position > (int)Text.length())
    {
        m_Position = (int)Text.length();
    }

    if (MaxLength > 0)
    {
        if (NumberOnly)
        {
            wstring wstr = Text;
            wstr.insert(wstr.begin() + m_Position, ch);
            if (std::stoi(wstr) >= MaxLength)
            {
                return false;
            }
        }
        else if ((int)Text.length() >= MaxLength)
        {
            return false;
        }
    }

    Text.insert(Text.begin() + m_Position, ch);
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
    DEBUG_TRACE_FUNCTION;

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
        if (m_Position >= (int)Text.length())
        {
            return;
        }
    }

    if (m_Position < (int)Text.length())
    {
        Text.erase(Text.begin() + m_Position);
    }
    else
    {
        Text.erase(Text.length() - 1);
    }

    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::Clear()
{
    DEBUG_TRACE_FUNCTION;

    Text = {};
    m_CText = "";
    m_Position = 0;
    Changed = true;
    DrawOffset = 0;
    m_Texture.Clear();
    CaretPos.Reset();
}

void CEntryText::Paste()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    // FIXME: Move clipboard access to Wisp Window
    if (OpenClipboard(g_OrionWindow.Handle))
    {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);

        if (hData != nullptr)
        {
            wstring text((wchar_t *)GlobalLock(hData));
            CGump *gump = g_GumpManager.GetTextEntryOwner();
            if (gump != nullptr && gump->GumpType == GT_BOOK)
            {
                gump->PasteClipboardData(text);
            }
            else
            {
                for (int i = 0; i < (int)text.length(); i++)
                {
                    Insert(text[i]);
                }
            }

            GlobalUnlock(hData);
        }

        CloseClipboard();
    }
#else
    auto chBuffer = SDL_GetClipboardText();
    if (chBuffer != nullptr && (strlen(chBuffer) != 0u))
    {
        wstring str = g_EntryPointer->Data() + ToWString(chBuffer);
        g_EntryPointer->SetTextW(str);
    }
#endif
}

void CEntryText::AddPos(int val, CGump *gump)
{
    DEBUG_TRACE_FUNCTION;

    m_Position += val;
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (m_Position > (int)Text.length())
    {
        m_Position = (int)Text.length();
    }

    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::SetPos(int val, CGump *gump)
{
    DEBUG_TRACE_FUNCTION;

    m_Position = val;
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (m_Position > (int)Text.length())
    {
        m_Position = (int)Text.length();
    }

    Changed = true;
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

void CEntryText::SetTextA(const string &text)
{
    DEBUG_TRACE_FUNCTION;
    wstring wtext = ToWString(text);
    SetTextW(wtext);
    m_CText = text;
}

void CEntryText::SetTextW(const wstring &text)
{
    DEBUG_TRACE_FUNCTION;
    Clear();

    Text = text;
    m_Position = (int)Text.length();
    if (m_Position < 0)
    {
        m_Position = 0;
    }

    if (MaxLength > 0)
    {
        if (NumberOnly)
        {
            string str = ToString(Text);
            while (true)
            {
                size_t len = str.length();

                if (std::atoi(str.c_str()) >= MaxLength && len > 0)
                {
                    str.resize(len - 1);
                }
                else
                {
                    break;
                }
            }
        }
        else if ((int)Text.length() >= MaxLength)
        {
            Text.resize(MaxLength);
        }
    }

    CGump *gump = g_GumpManager.GetTextEntryOwner();
    if (gump != nullptr)
    {
        gump->FrameCreated = false;
    }
}

string CEntryText::CheckMaxWidthA(uint8_t font, string str)
{
    DEBUG_TRACE_FUNCTION;

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

wstring CEntryText::CheckMaxWidthW(uint8_t font, wstring str)
{
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;

    c_str();
    if (MaxWidth <= 0)
    {
        return;
    }

    int width = g_FontManager.GetWidthA(font, m_CText);
    size_t len = m_CText.length();
    while (MaxWidth < width && len > 0)
    {
        Remove((m_Position > 0), nullptr);
        len--;
        width = g_FontManager.GetWidthA(font, c_str());
    }
}

void CEntryText::FixMaxWidthW(uint8_t font)
{
    DEBUG_TRACE_FUNCTION;

    if (MaxWidth <= 0)
    {
        return;
    }

    int width = g_FontManager.GetWidthW(font, Text);
    size_t len = Text.length();
    while (MaxWidth < width && len > 0)
    {
        Remove((m_Position > 0), nullptr);
        len--;
        width = g_FontManager.GetWidthW(font, Text);
    }
}

void CEntryText::CreateTextureA(
    uint8_t font,
    const string &str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;

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
    const wstring &str,
    uint16_t color,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;

    if (Changed || Color != color)
    {
        FixMaxWidthA(font);
        CreateTextureA(font, m_CText, color, /*MaxWidth*/ Width, align, flags);
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
    DEBUG_TRACE_FUNCTION;

    if (Changed || Color != color)
    {
        FixMaxWidthW(font);
        CreateTextureW(font, Text, color, Width, align, flags);
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
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;

    const auto len = (int)Length();
    string str{};
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
    DEBUG_TRACE_FUNCTION;

    const auto len = (int)Length();
    wstring str = {};
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
    Text.erase(startPos, length);
}

string CEntryText::GetTextA() const
{
    return m_CText;
}

wstring CEntryText::GetTextW() const
{
    return Text;
}
