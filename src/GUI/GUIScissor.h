// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGump;

class CGUIScissor : public CGUIPolygonal
{
    //! X coordinate of the component inside the container the element lives in, relative to the gump origin
    int BaseX = 0;

    //! Y coordinate of the component inside the container the element lives in, relative to the gump origin
    int BaseY = 0;

public:
    //! Parent gump
    CGump *GumpParent = nullptr;

    CGUIScissor(
        bool enabled,
        int baseX = 0,
        int baseY = 0,
        int x = 0,
        int y = 0,
        int width = 0,
        int height = 0);
    virtual ~CGUIScissor();

    virtual void Draw(bool checktrans = false);

    //!Push a scissor box authored in the current gump's local coordinates,
    //!translating it into whatever space the gump pass draws items in (the
    //!framebuffer pass draws gump-local, the direct pass draws in window
    //!space). Use this instead of Render_PushScissor from gump item code, or
    //!the box lands in the wrong space whenever the direct pass is active.
    static void PushGumpLocal(int x, int y, int width, int height);
};
