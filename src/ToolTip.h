// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <string>
#include "Point.h"
#include "Sprite.h"

class CToolTip
{
public:
    uint32_t Timer = 0;
    std::wstring Data = {};
    uint32_t ClilocID = 0;
    int MaxWidth = 0;
    CPoint2Di Position = CPoint2Di();
    bool Use = false;
    CTextSprite m_TextSprite;
    class CRenderObject *m_Object{ nullptr };

    CToolTip();
    ~CToolTip();

    void Reset();
    void Create(CTextSprite &texture, const std::wstring &str, int &width, int minWidth);
    void Set(const std::wstring &str, int maxWidth = 0);
    void Set(int clilocID, const std::string &str, int maxWidth = 0, bool toCamelCase = false);
    void Draw(int cursorWidth = 0, int cursorHeight = 0);
};

extern CToolTip g_ToolTip;
