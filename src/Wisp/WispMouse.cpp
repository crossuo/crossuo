
#include "WispMouse.h"
#include <SDL_mouse.h>
namespace Wisp
{
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
    SDL_GetMouseState(&Position.X, &Position.Y);
    Dragging = (LeftButtonPressed || RightButtonPressed || MidButtonPressed);
    RealPosition = Position;
}

void CMouse::Release() const
{
    if (!(LeftButtonPressed || RightButtonPressed || MidButtonPressed))
    {
        SDL_CaptureMouse(SDL_FALSE);
    }
}

void CMouse::Capture() const
{
    SDL_CaptureMouse(SDL_TRUE);
}

CPoint2Di CMouse::LeftDroppedOffset()
{
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
    CPoint2Di position;

    if (MidButtonPressed)
    {
        position.X = RealPosition.X - MidDropPosition.X;
        position.Y = RealPosition.Y - MidDropPosition.Y;
    }

    return position;
}

}; // namespace Wisp
