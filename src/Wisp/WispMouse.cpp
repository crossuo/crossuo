// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "WispMouse.h"
#include "WispWindow.h"
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
    // only follow the mouse while it hovers our own window; other windows in
    // the process (like the embedded launcher) have their own mouse handling,
    // and their coordinates would otherwise move the game cursor around
    if (g_WispWindow != nullptr && SDL_GetMouseFocus() != g_WispWindow->m_window)
    {
        Dragging = (LeftButtonPressed || RightButtonPressed || MidButtonPressed);
        return;
    }
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
