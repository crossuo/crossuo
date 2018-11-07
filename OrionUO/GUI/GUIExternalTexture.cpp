// MIT License
// Copyright (C) August 2016 Hotride

CGUIExternalTexture::CGUIExternalTexture(
    CGLTexture *texture, bool deleteTextureOnDestroy, int x, int y, int drawWidth, int drawHeight)
    : CBaseGUI(GOT_EXTERNALTEXTURE, 0, 0, 0, x, y)
    , m_Texture(texture)
    , DeleteTextureOnDestroy(deleteTextureOnDestroy)
    , DrawWidth(drawWidth)
    , DrawHeight(drawHeight)
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

Wisp::CSize CGUIExternalTexture::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CSize size;

    if (m_Texture != nullptr)
    {
        if (DrawWidth)
            size.Width = DrawWidth;
        else
            size.Width = m_Texture->Width;

        if (DrawHeight)
            size.Height = DrawHeight;
        else
            size.Height = m_Texture->Height;
    }

    return size;
}

void CGUIExternalTexture::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;

    if (Color != 0)
    {
        if (PartialHue)
            glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
        else
            glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);

        g_ColorManager.SendColorsToShader(Color);
    }
    else
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
}

void CGUIExternalTexture::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Texture != nullptr)
    {
        SetShaderMode();

        if (DrawWidth || DrawHeight)
        {
            CGLTexture tex;
            tex.Texture = m_Texture->Texture;

            if (DrawWidth)
                tex.Width = DrawWidth;
            else
                tex.Width = m_Texture->Width;

            if (DrawHeight)
                tex.Height = DrawHeight;
            else
                tex.Height = m_Texture->Height;

            g_GL.GL1_Draw(tex, m_X, m_Y);

            tex.Texture = 0;
        }
        else
            m_Texture->Draw(m_X, m_Y, checktrans);
    }
}

bool CGUIExternalTexture::Select()
{
    DEBUG_TRACE_FUNCTION;
    if (m_Texture != nullptr)
        return m_Texture->Select(m_X, m_Y, !CheckPolygone);

    return false;
}

