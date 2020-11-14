// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>

class CQuestArrow
{
public:
    uint32_t Timer = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    bool Enabled = false;

    CQuestArrow();
    ~CQuestArrow();

    static const uint16_t m_Gump = 0x1194;
    void Draw();
};

extern CQuestArrow g_QuestArrow;
