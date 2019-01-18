// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CToolTip
{
public:
    uint32_t Timer = 0;
    wstring Data = {};
    uint32_t ClilocID = 0;
    int MaxWidth = 0;
    CPoint2Di Position = CPoint2Di();
    bool Use = false;
    CGLTextTexture Texture;
    class CRenderObject *m_Object{ nullptr };

    CToolTip();
    ~CToolTip();

    void Reset();
    void CreateTextTexture(CGLTextTexture &texture, const wstring &str, int &width, int minWidth);
    void Set(const wstring &str, int maxWidth = 0);
    void Set(int clilocID, const string &str, int maxWidth = 0, bool toCamelCase = false);
    void Draw(int cursorWidth = 0, int cursorHeight = 0);
};

extern CToolTip g_ToolTip;
