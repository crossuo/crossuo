// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "RenderFrameBuffer.h"
#include "../GameWindow.h"
#include "RenderAPI.h"
#include "../Utility/PerfMarker.h"

frame_buffer_t CRenderFrameBuffer::m_OldFrameBuffer;

CRenderFrameBuffer::CRenderFrameBuffer()
= default;

CRenderFrameBuffer::~CRenderFrameBuffer()
{
    Free();
}

bool CRenderFrameBuffer::Init(int width, int height)
{
    ScopedPerfMarker(__FUNCTION__);
    Free();

    bool result = false;

    if (width > 0 && height > 0)
    {
        m_FrameBuffer = Render_CreateFrameBuffer(uint32_t(width), uint32_t(height));
        result = m_FrameBuffer.texture != RENDER_TEXTUREHANDLE_INVALID &&
                 m_FrameBuffer.handle != RENDER_FRAMEBUFFER_INVALID;
        m_Ready = result;
        if (result)
        {
            m_Width = uint32_t(width);
            m_Height = uint32_t(height);
        }
    }

    return result;
}

void CRenderFrameBuffer::Free()
{
    ScopedPerfMarker(__FUNCTION__);

    if (m_FrameBuffer.handle != RENDER_FRAMEBUFFER_INVALID)
    {
        Render_DestroyFrameBuffer(m_FrameBuffer);
    }

    m_FrameBuffer = {};

    m_OldFrameBuffer = {};
}

void CRenderFrameBuffer::Release()
{
    ScopedPerfMarker(__FUNCTION__);

    // TODO this isn't the place to keep track of the previous frame buffer
    RenderAdd_SetFrameBuffer(g_renderCmdList, SetFrameBufferCmd{ m_OldFrameBuffer });

    RenderAdd_SetTexture(
        g_renderCmdList,
        SetTextureCmd{ m_FrameBuffer.texture, TextureType::TextureType_Texture2D_Mipmapped });

    auto window_width = g_GameWindow.GetSize().Width;
    auto window_height = g_GameWindow.GetSize().Height;
    auto viewParams = SetViewParamsCmd{
        0, 0, window_width, window_height, window_width, window_height, -150, 150
    };
    RenderAdd_SetViewParams(g_renderCmdList, viewParams);
}

bool CRenderFrameBuffer::Ready(int width, int height) const
{
    assert(width >= 0 && height >= 0);
    return (m_Ready && m_Width == uint32_t(width) && m_Height == uint32_t(height));
}

bool CRenderFrameBuffer::ReadyMinSize(int width, int height) const
{
    assert(width >= 0 && height >= 0);
    return (m_Ready && m_Width >= uint32_t(width) && m_Height >= uint32_t(height));
}

bool CRenderFrameBuffer::Use()
{
    ScopedPerfMarker(__FUNCTION__);
    bool result = false;

    if (m_Ready)
    {
        HACKRender_GetFrameBuffer(g_renderCmdList, &m_OldFrameBuffer);
        RenderAdd_SetFrameBuffer(g_renderCmdList, SetFrameBufferCmd{ m_FrameBuffer });
        static constexpr bool flipped_y = true;
        RenderAdd_SetViewParams(
            g_renderCmdList,
            SetViewParamsCmd{ 0,
                              0,
                              int(m_Width),
                              int(m_Height),
                              int(m_Width),
                              int(m_Height),
                              -150,
                              150,
                              1.f,
                              flipped_y });

        result = true;
    }

    return result;
}

void CRenderFrameBuffer::Draw(int x, int y)
{
    ScopedPerfMarker(__FUNCTION__);
    SCOPED_GL_DEBUG_MARKER_LABEL("CRenderFrameBuffer::Draw");
    if (m_Ready)
    {
        auto cmd =
            DrawQuadCmd{ m_FrameBuffer.texture, x, y, m_Width, m_Height, 1.f, 1.f, g_ColorInvalid };
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
    }
}
