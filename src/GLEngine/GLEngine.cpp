// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "../Renderer/RenderAPI.h"
#if defined(USE_GL1)

#include "GLEngine.h"
#include "../Utility/PerfMarker.h"
#include "../Wisp/WispWindow.h"

CGLEngine g_GL;

CGLEngine::CGLEngine()
{
}

CGLEngine::~CGLEngine()
{
    Uninstall();
}

void CGLEngine::Uninstall()
{
    if (m_context != nullptr)
    {
        SDL_GL_DeleteContext(m_context);
    }
}

void CGLEngine::BeginDraw()
{
    ScopedPerfMarker(__FUNCTION__);
    Drawing = true;
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    GL_CALL(glLoadIdentity());

    GL_CALL(glDisable(GL_DEPTH_TEST));
    GL_CALL(glDepthFunc(GL_LEQUAL));
    GL_CALL(glDisable(GL_STENCIL_TEST));
    GL_CALL(glDisable(GL_BLEND));

    GL_CALL(glEnable(GL_ALPHA_TEST));
    GL_CALL(glAlphaFunc(GL_GREATER, 0.0f));
}

void CGLEngine::EndDraw()
{
    ScopedPerfMarker(__FUNCTION__);
    Drawing = false;
    GL_CALL(glDisable(GL_ALPHA_TEST));
#if !defined(XUO_EMSC)
    GL_CALL(SDL_GL_SwapWindow(Wisp::g_WispWindow->m_window));
#endif
}

#endif
