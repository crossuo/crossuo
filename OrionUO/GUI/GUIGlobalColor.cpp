// MIT License
// Copyright (C) December 2016 Hotride

#include "GUIGlobalColor.h"

CGUIGlobalColor::CGUIGlobalColor(bool enabled, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
    : CBaseGUI(GOT_GLOBAL_COLOR, 0, 0, 0, 0, 0)
    , R(r)
    , G(g)
    , B(b)
    , A(a)
{
    Enabled = enabled;
}

CGUIGlobalColor::~CGUIGlobalColor()
{
}

void CGUIGlobalColor::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Enabled)
    {
        glColor4f(R, G, B, A);
    }
}
