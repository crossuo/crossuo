// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

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
