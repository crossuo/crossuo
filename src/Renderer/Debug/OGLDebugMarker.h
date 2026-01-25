// AGPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#include "../../SDL_wrapper.h"
#include "../RenderCommands.h"

// OpenGL debug markers for RenderDoc support
// Only enabled for GL3 (NEW_RENDERER_ENABLED) to ensure KHR_debug extension availability

#if defined(NEW_RENDERER_ENABLED) && !defined(RENDERER_LEGACY) && (!defined(USE_GL2) || defined(USE_GL3) || defined(USE_GLES))

// Forward declarations - implemented in RenderAdd.cpp
bool RenderAdd_PushDebugMarker(RenderCmdList *cmdList, const char *label);
bool RenderAdd_PopDebugMarker(RenderCmdList *cmdList);

// Scoped GL debug marker - automatically pushes a marker on construction and pops on destruction
struct ScopedGLDebugMarker
{
    RenderCmdList *m_cmdList = nullptr;

    explicit ScopedGLDebugMarker(RenderCmdList *cmdList, const char *label)
        : m_cmdList(cmdList)
    {
        if (m_cmdList)
        {
            RenderAdd_PushDebugMarker(m_cmdList, label);
        }
    }

    ~ScopedGLDebugMarker()
    {
        if (m_cmdList)
        {
            RenderAdd_PopDebugMarker(m_cmdList);
        }
    }
};

#define SCOPED_GL_DEBUG_MARKER() ScopedGLDebugMarker _scopedGLMarker(g_renderCmdList, __FUNCTION__)
#define SCOPED_GL_DEBUG_MARKER_LABEL(label) ScopedGLDebugMarker _scopedGLMarker(g_renderCmdList, label)
#define GL_DEBUG_MARKER_PUSH(label) RenderAdd_PushDebugMarker(g_renderCmdList, label)
#define GL_DEBUG_MARKER_POP() RenderAdd_PopDebugMarker(g_renderCmdList)

#else // #if defined(NEW_RENDERER_ENABLED) && !defined(RENDERER_LEGACY) && (defined(USE_GL3) || defined(USE_GLES))

#define SCOPED_GL_DEBUG_MARKER()
#define SCOPED_GL_DEBUG_MARKER_LABEL(label)
#define GL_DEBUG_MARKER_PUSH(label)
#define GL_DEBUG_MARKER_POP()

#endif // #if defined(NEW_RENDERER_ENABLED) && !defined(RENDERER_LEGACY) && (defined(USE_GL3) || defined(USE_GLES))
