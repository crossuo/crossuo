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
    if (m_Sprite == nullptr || m_Sprite->Texture == nullptr)
    {
        return;
    }

    SetShaderMode();
    const int spriteWidth = m_Sprite->Width;
    const int spriteHeight = m_Sprite->Height;
    // a zero field means "use the natural size"
    const int boxWidth = DrawWidth ? DrawWidth : spriteWidth;
    const int boxHeight = DrawHeight ? DrawHeight : spriteHeight;

    switch (DrawMode)
    {
        case EXTERNAL_TEXTURE_DRAW_MODE::Stretch:
        {
            // CRenderTexture::Draw(x, y, w, h) scales the quad UVs to fill the
            // box, which is exactly the stretched placement
            m_Sprite->Texture->Draw(m_X, m_Y, boxWidth, boxHeight, checktrans);
            break;
        }
        case EXTERNAL_TEXTURE_DRAW_MODE::Fit:
        {
            // aspect fit: scale both axes by the same factor, then center the
            // result inside the box (integer math, never upscales past the box)
            if (spriteWidth <= 0 || spriteHeight <= 0 || boxWidth <= 0 || boxHeight <= 0)
            {
                break;
            }
            const bool widthBound =
                int64_t(boxWidth) * spriteHeight <= int64_t(boxHeight) * spriteWidth;
            const int scaleWidth = widthBound ? boxWidth : (boxHeight * spriteWidth) / spriteHeight;
            const int scaleHeight =
                widthBound ? (boxWidth * spriteHeight) / spriteWidth : boxHeight;
            const int x = m_X + (boxWidth - scaleWidth) / 2;
            const int y = m_Y + (boxHeight - scaleHeight) / 2;
            m_Sprite->Texture->Draw(x, y, scaleWidth, scaleHeight, checktrans);
            break;
        }
        case EXTERNAL_TEXTURE_DRAW_MODE::Tile:
        {
            // repeat the sprite over the box. The quad command has no scissor,
            // so the last column/row of copies is drawn at its natural size and
            // overshoots the box; backgrounds are authored to be exact
            // multiples when that matters (640x480 screen over a 64/128/320
            // source), same as CGUIGumppicTiled
            if (spriteWidth <= 0 || spriteHeight <= 0)
            {
                break;
            }
            for (int y = m_Y; y < m_Y + boxHeight; y += spriteHeight)
            {
                for (int x = m_X; x < m_X + boxWidth; x += spriteWidth)
                {
                    m_Sprite->Texture->Draw(x, y, checktrans);
                }
            }
            break;
        }
        case EXTERNAL_TEXTURE_DRAW_MODE::Native:
        default:
        {
            // the sprite's own size; an explicit box is honoured through the
            // scaled draw so DrawWidth/DrawHeight stay meaningful
            if ((DrawWidth != 0) || (DrawHeight != 0))
            {
                m_Sprite->Texture->Draw(m_X, m_Y, boxWidth, boxHeight, checktrans);
            }
            else
            {
                m_Sprite->Texture->Draw(m_X, m_Y, checktrans);
            }
            break;
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
