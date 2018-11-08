
#pragma once

#include <SDL_events.h>

namespace Wisp
{
class CMouse
{
public:
    Wisp::CPoint2Di RealPosition = Wisp::CPoint2Di();
    Wisp::CPoint2Di Position = Wisp::CPoint2Di();
    Wisp::CPoint2Di LeftDropPosition = Wisp::CPoint2Di();
    Wisp::CPoint2Di RightDropPosition = Wisp::CPoint2Di();
    Wisp::CPoint2Di MidDropPosition = Wisp::CPoint2Di();

    bool LeftButtonPressed = false;
    bool RightButtonPressed = false;
    bool MidButtonPressed = false;
    bool Dragging = false;
    int DoubleClickDelay = 350;
    bool CancelDoubleClick = false;

    uint32_t LastLeftButtonClickTimer = 0;
    uint32_t LastRightButtonClickTimer = 0;
    uint32_t LastMidButtonClickTimer = 0;

    CMouse();
    virtual ~CMouse();

    void Update();
    void Release() const;
    void Capture() const;

    Wisp::CPoint2Di LeftDroppedOffset();
    Wisp::CPoint2Di RightDroppedOffset();
    Wisp::CPoint2Di MidDroppedOffset();
};

extern CMouse *g_WispMouse;

};
