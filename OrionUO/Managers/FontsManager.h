// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

typedef struct MULTILINES_FONT_DATA
{
    wchar_t item;
    ushort flags;
    uchar font;
    ushort linkID;
    uint color;

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
    //!Была посещена
    bool Visited;

    //!Ссылка
    string WebLink;
};

struct HTML_char
{
    //!Индекс символа
    wchar_t Char;

    //!Индекс шрифта
    uchar Font;

    //!Выравнивание
    TEXT_ALIGN_TYPE Align;

    //!Набор флагов
    ushort Flags;

    //!Цвет символа
    uint Color;

    //!Индекс ссылки
    ushort LinkID;
};

struct HTML_DATA_INFO
{
    HTML_TAG_TYPE Tag;
    TEXT_ALIGN_TYPE Align;
    ushort Flags;
    uchar Font;
    uint Color;
    ushort Link;
};

typedef map<ushort, WEB_LINK> WEBLINK_MAP;
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

    static uchar m_FontIndex[256];

    size_t m_UnicodeFontAddress[20];
    uint m_UnicodeFontSize[20];

    bool m_UseHTML = false;
    uint m_HTMLColor = 0xFFFFFFFF;
    bool m_HTMLBackgroundCanBeColored = false;

    uint m_BackgroundColor = 0;
    uint m_WebLinkColor = 0;
    uint m_VisitedWebLinkColor = 0;

    int m_LeftMargin = 0;
    int m_TopMargin = 0;
    int m_RightMargin = 0;
    int m_BottomMargin = 0;

    ushort GetWebLinkID(const string &link, uint &color);
    ushort GetWebLinkID(const wstring &link, uint &color);

    HTMLCHAR_LIST
    GetHTMLData(uchar font, const wchar_t *str, int &len, TEXT_ALIGN_TYPE align, ushort flags);

    HTML_DATA_INFO GetHTMLInfoFromTag(const HTML_TAG_TYPE &tag);
    HTML_DATA_INFO GetCurrentHTMLInfo(const HTMLINFO_LIST &list);

    void GetHTMLInfoFromContent(HTML_DATA_INFO &info, const string &content);
    void TrimHTMLString(string &str);
    uint GetHTMLColorFromText(string &str);

    HTML_TAG_TYPE
    ParseHTMLTag(const wchar_t *str, int len, int &i, bool &endTag, HTML_DATA_INFO &info);

    HTMLCHAR_LIST
    GetHTMLDataOld(uchar font, const wchar_t *str, int &len, TEXT_ALIGN_TYPE align, ushort flags);

    PMULTILINES_FONT_INFO GetInfoHTML(
        uchar font, const wchar_t *str, int len, TEXT_ALIGN_TYPE align, ushort flags, int width);

    bool GenerateABase(
        uchar font,
        CGLTextTexture &th,
        const string &str,
        ushort color,
        int width,
        TEXT_ALIGN_TYPE align,
        ushort flags);

    bool GenerateWBase(
        uchar font,
        CGLTextTexture &th,
        const wstring &str,
        ushort color,
        uchar cell,
        int width,
        TEXT_ALIGN_TYPE align,
        ushort flags);

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
    bool UnicodeFontExists(uchar font);
    void GoToWebLink(ushort link);

    inline bool IsPrintASCII(uchar index) { return (m_FontIndex[index] != 0xFF); }
    int GetFontOffsetY(uchar font, uchar index);
    Wisp::CPoint2Di GetCaretPosA(
        uchar font, const string &str, int pos, int width, TEXT_ALIGN_TYPE align, ushort flags);

    int CalculateCaretPosA(
        uchar font,
        const string &str,
        int x,
        int y,
        int width,
        TEXT_ALIGN_TYPE align,
        ushort flags);

    int GetWidthA(uchar font, const string &str);

    int
    GetWidthExA(uchar font, const string &str, int maxWidth, TEXT_ALIGN_TYPE align, ushort flags);

    int GetHeightA(
        uchar font,
        const string &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        ushort flags = 0);

    int GetHeightA(PMULTILINES_FONT_INFO info);

    string GetTextByWidthA(uchar font, const string &str, int width, bool isCropped);

    PMULTILINES_FONT_INFO
    GetInfoA(uchar font, const char *str, int len, TEXT_ALIGN_TYPE align, ushort flags, int width);

    vector<uint32_t> GeneratePixelsA(
        uchar font,
        CGLTextTexture &th,
        const char *str,
        ushort color,
        int width,
        TEXT_ALIGN_TYPE align,
        ushort flags);

    bool GenerateA(
        uchar font,
        CGLTextTexture &th,
        const string &str,
        ushort color = 0,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        ushort flags = 0);

    void DrawA(
        uchar font,
        const string &str,
        ushort color,
        int x,
        int y,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        ushort flags = 0);

    Wisp::CPoint2Di GetCaretPosW(
        uchar font, const wstring &str, int pos, int width, TEXT_ALIGN_TYPE align, ushort flags);

    int CalculateCaretPosW(
        uchar font,
        const wstring &str,
        int x,
        int y,
        int width,
        TEXT_ALIGN_TYPE align,
        ushort flags);

    int GetWidthW(uchar font, const wstring &str);

    int
    GetWidthExW(uchar font, const wstring &str, int maxWidth, TEXT_ALIGN_TYPE align, ushort flags);

    int GetHeightW(
        uchar font,
        const wstring &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        ushort flags = 0);

    int GetHeightW(PMULTILINES_FONT_INFO info);

    wstring GetTextByWidthW(uchar font, const wstring &str, int width, bool isCropped);

    PMULTILINES_FONT_INFO GetInfoW(
        uchar font, const wchar_t *str, int len, TEXT_ALIGN_TYPE align, ushort flags, int width);

    vector<uint32_t> GeneratePixelsW(
        uchar font,
        CGLTextTexture &th,
        const wchar_t *str,
        ushort color,
        uchar cell,
        int width,
        TEXT_ALIGN_TYPE align,
        ushort flags);

    bool GenerateW(
        uchar font,
        CGLTextTexture &th,
        const wstring &str,
        ushort color = 0,
        uchar cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        ushort flags = 0);

    void DrawW(
        uchar font,
        const wstring &str,
        ushort color,
        int x,
        int y,
        uchar cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        ushort flags = 0);
};

extern CFontsManager g_FontManager;
