// crossuo/src/Renderer/Debug/RenderDebug.h
// Debug utilities for tracking render state changes and identifying state leaks

#ifndef RENDER_DEBUG_H
#define RENDER_DEBUG_H

#include "../RenderTypes.h"
#include <stdio.h>
#include <string>
#include <cstring>

// ============================================================================
// External Variables
// ============================================================================
extern int g_CurrentDrawMode;

// ============================================================================
// State Tracking Configuration
// ============================================================================

// ============================================================================
// QUICK START: To debug state leaks, uncomment these defines:
//
// Required for all debugging:
//   1. RENDER_DEBUG_STATE_TRACKING - Enables all debugging infrastructure
//
// For finding the state leaking object (recommended):
//   2. RENDER_DEBUG_DETECT_LEAKS - Detects when states are not reset properly
//
// For verbose logging (use only when needed, very noisy):
//   3. RENDER_DEBUG_LOG_ALL_CHANGES - Logs every single state change
// ============================================================================

// Uncomment or define this to enable state tracking and logging
//#define RENDER_DEBUG_STATE_TRACKING

// Uncomment or define this to log all state changes (verbose)
// Can be very noisy - use only when investigating specific issues
// #define RENDER_DEBUG_LOG_ALL_CHANGES

// Uncomment or define this to detect and log state leaks after draw calls
// This is the main feature for finding the object leaking state
//#define RENDER_DEBUG_DETECT_LEAKS

// ============================================================================
// Platform-specific includes
// ============================================================================

#ifdef RENDER_DEBUG_STATE_TRACKING
    #ifdef __linux__
        #include <execinfo.h>
        #include <cxxabi.h>
    #endif
#endif

// ============================================================================
// Logging Macros
// ============================================================================

