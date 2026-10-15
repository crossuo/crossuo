// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>

class CQuestArrow
{
    int m_ScreenX = 0;
    int m_ScreenY = 0;
    int m_Width = 0;
    int m_Height = 0;

public:
    uint32_t Timer = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    bool Enabled = false;

    CQuestArrow();
    ~CQuestArrow();

    static const uint16_t m_Gump = 0x1194;
    void Draw();
    // true when the last Draw() placed the arrow under the given screen point
    bool Contains(int x, int y) const;
};

extern CQuestArrow g_QuestArrow;
