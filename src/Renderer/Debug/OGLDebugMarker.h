// AGPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#include "../../SDL_wrapper.h"
#include "../RenderCommands.h"

// OpenGL debug markers for RenderDoc support
// Only enabled for GL3 (NEW_RENDERER_ENABLED) to ensure KHR_debug extension availability

#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))

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

// Convenience macro for scoped markers with __FUNCTION__
#define SCOPED_GL_DEBUG_MARKER(cmdList) ScopedGLDebugMarker _scopedGLMarker(cmdList, __FUNCTION__)
#define SCOPED_GL_DEBUG_MARKER_LABEL(cmdList, label) ScopedGLDebugMarker _scopedGLMarker(cmdList, label)

// Manual marker macros for more control
#define GL_DEBUG_MARKER_PUSH(cmdList, label) RenderAdd_PushDebugMarker(cmdList, label)
#define GL_DEBUG_MARKER_POP(cmdList) RenderAdd_PopDebugMarker(cmdList)

#else

#define SCOPED_GL_DEBUG_MARKER(cmdList)
#define SCOPED_GL_DEBUG_MARKER_LABEL(cmdList, label)

// Manual marker macros for more control
#define GL_DEBUG_MARKER_PUSH(cmdList, label)
#define GL_DEBUG_MARKER_POP(cmdList)

#endif
