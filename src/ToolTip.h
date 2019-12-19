// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <common/str.h>
#include "Point.h"
#include "Sprite.h"

class CToolTip
{
public:
    uint32_t Timer = 0;
    wstr_t Data = {};
    uint32_t ClilocID = 0;
    int MaxWidth = 0;
    CPoint2Di Position = CPoint2Di();
    bool Use = false;
    CTextSprite m_TextSprite;
    class CRenderObject *m_Object{ nullptr };

    CToolTip();
    ~CToolTip();

    void Reset();
    void Create(CTextSprite &texture, const wstr_t &str, int &width, int minWidth);
    void Set(const wstr_t &str, int maxWidth = 0);
    void Set(int clilocID, const astr_t &str, int maxWidth = 0, bool toCamelCase = false);
    void Draw(int cursorWidth = 0, int cursorHeight = 0);
};

extern CToolTip g_ToolTip;
