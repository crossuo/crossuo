// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIWorldMapTexture.h"
#include "../Managers/MouseManager.h"
#include "../Renderer/RenderAPI.h"
#include "../Renderer/RenderTexture.h" // g_MapTexture full type
#include "../Utility/PerfMarker.h"
#include "../Globals.h" // g_MapTexture

CGUIWorldMapTexture::CGUIWorldMapTexture(int x, int y)
    : CBaseGUI(GOT_EXTERNALTEXTURE, 0, 0, 0, x, y)
{
}

CGUIWorldMapTexture::~CGUIWorldMapTexture()
= default;

void CGUIWorldMapTexture::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    if (g_MapTexture[Index].Texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        auto cmd = DrawQuadCmd{ g_MapTexture[Index].Texture,
                                m_X + OffsetX,
                                m_Y + OffsetY,
                                uint32_t(Width),
                                uint32_t(Height) };
        RenderAdd_DrawQuad(g_renderCmdList, cmd);

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
