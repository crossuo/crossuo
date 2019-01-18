// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../plugin/mulstruct.h"

typedef struct MULTILINES_FONT_DATA
{
    wchar_t item;
    uint16_t flags;
    uint8_t font;
    uint16_t linkID;
    uint32_t color;

    MULTILINES_FONT_DATA *Next;
} * PMULTILINES_FONT_DATA;

typedef struct MULTILINES_FONT_INFO
{
    int Width;
    int IndentionOffset;
    int MaxHeight;
    int CharStart;
    int CharCount;
    TEXT_ALIGN_TYPE Align;
    vector<MULTILINES_FONT_DATA> Data;

    MULTILINES_FONT_INFO *m_Next;

    void Reset()
    {
        Width = 0;
        IndentionOffset = 0;
        MaxHeight = 0;
        CharStart = 0;
        CharCount = 0;
        Align = TS_LEFT;
        m_Next = nullptr;
    }
} * PMULTILINES_FONT_INFO;

struct WEB_LINK
{
    bool Visited;
    string WebLink;
};

struct HTML_char
{
    wchar_t Char;
    uint8_t Font;
    TEXT_ALIGN_TYPE Align;
    uint16_t Flags;
    uint32_t Color;
    uint16_t LinkID;
};

struct HTML_DATA_INFO
{
    HTML_TAG_TYPE Tag;
    TEXT_ALIGN_TYPE Align;
    uint16_t Flags;
    uint8_t Font;
    uint32_t Color;
    uint16_t Link;
};

typedef map<uint16_t, WEB_LINK> WEBLINK_MAP;
typedef vector<HTML_char> HTMLCHAR_LIST;
typedef vector<HTML_DATA_INFO> HTMLINFO_LIST;

class CFontsManager
{
public:
    bool RecalculateWidthByInfo = false;
    bool UnusePartialHue = false;
    int FontCount = 0;

private:
    FONT_DATA *Font = nullptr;
    WEBLINK_MAP m_WebLink;

    static uint8_t m_FontIndex[256];

    size_t m_UnicodeFontAddress[20];
    uint32_t m_UnicodeFontSize[20];

    bool m_UseHTML = false;
    uint32_t m_HTMLColor = 0xFFFFFFFF;
    bool m_HTMLBackgroundCanBeColored = false;

    uint32_t m_BackgroundColor = 0;
    uint32_t m_WebLinkColor = 0;
    uint32_t m_VisitedWebLinkColor = 0;

    int m_LeftMargin = 0;
    int m_TopMargin = 0;
    int m_RightMargin = 0;
    int m_BottomMargin = 0;

    uint16_t GetWebLinkID(const string &link, uint32_t &color);
    uint16_t GetWebLinkID(const wstring &link, uint32_t &color);

    HTMLCHAR_LIST
    GetHTMLData(uint8_t font, const wchar_t *str, int &len, TEXT_ALIGN_TYPE align, uint16_t flags);

    HTML_DATA_INFO GetHTMLInfoFromTag(const HTML_TAG_TYPE &tag);
    HTML_DATA_INFO GetCurrentHTMLInfo(const HTMLINFO_LIST &list);

    void GetHTMLInfoFromContent(HTML_DATA_INFO &info, const string &content);
    void TrimHTMLString(string &str);
    uint32_t GetHTMLColorFromText(string &str);

    HTML_TAG_TYPE
    ParseHTMLTag(const wchar_t *str, int len, int &i, bool &endTag, HTML_DATA_INFO &info);

    HTMLCHAR_LIST
    GetHTMLDataOld(
        uint8_t font, const wchar_t *str, int &len, TEXT_ALIGN_TYPE align, uint16_t flags);

    PMULTILINES_FONT_INFO GetInfoHTML(
        uint8_t font,
        const wchar_t *str,
        int len,
        TEXT_ALIGN_TYPE align,
        uint16_t flags,
        int width);

    bool GenerateABase(
        uint8_t font,
        CGLTextTexture &th,
        const string &str,
        uint16_t color,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    bool GenerateWBase(
        uint8_t font,
        CGLTextTexture &th,
        const wstring &str,
        uint16_t color,
        uint8_t cell,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

public:
    CFontsManager();
    ~CFontsManager();

    void
    SetUseHTML(bool val, int htmlStartColor = 0xFFFFFFFF, const bool backgroundCanBeColored = false)
    {
        m_UseHTML = val;
        m_HTMLColor = htmlStartColor;
        m_HTMLBackgroundCanBeColored = backgroundCanBeColored;
    }
    bool GetUseHTML() const { return m_UseHTML; }

    bool LoadFonts();
    bool UnicodeFontExists(uint8_t font);
    void GoToWebLink(uint16_t link);

    inline bool IsPrintASCII(uint8_t index) { return (m_FontIndex[index] != 0xFF); }
    int GetFontOffsetY(uint8_t font, uint8_t index);
    CPoint2Di GetCaretPosA(
        uint8_t font, const string &str, int pos, int width, TEXT_ALIGN_TYPE align, uint16_t flags);

    int CalculateCaretPosA(
        uint8_t font,
        const string &str,
        int x,
        int y,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    int GetWidthA(uint8_t font, const string &str);

    int GetWidthExA(
        uint8_t font, const string &str, int maxWidth, TEXT_ALIGN_TYPE align, uint16_t flags);

    int GetHeightA(
        uint8_t font,
        const string &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    int GetHeightA(PMULTILINES_FONT_INFO info);

    string GetTextByWidthA(uint8_t font, const string &str, int width, bool isCropped);

    PMULTILINES_FONT_INFO
    GetInfoA(
        uint8_t font, const char *str, int len, TEXT_ALIGN_TYPE align, uint16_t flags, int width);

    vector<uint32_t> GeneratePixelsA(
        uint8_t font,
        CGLTextTexture &th,
        const char *str,
        uint16_t color,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    bool GenerateA(
        uint8_t font,
        CGLTextTexture &th,
        const string &str,
        uint16_t color = 0,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void DrawA(
        uint8_t font,
        const string &str,
        uint16_t color,
        int x,
        int y,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    CPoint2Di GetCaretPosW(
        uint8_t font,
        const wstring &str,
        int pos,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    int CalculateCaretPosW(
        uint8_t font,
        const wstring &str,
        int x,
        int y,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    int GetWidthW(uint8_t font, const wstring &str);

    int GetWidthExW(
        uint8_t font, const wstring &str, int maxWidth, TEXT_ALIGN_TYPE align, uint16_t flags);

    int GetHeightW(
        uint8_t font,
        const wstring &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    int GetHeightW(PMULTILINES_FONT_INFO info);

    wstring GetTextByWidthW(uint8_t font, const wstring &str, int width, bool isCropped);

    PMULTILINES_FONT_INFO GetInfoW(
        uint8_t font,
        const wchar_t *str,
        int len,
        TEXT_ALIGN_TYPE align,
        uint16_t flags,
        int width);

    vector<uint32_t> GeneratePixelsW(
        uint8_t font,
        CGLTextTexture &th,
        const wchar_t *str,
        uint16_t color,
        uint8_t cell,
        int width,
        TEXT_ALIGN_TYPE align,
        uint16_t flags);

    bool GenerateW(
        uint8_t font,
        CGLTextTexture &th,
        const wstring &str,
        uint16_t color = 0,
        uint8_t cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void DrawW(
        uint8_t font,
        const wstring &str,
        uint16_t color,
        int x,
        int y,
        uint8_t cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);
};

extern CFontsManager g_FontManager;
