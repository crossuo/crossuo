// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIResizepic.h"
#include "../OrionUO.h"

CGUIResizepic::CGUIResizepic(int serial, uint16_t graphic, int x, int y, int width, int height)
    : CGUIPolygonal(GOT_RESIZEPIC, x, y, width, height)
{
    Serial = serial;
    Graphic = graphic;
}

CGUIResizepic::~CGUIResizepic()
{
}

void CGUIResizepic::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.ExecuteResizepic(Graphic);
}

void CGUIResizepic::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th[9] = { nullptr };

    for (int i = 0; i < 9; i++)
    {
        CGLTexture *pth = g_Orion.ExecuteGump(Graphic + (int)i);

        if (pth == nullptr)
        {
            return;
        }

        if (i == 4)
        {
            th[8] = pth;
        }
        else if (i > 4)
        {
            th[i - 1] = pth;
        }
        else
        {
            th[i] = pth;
        }
    }

    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);

    if (checktrans)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        g_GL_DrawResizepic(th, m_X, m_Y, Width, Height);

        glDisable(GL_BLEND);

        glEnable(GL_STENCIL_TEST);

        g_GL_DrawResizepic(th, m_X, m_Y, Width, Height);

        glDisable(GL_STENCIL_TEST);
    }
    else
    {
        g_GL_DrawResizepic(th, m_X, m_Y, Width, Height);
    }
}

bool CGUIResizepic::Select()
{
    DEBUG_TRACE_FUNCTION;
    if (CheckPolygone)
    {
        return CGUIPolygonal::Select();
    }

    return g_Orion.ResizepicPixelsInXY(Graphic, m_X, m_Y, Width, Height);
}
