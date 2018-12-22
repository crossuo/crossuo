// MIT License
// Copyright (C) August 2016 Hotride

CGUIScissor::CGUIScissor(bool enabled, int baseX, int baseY, int x, int y, int width, int height)
    : CGUIPolygonal(GOT_SCISSOR, x, y, width, height)
    , BaseX(baseX)
    , BaseY(baseY)
{
    Enabled = enabled;
}

CGUIScissor::~CGUIScissor()
{
}

void CGUIScissor::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Enabled)
    {
        int x = m_X;
        int y = m_Y;

        if (GumpParent != nullptr)
        {
            x -= GumpParent->GumpRect.Position.X;
            y -= GumpParent->GumpRect.Position.Y;
        }

        g_GL.PushScissor(BaseX + x, BaseY + y, Width, Height);
    }
    else
    {
        g_GL.PopScissor();
    }
}
