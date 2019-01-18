// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIBlending.h"

CGUIBlending::CGUIBlending(bool enabled, GLenum sFactor, GLenum dFactor)
    : CBaseGUI(GOT_BLENDING, 0, 0, 0, 0, 0)
    , SFactor(sFactor)
    , DFactor(dFactor)
{
    Enabled = enabled;
}

CGUIBlending::~CGUIBlending()
{
}

void CGUIBlending::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Enabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(SFactor, DFactor);
    }
    else
    {
        glDisable(GL_BLEND);
    }
}
