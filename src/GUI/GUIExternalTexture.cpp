// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIExternalTexture.h"
#include "../Managers/ColorManager.h"
#include "../Point.h"

CGUIExternalTexture::CGUIExternalTexture(
    CGLTexture *texture, bool deleteTextureOnDestroy, int x, int y, int drawWidth, int drawHeight)
    : CBaseGUI(GOT_EXTERNALTEXTURE, 0, 0, 0, x, y)
    , DeleteTextureOnDestroy(deleteTextureOnDestroy)
    , DrawWidth(drawWidth)
    , DrawHeight(drawHeight)
    , m_Texture(texture)
{
}

CGUIExternalTexture::~CGUIExternalTexture()
{
    DEBUG_TRACE_FUNCTION;
    if (DeleteTextureOnDestroy)
    {
        RELEASE_POINTER(m_Texture);
    }
}

CSize CGUIExternalTexture::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    CSize size;

    if (m_Texture != nullptr)
    {
        if (DrawWidth != 0)
        {
            size.Width = DrawWidth;
        }
        else
        {
            size.Width = m_Texture->Width;
        }

        if (DrawHeight != 0)
        {
            size.Height = DrawHeight;
        }
        else
        {
            size.Height = m_Texture->Height;
        }
    }

    return size;
}

void CGUIExternalTexture::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;

    if (Color != 0)
    {
        if (PartialHue)
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
        }
        else
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
        }

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
    }
}

void CGUIExternalTexture::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Texture != nullptr)
    {
        SetShaderMode();

        if ((DrawWidth != 0) || (DrawHeight != 0))
        {
            CGLTexture tex;
            tex.Texture = m_Texture->Texture;

            if (DrawWidth != 0)
            {
                tex.Width = DrawWidth;
            }
            else
            {
                tex.Width = m_Texture->Width;
            }

            if (DrawHeight != 0)
            {
                tex.Height = DrawHeight;
            }
            else
            {
                tex.Height = m_Texture->Height;
            }

            g_GL.GL1_Draw(tex, m_X, m_Y);

            tex.Texture = 0;
        }
        else
        {
            m_Texture->Draw(m_X, m_Y, checktrans);
        }
    }
}

bool CGUIExternalTexture::Select()
{
    DEBUG_TRACE_FUNCTION;
    if (m_Texture != nullptr)
    {
        return m_Texture->Select(m_X, m_Y, !CheckPolygone);
    }

    return false;
}
