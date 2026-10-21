// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIExternalTexture.h"
#include "../Managers/ColorManager.h"
#include "../Point.h"
#include "../Globals.h"
#include "../Sprite.h"
#include "../Renderer/RenderAPI.h"
#include "../Renderer/RenderTexture.h" // g_AuraTexture full type
#include "../Utility/PerfMarker.h"
#include "GUI/BaseGUI.h"

CGUIExternalTexture::CGUIExternalTexture(
    CSprite *sprite, bool deleteTextureOnDestroy, int x, int y, int drawWidth, int drawHeight)
    : CBaseGUI(GOT_EXTERNALTEXTURE, 0, 0, 0, x, y)
    , DeleteTextureOnDestroy(deleteTextureOnDestroy)
    , DrawWidth(drawWidth)
    , DrawHeight(drawHeight)
    , m_Sprite(sprite)
{
}

CGUIExternalTexture::~CGUIExternalTexture()
{
    if (DeleteTextureOnDestroy)
    {
        if (m_Sprite)
        {
            delete m_Sprite;
        }
        m_Sprite = nullptr;
    }
}

CSize CGUIExternalTexture::GetSize()
{
    CSize size;
    if (m_Sprite != nullptr)
    {
        if (DrawWidth != 0)
        {
            size.Width = DrawWidth;
        }
        else
        {
            size.Width = m_Sprite->Width;
        }

        if (DrawHeight != 0)
        {
            size.Height = DrawHeight;
        }
        else
        {
            size.Height = m_Sprite->Height;
        }
    }

    return size;
}

void CGUIExternalTexture::SetShaderMode()
{
    if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{uniformValue});

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{SDM_NO_COLOR});
    }
}

void CGUIExternalTexture::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    if (m_Sprite != nullptr)
    {
        SetShaderMode();
        if ((DrawWidth != 0) || (DrawHeight != 0))
        {
            auto cmd =
                DrawQuadCmd{ m_Sprite->Texture->Texture,
                             m_X,
                             m_Y,
                             DrawWidth ? uint32_t(DrawWidth) : uint32_t(m_Sprite->Width),
                             DrawHeight ? uint32_t(DrawHeight) : uint32_t(m_Sprite->Height) };
            RenderAdd_DrawQuad(g_renderCmdList, cmd);
        }
        else
        {
            m_Sprite->Texture->Draw(m_X, m_Y, checktrans);
        }
    }
}

bool CGUIExternalTexture::Select()
{
    if (m_Sprite != nullptr)
    {
        return m_Sprite->Select(m_X, m_Y, !BoundingBoxCheck);
    }
    return false;
}