#ifdef RENDER_DEBUG_STATE_TRACKING
    #define RENDER_DEBUG_LOG(fmt, ...) \
        fprintf(stderr, "[RenderDebug] " fmt "\n", ##__VA_ARGS__)

    #define RENDER_DEBUG_LOG_PREFIX(prefix, fmt, ...) \
        fprintf(stderr, "[RenderDebug] %s" fmt "\n", prefix, ##__VA_ARGS__)
#else
    #define RENDER_DEBUG_LOG(fmt, ...) ((void)0)
    #define RENDER_DEBUG_LOG_PREFIX(prefix, fmt, ...) ((void)0)
#endif

// Verbose logging - only enabled when RENDER_DEBUG_LOG_ALL_CHANGES is also defined
#ifdef RENDER_DEBUG_LOG_ALL_CHANGES
    #define RENDER_VERBOSE_LOG(fmt, ...) RENDER_DEBUG_LOG(fmt, ##__VA_ARGS__)
#else
    #define RENDER_VERBOSE_LOG(fmt, ...) ((void)0)
#endif

// ============================================================================
// Stringification Helper
// ============================================================================

#define RENDER_STRINGIFY_IMPL(x) #x
#define RENDER_STRINGIFY(x) RENDER_STRINGIFY_IMPL(x)

// ============================================================================
// Call Site Tagging
// ============================================================================

#ifdef RENDER_DEBUG_STATE_TRACKING
    // Simple call site without __FUNCTION__ to avoid macro expansion issues
    #define RENDER_CALL_SITE __FILE__ ":" RENDER_STRINGIFY(__LINE__)
#else
    #define RENDER_CALL_SITE ""
#endif

// ============================================================================
// State Change Logging
// ============================================================================

// These macros only log when RENDER_DEBUG_LOG_ALL_CHANGES is defined
// They are verbose and can generate a lot of output
#ifdef RENDER_DEBUG_STATE_TRACKING
    #ifdef RENDER_DEBUG_LOG_ALL_CHANGES
        #define RENDER_STATE_LOG(state_name, old_value, new_value) \
            RENDER_DEBUG_LOG("STATE CHANGE [%s] %s: %s -> %s at %s", \
                             state_name, #old_value, \
                             old_value ? "true" : "false", \
                             new_value ? "true" : "false", \
                             RENDER_CALL_SITE)
    #else
        #define RENDER_STATE_LOG(state_name, old_value, new_value) ((void)0)
    #endif

    #ifdef RENDER_DEBUG_LOG_ALL_CHANGES
        #define RENDER_STATE_LOG_INT(state_name, old_value, new_value) \
            RENDER_DEBUG_LOG("STATE CHANGE [%s] %s: %d -> %d at %s", \
                             state_name, #old_value, old_value, new_value, RENDER_CALL_SITE)
    #else
        #define RENDER_STATE_LOG_INT(state_name, old_value, new_value) ((void)0)
    #endif

    // Always log draw mode changes as they're critical for debugging
    #define RENDER_STATE_LOG_DRAW_MODE(old_value, new_value) \
        RENDER_DEBUG_LOG("DRAW MODE CHANGE: %d -> %d at %s", \
                         old_value, new_value, RENDER_CALL_SITE)

    // Always log blend state changes as they're critical for debugging
    #define RENDER_STATE_LOG_BLEND(old_enabled, new_enabled) \
        RENDER_DEBUG_LOG("BLEND STATE CHANGE: %s -> %s at %s", \
                         old_enabled ? "ENABLED" : "DISABLED", \
                         new_enabled ? "ENABLED" : "DISABLED", \
                         RENDER_CALL_SITE)
#else
    #define RENDER_STATE_LOG(state_name, old_value, new_value) ((void)0)
    #define RENDER_STATE_LOG_INT(state_name, old_value, new_value) ((void)0)
    #define RENDER_STATE_LOG_DRAW_MODE(old_value, new_value) ((void)0)
    #define RENDER_STATE_LOG_BLEND(old_enabled, new_enabled) ((void)0)
#endif

// State leak detection - controlled by RENDER_DEBUG_DETECT_LEAKS
#ifdef RENDER_DEBUG_DETECT_LEAKS
    #define RENDER_STATE_LEAK_DETECTED(state_info) \
        do { \
            RENDER_DEBUG_LOG("!!! STATE LEAK DETECTED !!!"); \
            RENDER_DEBUG_LOG("  %s", state_info); \
            RenderDebug_LogStackTrace("  Leak source:"); \
        } while(0)
#else
    #define RENDER_STATE_LEAK_DETECTED(state_info) ((void)0)
#endif

// ============================================================================
// Stack Trace Logging
// ============================================================================

#ifdef RENDER_DEBUG_STATE_TRACKING

// Log a stack trace with optional prefix
inline void RenderDebug_LogStackTrace(const char* prefix = "")
{
#ifdef __linux__
    void* callstack[16];
    int frames = backtrace(callstack, 16);
    char** strs = backtrace_symbols(callstack, frames);

    if (strs == nullptr)
    {
        RENDER_DEBUG_LOG_PREFIX(prefix, "Unable to capture stack trace");
        return;
    }

    RENDER_DEBUG_LOG_PREFIX(prefix, "Stack trace:");

    // Skip frames: this function + maybe one wrapper
    int skip_frames = 1;

    for (int i = skip_frames; i < frames && i < skip_frames + 8; ++i)
    {
        char* line = strs[i];

        // Try to demangle C++ function names
        // Format: /path/to/binary(func+offset) [address]

        char* begin = nullptr;
        char* end = nullptr;
        char* addr = nullptr;

        // Find function name in backtrace_symbols format
        for (char* p = line; *p; ++p)
        {
            if (*p == '(' && !begin)
            {
                begin = p;
            }
            else if (*p == '+' && begin)
            {
                end = p;
                break;
            }
            else if (*p == '[')
            {
                addr = p;
            }
        }

        if (begin && end)
        {
            *begin++ = '\0';
            *end = '\0';

            int status = 0;
            char* ret = abi::__cxa_demangle(begin, nullptr, nullptr, &status);

            if (status == 0 && ret)
            {
                // Extract just the filename from the path
                const char* filename = line;
                const char* last_slash = strrchr(line, '/');
                if (last_slash)
                {
                    filename = last_slash + 1;
                }

                if (addr)
                {
                    RENDER_DEBUG_LOG_PREFIX("    ", "#%d: %s in %s %s",
                                           i - skip_frames, ret, filename, addr);
                }
                else
                {
                    RENDER_DEBUG_LOG_PREFIX("    ", "#%d: %s in %s",
                                           i - skip_frames, ret, filename);
                }
                free(ret);
            }
            else
            {
                RENDER_DEBUG_LOG_PREFIX("    ", "#%d: %s %s",
                                       i - skip_frames, begin, line);
            }
        }
        else
        {
            RENDER_DEBUG_LOG_PREFIX("    ", "#%d: %s", i - skip_frames, line);
        }
    }

    free(strs);
#else
    RENDER_DEBUG_LOG_PREFIX(prefix, "Stack trace not available on this platform");
#endif
}

#else
    inline void RenderDebug_LogStackTrace(const char* prefix = "") {}
#endif

// ============================================================================
// State Dumping Helpers
// ============================================================================

#ifdef RENDER_DEBUG_STATE_TRACKING

// Log the current render state
inline void RenderDebug_DumpState(const RenderState* state)
{
    if (!state)
    {
        RENDER_DEBUG_LOG("RenderState: NULL");
        return;
    }

    RENDER_DEBUG_LOG("=== Current RenderState ===");
    RENDER_DEBUG_LOG("  blend.enabled = %s", state->blend.enabled ? "true" : "false");
    RENDER_DEBUG_LOG("  blend.src = %d", (int)state->blend.src);
    RENDER_DEBUG_LOG("  blend.dst = %d", (int)state->blend.dst);
    RENDER_DEBUG_LOG("  blend.equation = %d", (int)state->blend.equation);

    RENDER_DEBUG_LOG("  alphaTest.enabled = %s", state->alphaTest.enabled ? "true" : "false");
    RENDER_DEBUG_LOG("  alphaTest.func = %d", (int)state->alphaTest.func);
    RENDER_DEBUG_LOG("  alphaTest.alphaRef = %f", state->alphaTest.alphaRef);

    RENDER_DEBUG_LOG("  stencil.enabled = %s", state->stencil.enabled ? "true" : "false");
    RENDER_DEBUG_LOG("  stencil.func = %d", (int)state->stencil.func);
    RENDER_DEBUG_LOG("  stencil.ref = %u", state->stencil.ref);
    RENDER_DEBUG_LOG("  stencil.mask = %u", state->stencil.mask);

    RENDER_DEBUG_LOG("  depth.enabled = %s", state->depth.enabled ? "true" : "false");
    RENDER_DEBUG_LOG("  depth.func = %d", (int)state->depth.func);

    RENDER_DEBUG_LOG("  scissor.enabled = %s", state->scissor.enabled ? "true" : "false");
    RENDER_DEBUG_LOG("  scissor.x = %d", state->scissor.x);
    RENDER_DEBUG_LOG("  scissor.y = %d", state->scissor.y);
    RENDER_DEBUG_LOG("  scissor.width = %u", state->scissor.width);
    RENDER_DEBUG_LOG("  scissor.height = %u", state->scissor.height);

#if defined(USE_GLES) || defined(USE_GL3)
    RENDER_DEBUG_LOG("  cached.currentDrawMode = %d", state->currentDrawMode);
    RENDER_DEBUG_LOG("  cached.currentProgram = %u", state->currentProgram);
    RENDER_DEBUG_LOG("  cached.modelMatrixCached = %s",
                    state->modelMatrixCached ? "true" : "false");
#endif

    RENDER_DEBUG_LOG("===========================");
}

#else
    inline void RenderDebug_DumpState(const RenderState* state) { (void)state; }
#endif

// Log state before and after a draw operation
// Always defined, with no-op when disabled
#ifdef RENDER_DEBUG_STATE_TRACKING
    #define RENDER_STATE_DUMP_BEFORE(state) \
            do { \
                RENDER_DEBUG_LOG("--- State BEFORE draw at %s ---", RENDER_CALL_SITE); \
                RenderDebug_DumpState(state); \
            } while (0)

    #define RENDER_STATE_DUMP_AFTER(state) \
            do { \
                RENDER_DEBUG_LOG("--- State AFTER draw at %s ---", RENDER_CALL_SITE); \
                RenderDebug_DumpState(state); \
            } while (0)
#else
    #define RENDER_STATE_DUMP_BEFORE(state) ((void)0)
    #define RENDER_STATE_DUMP_AFTER(state) ((void)0)
#endif

// ============================================================================
// State Leak Detection
// ============================================================================

// Check if the expected default state matches the actual state
// and log a stack trace if they don't match
// Always defined, with no-op when disabled
inline void RenderDebug_CheckStateLeaks(const RenderState* state, const char* operation)
{
#ifdef RENDER_DEBUG_DETECT_LEAKS
    #if defined(USE_GLES) || defined(USE_GL3)
    bool has_leak = false;
    std::string leak_info = operation;
    leak_info += " - ";

    if (state->blend.enabled)
    {
        has_leak = true;
        leak_info += "blend.enabled=true; ";
    }

    if (state->currentDrawMode != 1)
    {
        has_leak = true;
        leak_info += "currentDrawMode=";
        leak_info += std::to_string(state->currentDrawMode);
        leak_info += " (expected 1); ";
    }

    if (g_CurrentDrawMode != 0)
    {
        has_leak = true;
        leak_info += "g_CurrentDrawMode=";
        leak_info += std::to_string(g_CurrentDrawMode);
        leak_info += " (expected 0); ";
    }

    if (has_leak)
    {
        RENDER_STATE_LEAK_DETECTED(leak_info.c_str());
    }
    #endif
#else
    (void)state;
    (void)operation;
#endif // RENDER_DEBUG_DETECT_LEAKS
}

// ============================================================================
// Scoped State Change Tracker
// ============================================================================

#ifdef RENDER_DEBUG_STATE_TRACKING

// RAII class to track state changes within a scope
class RenderDebug_StateTracker
{
public:
    explicit RenderDebug_StateTracker(const char* name)
        : m_name(name)
    {
        RENDER_DEBUG_LOG(">>> ENTER: %s", m_name);
    }

    ~RenderDebug_StateTracker()
    {
        RENDER_DEBUG_LOG("<<< EXIT: %s", m_name);
    }

private:
    const char* m_name;
};

#define RENDER_DEBUG_SCOPE(name) \
    RenderDebug_StateTracker RENDER_DEBUG_SCOPE_##name(#name)

#else
    #define RENDER_DEBUG_SCOPE(name) ((void)0)
#endif // RENDER_DEBUG_STATE_TRACKING

#endif // RENDER_DEBUG_H
