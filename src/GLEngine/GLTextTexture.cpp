// MIT License
// Copyright (C) August 2016 Hotride

CGLTextTexture::CGLTextTexture()

{
}

CGLTextTexture::~CGLTextTexture()
{
}

void CGLTextTexture::Clear()
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture::Clear();
    LinesCount = 0;
}

void CGLTextTexture::Draw(int x, int y, bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
        if (checktrans)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            g_GL_Draw(*this, x, y);

            glDisable(GL_BLEND);

            glEnable(GL_STENCIL_TEST);

            g_GL_Draw(*this, x, y);

            glDisable(GL_STENCIL_TEST);
        }
        else
        {
            g_GL_Draw(*this, x, y);
        }
    }
}
