// MIT License
// Copyright (C) September 2016 Hotride

#include "GUIWorldMapTexture.h"
#include "../Managers/MouseManager.h"
#include "Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGUIWorldMapTexture::CGUIWorldMapTexture(int x, int y)
    : CBaseGUI(GOT_EXTERNALTEXTURE, 0, 0, 0, x, y)
{
}

CGUIWorldMapTexture::~CGUIWorldMapTexture()
{
}

void CGUIWorldMapTexture::Draw(bool checktrans)
{
    if (g_MapTexture[Index].Texture != 0)
    {
#ifndef NEW_RENDERER_ENABLED
        CGLTexture tex;
        tex.Texture = g_MapTexture[Index].Texture;
        tex.Width = Width;
        tex.Height = Height;

        g_GL.GL1_Draw(tex, m_X + OffsetX, m_Y + OffsetY);

        tex.Texture = 0;
#else
        auto cmd =
            DrawQuadCmd(g_MapTexture[Index].Texture, m_X + OffsetX, m_Y + OffsetY, Width, Height);
        RenderAdd_DrawQuad(g_renderCmdList, &cmd, 1);
#endif

        //g_MapTexture[m_Index].Draw(m_X + m_OffsetX, m_Y + m_OffsetY, Width, Height, checktrans);
    }
}

bool CGUIWorldMapTexture::Select()
{
    bool select = false;

    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    if (x >= 0 && y >= 0 && x < Width && y < Height)
    {
        select = true;
    }

    return select;
}
