// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIExternalTexture.h"
#include "../Managers/ColorManager.h"
#include "../Point.h"
#include "../Sprite.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"
#include "../GLEngine/GLEngine.h" // REMOVE

extern RenderCmdList *g_renderCmdList;

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
    DEBUG_TRACE_FUNCTION;
    if (DeleteTextureOnDestroy)
    {
        RELEASE_POINTER(m_Sprite);
    }
}

CSize CGUIExternalTexture::GetSize()
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else

        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
    }
}

void CGUIExternalTexture::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    if (m_Sprite != nullptr)
    {
        SetShaderMode();
        if ((DrawWidth != 0) || (DrawHeight != 0))
        {
#ifndef NEW_RENDERER_ENABLED
            CGLTexture tex;
            tex.Texture = m_Sprite->Texture->Texture;
            if (DrawWidth != 0)
            {
                tex.Width = DrawWidth;
            }
            else
            {
                tex.Width = m_Sprite->Width;
            }

            if (DrawHeight != 0)
            {
                tex.Height = DrawHeight;
            }
            else
            {
                tex.Height = m_Sprite->Height;
            }
            g_GL.Draw(tex, m_X, m_Y);
            tex.Texture = 0;
#else
            auto cmd =
                DrawQuadCmd{ m_Sprite->Texture->Texture,
                             m_X,
                             m_Y,
                             DrawWidth ? uint32_t(DrawWidth) : uint32_t(m_Sprite->Width),
                             DrawHeight ? uint32_t(DrawHeight) : uint32_t(m_Sprite->Height) };
            RenderAdd_DrawQuad(g_renderCmdList, cmd);
#endif
        }
        else
        {
            m_Sprite->Texture->Draw(m_X, m_Y, checktrans);
        }
    }
}

bool CGUIExternalTexture::Select()
{
    DEBUG_TRACE_FUNCTION;
    if (m_Sprite != nullptr)
    {
        return m_Sprite->Select(m_X, m_Y, !BoundingBoxCheck);
    }
    return false;
}
