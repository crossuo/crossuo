// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../Globals.h"
#include "../Point.h"
#include <stdint.h>

struct CRenderTexture;

extern uint16_t g_CursorData[2][16];

class CMouseManager
{
public:
    CPoint2Di RealPosition = CPoint2Di();
    CPoint2Di Position = CPoint2Di();
    CPoint2Di LeftDropPosition = CPoint2Di();

    bool LeftButtonPressed = false;
    bool RightButtonPressed = false;
    bool MidButtonPressed = false;
    bool Dragging = false;
    int DoubleClickDelay = 350;
    bool CancelDoubleClick = false;

    uint32_t LastLeftButtonClickTimer = 0;
    uint32_t LastRightButtonClickTimer = 0;
    uint32_t LastMidButtonClickTimer = 0;

private:
    int m_CursorOffset[2][16];
    bool m_Hidden = false;

public:
    CMouseManager() { memset(&m_CursorOffset[0][0], 0, sizeof(m_CursorOffset)); }
    ~CMouseManager() = default;

    void Update();
    void Release() const;
    void Capture() const;

    CPoint2Di LeftDroppedOffset() const;

    void Hide(bool hide = true) { m_Hidden = hide; };
    bool LoadCursorTextures();
    int Sgn(int val);
    int GetFacing(int x1, int y1, int to_x, int to_y, int current_facing);
    uint16_t GetGameCursor();
    void ProcessWalking();
    void Draw(uint16_t id);
    // Overlay mirror support (the assistant draws the cursor above its UI):
    // resolves the rect Draw() uses for the cursor sprite of id - top-left
    // position (Position plus the hotspot offset) and the sprite size - plus
    // the source CRenderTexture for a pixel mirror. false when Draw() would render
    // nothing for this id (unknown cursor index or the art has no texture).
    bool GetCursorDrawRect(uint16_t id, int *x, int *y, int *w, int *h, CRenderTexture **texture);
};

extern CMouseManager g_MouseManager;
