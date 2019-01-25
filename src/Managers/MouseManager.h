// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

extern uint16_t g_CursorData[2][16];

class CMouseManager : public Wisp::CMouse
{
private:
    int m_CursorOffset[2][16];

public:
    CMouseManager()
        : Wisp::CMouse()
    {
        memset(&m_CursorOffset[0][0], 0, sizeof(m_CursorOffset));
    }
    ~CMouseManager() {}

    bool LoadCursorTextures();
    int Sgn(int val);
    int GetFacing(int x1, int y1, int to_x, int to_y, int current_facing);
    uint16_t GetGameCursor();
    void ProcessWalking();
    void Draw(uint16_t id);
};

extern CMouseManager g_MouseManager;
