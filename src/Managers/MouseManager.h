// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Wisp.h"

struct CGLTexture;

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
    // Overlay mirror support (the assistant draws the cursor above its UI):
    // resolves the rect Draw() uses for the cursor sprite of id - top-left
    // position (Position plus the hotspot offset) and the sprite size - plus
    // the source CGLTexture for a pixel mirror. false when Draw() would render
    // nothing for this id (unknown cursor index or the art has no texture).
    bool GetCursorDrawRect(uint16_t id, int *x, int *y, int *w, int *h, CGLTexture **texture);
};

extern CMouseManager g_MouseManager;
