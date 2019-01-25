#ifndef WISP_MOUSE_H
#define WISP_MOUSE_H

#include <SDL_events.h>

namespace Wisp
{
class CMouse
{
public:
    CPoint2Di RealPosition = CPoint2Di();
    CPoint2Di Position = CPoint2Di();
    CPoint2Di LeftDropPosition = CPoint2Di();
    CPoint2Di RightDropPosition = CPoint2Di();
    CPoint2Di MidDropPosition = CPoint2Di();

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

    CPoint2Di LeftDroppedOffset();
    CPoint2Di RightDroppedOffset();
    CPoint2Di MidDroppedOffset();
};

extern CMouse *g_WispMouse;

}; // namespace Wisp

#endif // WISP_MOUSE_H
