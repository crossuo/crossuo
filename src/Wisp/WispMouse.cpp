

#include <SDL_mouse.h>
namespace Wisp
{
#if defined(_MSC_VER)
#pragma warning(disable : 4800)
#endif

CMouse *g_WispMouse = nullptr;

CMouse::CMouse()
{
    g_WispMouse = this;
}

CMouse::~CMouse()
{
    g_WispMouse = nullptr;
}

void CMouse::Update()
{
    DEBUG_TRACE_FUNCTION;
    SDL_GetMouseState(&Position.X, &Position.Y);
    Dragging = (LeftButtonPressed || RightButtonPressed || MidButtonPressed);
    RealPosition = Position;
}

void CMouse::Release() const
{
    DEBUG_TRACE_FUNCTION;
    if (!(LeftButtonPressed || RightButtonPressed || MidButtonPressed))
    {
        SDL_CaptureMouse(SDL_FALSE);
    }
}

void CMouse::Capture() const
{
    DEBUG_TRACE_FUNCTION;
    SDL_CaptureMouse(SDL_TRUE);
}

CPoint2Di CMouse::LeftDroppedOffset()
{
    DEBUG_TRACE_FUNCTION;
    CPoint2Di position;

    if (LeftButtonPressed)
    {
        position.X = RealPosition.X - LeftDropPosition.X;
        position.Y = RealPosition.Y - LeftDropPosition.Y;
    }

    return position;
}

CPoint2Di CMouse::RightDroppedOffset()
{
    DEBUG_TRACE_FUNCTION;
    CPoint2Di position;

    if (RightButtonPressed)
    {
        position.X = RealPosition.X - RightDropPosition.X;
        position.Y = RealPosition.Y - RightDropPosition.Y;
    }

    return position;
}

CPoint2Di CMouse::MidDroppedOffset()
{
    DEBUG_TRACE_FUNCTION;
    CPoint2Di position;

    if (MidButtonPressed)
    {
        position.X = RealPosition.X - MidDropPosition.X;
        position.Y = RealPosition.Y - MidDropPosition.Y;
    }

    return position;
}

}; // namespace Wisp
