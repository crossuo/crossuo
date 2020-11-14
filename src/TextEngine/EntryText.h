// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../Point.h"
#include "../Sprite.h"
#include "../Platform.h" // REMOVE Keycode
#include <xuocore/enumlist.h>

class CGump;

class CEntryText
{
public:
    int MaxLength = 0;
    int Width = 0;
    int MaxWidth = 0;
    bool Changed = true;
    uint16_t Color = 0;
    int DrawOffset = 0;
    CPoint2Di CaretPos = {};
    bool NumberOnly = false;

protected:
    int m_Position = 0;
    wstr_t WText;

public:
    CEntryText(int maxLength = 0, int width = 0, int maxWidth = 0, bool numberOnly = false);
    virtual ~CEntryText();

    CTextSprite m_Texture;
    CTextSprite m_CaretTexture;

    void Clear();
    void Paste();

    int GetLinesCountA(
        uint8_t font, TEXT_ALIGN_TYPE align = TS_LEFT, uint16_t flags = 0, int width = 0);

    int GetLinesCountW(
        uint8_t font, TEXT_ALIGN_TYPE align = TS_LEFT, uint16_t flags = 0, int width = 0);

    virtual bool Insert(char16_t ch, CGump *gump = nullptr);
    void Remove(bool left, CGump *gump = nullptr);
    void RemoveSequence(int startPos, int length);
    void AddPos(int val, CGump *gump = nullptr);
    void SetPos(int val, CGump *gump = nullptr);
    void SetTextA(const astr_t &text);
    void SetTextW(const wstr_t &text);
    inline astr_t GetTextA() { return str_from(WText); }

    inline const wstr_t &GetTextW() const { return WText; }

    void OnClick(
        CGump *gump,
        uint8_t font,
        bool unicode,
        int x,
        int y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void OnKey(CGump *gump, Keycode key);
    int Pos() const { return m_Position; }
    size_t Length() const { return WText.length(); }
    astr_t CheckMaxWidthA(uint8_t font, astr_t str);
    wstr_t CheckMaxWidthW(uint8_t font, wstr_t str);
    void FixMaxWidthA(uint8_t font);
    void FixMaxWidthW(uint8_t font);

    void CreateTextureA(
        uint8_t font,
        const astr_t &str,
        uint16_t color,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    void CreateTextureW(
        uint8_t font,
        const wstr_t &str,
        uint16_t color,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    virtual void PrepareToDrawA(
        uint8_t font, uint16_t color, TEXT_ALIGN_TYPE align = TS_LEFT, uint16_t flags = 0);

    virtual void PrepareToDrawW(
        uint8_t font, uint16_t color, TEXT_ALIGN_TYPE align = TS_LEFT, uint16_t flags = 0);

    virtual void DrawA(
        uint8_t font,
        uint16_t color,
        int x,
        int y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    virtual void DrawW(
        uint8_t font,
        uint16_t color,
        int x,
        int y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void DrawMaskA(
        uint8_t font,
        uint16_t color,
        int X,
        int Y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void DrawMaskW(
        uint8_t font,
        uint16_t color,
        int X,
        int Y,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);
};

extern CEntryText *g_EntryPointer;
