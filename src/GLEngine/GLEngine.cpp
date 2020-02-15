// MIT License
// Copyright (C) August 2016 Hotride

#include "../Renderer/RenderAPI.h"
#ifndef NEW_RENDERER_ENABLED

#include <cmath> // cos, sin
#include <algorithm>
#include "GLEngine.h"
#include "GLTexture.h"
#include "../Globals.h" // g_GlobalScale
#include "../Point.h"
#include "../GameWindow.h"
#include "../Managers/GumpManager.h"
#include "../GameObjects/LandObject.h"
#include "../Utility/PerfMarker.h"

CGLEngine g_GL;

CGLEngine::CGLEngine()
{
}

CGLEngine::~CGLEngine()
{
    Uninstall();
}

// #define OGL_DEBUGCONTEXT_ENABLED
#ifdef OGL_DEBUGCONTEXT_ENABLED
#define OGL_DEBUGMSG_SEVERITY_COUNT (3)
#define OGL_DEBUGMSG_TYPE_COUNT (8)
#define OGL_DEBUGMSG_IDS_MAX (16)
#define OGL_DEBUGMSG_INVALIDID (0xffffffff)

struct
{
    bool assert = false;
    bool log = false;
} static s_openglDebugMsgSeverity[OGL_DEBUGMSG_SEVERITY_COUNT];

struct
{
    bool assert = false;
    bool log = false;
} static s_openglDebugMsgType[OGL_DEBUGMSG_SEVERITY_COUNT];

struct
{
    GLuint id = OGL_DEBUGMSG_INVALIDID;
    bool assert = false;
    bool log = false;
} static s_openglDebugMsgs[OGL_DEBUGMSG_IDS_MAX];

static void EnableOpenGLDebugMsgSeverity(GLenum severity, bool assert, bool log)
{
    auto &info = s_openglDebugMsgSeverity[severity % OGL_DEBUGMSG_SEVERITY_COUNT];
    info.assert = assert;
    info.log = log;
}

static void EnableOpenGLDebugMsgType(GLenum type, bool assert, bool log)
{
    auto &info = s_openglDebugMsgType[type % OGL_DEBUGMSG_TYPE_COUNT];
    info.assert = assert;
    info.log = log;
}

static void EnableOpenGLMessage(GLuint id, bool assert, bool log)
{
    for (auto &msg : s_openglDebugMsgs)
    {
        if (msg.id == OGL_DEBUGMSG_INVALIDID)
        {
            msg.id = id;
            msg.assert = assert;
            msg.log = log;
            return;
        }
    }

    Error(Renderer, "Can't change settings for OpenGL debug message, OGL_DEBUGMSG_IDS_MAX reached");
    assert(false);
}

static void openglCallbackFunction(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *userParam)
{
    (void)source;
    (void)length;
    (void)userParam;

    // auto getMsgInfo = [](GLenum sev, GLenum type, GLuint id) {
    //     auto &infoSeverity = s_openglDebugMsgSeverity[sev % OGL_DEBUGMSG_SEVERITY_COUNT];
    //     auto &infoType = s_openglDebugMsgType[type % OGL_DEBUGMSG_TYPE_COUNT];

    //     auto [shouldAssert, shouldLog] = std::tie(infoSeverity.assert, infoSeverity.log);
    //     shouldAssert &= infoType.assert;
    //     shouldLog &= infoType.log;

    //     if (!shouldAssert && !shouldLog)
    //         return std::tie(shouldAssert, shouldLog);

    //     for (auto &ctrl : s_openglDebugMsgs)
    //     {
    //         if (ctrl.id == OGL_DEBUGMSG_INVALIDID)
    //         {
    //             break;
    //         }

    //         if (ctrl.id == id)
    //         {
    //             shouldAssert &= ctrl.assert;
    //             shouldLog &= ctrl.log;
    //             break;
    //         }
    //     }

    //     return std::tie(shouldAssert, shouldLog);
    // };

    // auto [shouldAssert, shouldLog] = getMsgInfo(severity, type, id);
    auto shouldLog = true;
    auto shouldAssert = false;
    if (shouldLog)
    {
        Info(Renderer, "OpenGL debug message (id %d): %s", id, message);
    }
    assert(!shouldAssert);
}
#endif // OGL_DEBUGCONTEXT_ENABLED

static void SetupOGLDebugMessage()
{
#ifdef OGL_DEBUGCONTEXT_ENABLED
    EnableOpenGLDebugMsgSeverity(GL_DEBUG_SEVERITY_HIGH, true, true);
    EnableOpenGLDebugMsgSeverity(GL_DEBUG_SEVERITY_MEDIUM, false, true);
    EnableOpenGLDebugMsgSeverity(GL_DEBUG_SEVERITY_LOW, false, true);

    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_ERROR, true, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, true, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_OTHER, true, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, false, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_PORTABILITY, false, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_PERFORMANCE, false, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_MARKER, false, false);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_POP_GROUP, false, true);

    EnableOpenGLMessage(OGL_USERPERFMARKERS_ID, false, false);

    // GL error "GL_INVALID_OPERATION in ...":
    // 1) FIXME no shader set when glUniform1iARB is called for g_ShaderDrawMode
    EnableOpenGLMessage(1282, false, false);

    // Usage warning: glClear() called with GL_STENCIL_BUFFER_BIT, but there is no stencil buffer. Operation will have no effect.
    EnableOpenGLMessage(131076, false, false);

    glDebugMessageCallback(openglCallbackFunction, nullptr);
    glDebugMessageControl(
        GL_DONT_CARE, // source
        GL_DONT_CARE, // type
        GL_DONT_CARE, // severity
        0,
        nullptr,
        GL_TRUE);
#endif
}

bool CGLEngine::Install()
{
    OldTexture = -1;

#ifdef OGL_DEBUGCONTEXT_ENABLED
    auto debugContext = true;
#else
    auto debugContext = false;
#endif
    if (debugContext)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }

    m_context = SDL_GL_CreateContext(g_GameWindow.m_window);
    SDL_GL_MakeCurrent(g_GameWindow.m_window, m_context);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int glewInitResult = glewInit();
    Info(
        Renderer,
        "glewInit() = %i fb=%i v(%s) (shader: %i)",
        glewInitResult,
        GL_ARB_framebuffer_object,
        glGetString(GL_VERSION),
        GL_ARB_shader_objects);
    if (glewInitResult != 0)
    {
        return false;
    }

    // debug messages callback needs ogl >= 4.30
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDebugMessageControl.xhtml
    if (debugContext && GLEW_KHR_debug)
    {
        SetupOGLDebugMessage();
    }

    Info(Renderer, "Graphics Successfully Initialized");
    Info(Renderer, "OpenGL Info:");
    Info(Renderer, "    Version: %s", glGetString(GL_VERSION));
    Info(Renderer, "     Vendor: %s", glGetString(GL_VENDOR));
    Info(Renderer, "   Renderer: %s", glGetString(GL_RENDERER));
    Info(Renderer, "    Shading: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    auto canUseFrameBuffer =
        (GL_ARB_framebuffer_object && glBindFramebuffer && glDeleteFramebuffers &&
         glFramebufferTexture2D && glGenFramebuffers);

    Info(Renderer, "g_UseFrameBuffer = %i", canUseFrameBuffer);

    if (!canUseFrameBuffer)
    {
        SDL_GL_DeleteContext(m_context);
        m_context = nullptr;

        Error(Client, "Your graphics card does not support Frame Buffers");
        g_GameWindow.ShowMessage("Your graphics card does not support Frame Buffers", "Error");
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black Background
    glShadeModel(GL_SMOOTH);              // Enables Smooth Color Shading
    glClearDepth(1.0);                    // Depth Buffer Setup
    glDisable(GL_DITHER);

    //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);   //Realy Nice perspective calculations
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    glEnable(GL_TEXTURE_2D);

    SDL_GL_SetSwapInterval(0); // 1 vsync

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glClearStencil(0);
    glStencilMask(1);

    glEnable(GL_LIGHT0);

    GLfloat lightPosition[] = { -1.0f, -1.0f, 0.5f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, &lightPosition[0]);

    GLfloat lightAmbient[] = { 2.0f, 2.0f, 2.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, &lightAmbient[0]);

    GLfloat lav = 0.8f;
    GLfloat lightAmbientValues[] = { lav, lav, lav, lav };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, &lightAmbientValues[0]);

    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    const auto size = g_GameWindow.GetSize();
    ViewPort(0, 0, size.Width, size.Height);
    return true;
}

void CGLEngine::Uninstall()
{
    if (m_context != nullptr)
    {
        SDL_GL_DeleteContext(m_context);
    }
}

void CGLEngine::UpdateRect()
{
    ScopedPerfMarker(__FUNCTION__);

    int width, height;
    //SDL_GL_GetDrawableSize(g_GameWindow.m_window, &width, &height);
    SDL_GetWindowSize(g_GameWindow.m_window, &width, &height);

    ViewPort(0, 0, width, height);
    //ViewPort(0, 0, g_GameWindow.GetSize().Width, g_GameWindow.GetSize().Height);

    g_GumpManager.RedrawAll();
}

void CGLEngine::BindTexture16(CGLTexture &texture, int width, int height, uint16_t *pixels)
{
    ScopedPerfMarker(__FUNCTION__);

    GLuint tex = 0;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB5_A1,
        width,
        height,
        0,
        GL_BGRA,
        GL_UNSIGNED_SHORT_1_5_5_5_REV,
        pixels);

    texture.Width = width;
    texture.Height = height;
    texture.Texture = tex;
}

void CGLEngine::BindTexture32(CGLTexture &texture, int width, int height, uint32_t *pixels)
{
    ScopedPerfMarker(__FUNCTION__);

    GLuint tex = 0;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA4, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, pixels);

    texture.Width = width;
    texture.Height = height;
    texture.Texture = tex;
}

void CGLEngine::BeginDraw()
{
    ScopedPerfMarker(__FUNCTION__);
    Drawing = true;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
}

void CGLEngine::EndDraw()
{
    ScopedPerfMarker(__FUNCTION__);
    Drawing = false;
    glDisable(GL_ALPHA_TEST);
    SDL_GL_SwapWindow(Wisp::g_WispWindow->m_window);
}

void CGLEngine::ViewPortScaled(int x, int y, int width, int height)
{
    ScopedPerfMarker(__FUNCTION__);

    glViewport(x, g_GameWindow.GetSize().Height - y - height, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    GLdouble left = (GLdouble)x;
    GLdouble right = (GLdouble)(width + x);
    GLdouble top = (GLdouble)y;
    GLdouble bottom = (GLdouble)(height + y);

    GLdouble newRight = right * g_GlobalScale;
    GLdouble newBottom = bottom * g_GlobalScale;

    left = (left * g_GlobalScale) - (newRight - right);
    top = (top * g_GlobalScale) - (newBottom - bottom);

    glOrtho(left, newRight, newBottom, top, -150.0, 150.0);
    glMatrixMode(GL_MODELVIEW);
}

void CGLEngine::ViewPort(int x, int y, int width, int height)
{
    ScopedPerfMarker(__FUNCTION__);

    const auto size = g_GameWindow.GetSize();
    glViewport(x, size.Height - y - height, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(x, width + x, height + y, y, -150.0, 150.0);
    glMatrixMode(GL_MODELVIEW);
}

void CGLEngine::RestorePort()
{
    ScopedPerfMarker(__FUNCTION__);

    glViewport(0, 0, g_GameWindow.GetSize().Width, g_GameWindow.GetSize().Height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, g_GameWindow.GetSize().Width, g_GameWindow.GetSize().Height, 0.0, -150.0, 150.0);
    glMatrixMode(GL_MODELVIEW);
}

void CGLEngine::PushScissor(int x, int y, int width, int height)
{
    PushScissor(CRect(x, y, width, height));
}

void CGLEngine::PushScissor(const CPoint2Di &position, int width, int height)
{
    PushScissor(CRect(position, width, height));
}

void CGLEngine::PushScissor(int x, int y, const CSize &size)
{
    PushScissor(CRect(x, y, size));
}

void CGLEngine::PushScissor(const CPoint2Di &position, const CSize &size)
{
    PushScissor(CRect(position, size));
}

void CGLEngine::PushScissor(const CRect &rect)
{
    ScopedPerfMarker(__FUNCTION__);
    m_ScissorList.push_back(rect);
    glEnable(GL_SCISSOR_TEST);
    glScissor(rect.Position.X, rect.Position.Y, rect.Size.Width, rect.Size.Height);
}

void CGLEngine::PopScissor()
{
    ScopedPerfMarker(__FUNCTION__);

    if (!m_ScissorList.empty())
    {
        m_ScissorList.pop_back();
    }

    if (m_ScissorList.empty())
    {
        glDisable(GL_SCISSOR_TEST);
    }
    else
    {
        CRect &rect = m_ScissorList.back();
        glScissor(rect.Position.X, rect.Position.Y, rect.Size.Width, rect.Size.Height);
    }
}

void CGLEngine::ClearScissorList()
{
    ScopedPerfMarker(__FUNCTION__);
    m_ScissorList.clear();
    glDisable(GL_SCISSOR_TEST);
}

inline void CGLEngine::BindTexture(GLuint texture)
{
    ScopedPerfMarker(__FUNCTION__);
    assert(texture != 0);
    if (OldTexture != texture)
    {
        OldTexture = texture;
        glBindTexture(GL_TEXTURE_2D, texture);
    }
}

void CGLEngine::DrawLine(int x, int y, int targetX, int targetY)
{
    ScopedPerfMarker(__FUNCTION__);

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_LINES);
    glVertex2i(x, y);
    glVertex2i(targetX, targetY);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

void CGLEngine::DrawPolygone(int x, int y, int width, int height)
{
    ScopedPerfMarker(__FUNCTION__);

    glDisable(GL_TEXTURE_2D);

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2i(0, height);
    glVertex2i(width, height);
    glVertex2i(0, 0);
    glVertex2i(width, 0);
    glEnd();

    glTranslatef((GLfloat)-x, (GLfloat)-y, 0.0f);

    glEnable(GL_TEXTURE_2D);
}

void CGLEngine::DrawCircle(float x, float y, float radius, int gradientMode)
{
    ScopedPerfMarker(__FUNCTION__);

    glDisable(GL_TEXTURE_2D);

    glTranslatef(x, y, 0.0f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2i(0, 0);

    if (gradientMode != 0)
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float pi = (float)XUO_M_PI * 2.0f;

    for (int i = 0; i <= 360; i++)
    {
        float a = (i / 180.0f) * pi;
        glVertex2f(cos(a) * radius, sin(a) * radius);
    }

    glEnd();

    glTranslatef(-x, -y, 0.0f);

    glEnable(GL_TEXTURE_2D);
}

void CGLEngine::DrawLandTexture(const CGLTexture &texture, int x, int y, CLandObject *land)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    float translateX = x - 22.0f;
    float translateY = y - 22.0f;

    const auto &rc = land->m_Rect;
    CVector *normals = land->m_Normals;

    glTranslatef(translateX, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f((GLfloat)normals[0].X, (GLfloat)normals[0].Y, (GLfloat)normals[0].Z);
    glTexCoord2i(0, 0);
    glVertex2i(22, -rc.x); //^

    glNormal3f((GLfloat)normals[3].X, (GLfloat)normals[3].Y, (GLfloat)normals[3].Z);
    glTexCoord2i(0, 1);
    glVertex2i(0, 22 - rc.y); //<

    glNormal3f((GLfloat)normals[1].X, (GLfloat)normals[1].Y, (GLfloat)normals[1].Z);
    glTexCoord2i(1, 0);
    glVertex2i(44, 22 - rc.h); //>

    glNormal3f((GLfloat)normals[2].X, (GLfloat)normals[2].Y, (GLfloat)normals[2].Z);
    glTexCoord2i(1, 1);
    glVertex2i(22, 44 - rc.w); //v
    glEnd();

    glTranslatef(-translateX, -translateY, 0.0f);
}

void CGLEngine::Draw(const CGLTexture &texture, int x, int y)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    int width = texture.Width;
    int height = texture.Height;

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2i(0, 1);
    glVertex2i(0, height);
    glTexCoord2i(1, 1);
    glVertex2i(width, height);
    glTexCoord2i(0, 0);
    glVertex2i(0, 0);
    glTexCoord2i(1, 0);
    glVertex2i(width, 0);
    glEnd();

    glTranslatef((GLfloat)-x, (GLfloat)-y, 0.0f);
}

void CGLEngine::DrawRotated(const CGLTexture &texture, int x, int y, float angle)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    int width = texture.Width;
    int height = texture.Height;

    GLfloat translateY = (GLfloat)(y - height);

    glTranslatef((GLfloat)x, translateY, 0.0f);

    glRotatef(angle, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2i(0, 1);
    glVertex2i(0, height);
    glTexCoord2i(1, 1);
    glVertex2i(width, height);
    glTexCoord2i(0, 0);
    glVertex2i(0, 0);
    glTexCoord2i(1, 0);
    glVertex2i(width, 0);
    glEnd();

    glRotatef(angle, 0.0f, 0.0f, -1.0f);
    glTranslatef((GLfloat)-x, -translateY, 0.0f);
}

void CGLEngine::DrawMirrored(const CGLTexture &texture, int x, int y, bool mirror)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    int width = texture.Width;
    int height = texture.Height;

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);

    if (mirror)
    {
        glTexCoord2i(0, 1);
        glVertex2i(width, height);
        glTexCoord2i(1, 1);
        glVertex2i(0, height);
        glTexCoord2i(0, 0);
        glVertex2i(width, 0);
        glTexCoord2i(1, 0);
        glVertex2i(0, 0);
    }
    else
    {
        glTexCoord2i(0, 1);
        glVertex2i(0, height);
        glTexCoord2i(1, 1);
        glVertex2i(width, height);
        glTexCoord2i(0, 0);
        glVertex2i(0, 0);
        glTexCoord2i(1, 0);
        glVertex2i(width, 0);
    }

    glEnd();

    glTranslatef((GLfloat)-x, (GLfloat)-y, 0.0f);
}

void CGLEngine::DrawSitting(
    const CGLTexture &texture, int x, int y, bool mirror, float h3mod, float h6mod, float h9mod)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    float width = (float)texture.Width;
    float height = (float)texture.Height;

    float h03 = height * h3mod;
    float h06 = height * h6mod;
    float h09 = height * h9mod;

    float widthOffset = (float)(width + SittingCharacterOffset);
    glBegin(GL_TRIANGLE_STRIP);

    if (mirror)
    {
        if (h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(width, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(0, 0);
            glTexCoord2f(0.0f, h3mod);
            glVertex2f(width, h03);
            glTexCoord2f(1.0f, h3mod);
            glVertex2f(0, h03);
        }

        if (h6mod != 0.0f)
        {
            if (h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(width, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(0, 0);
            }

            glTexCoord2f(0.0f, h6mod);
            glVertex2f(widthOffset, h06);
            glTexCoord2f(1.0f, h6mod);
            glVertex2f(SittingCharacterOffset, h06);
        }

        if (h9mod != 0.0f)
        {
            if (h6mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(widthOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(SittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(widthOffset, h09);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(SittingCharacterOffset, h09);
        }
    }
    else
    {
        if (h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(SittingCharacterOffset, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(widthOffset, 0);
            glTexCoord2f(0.0f, h3mod);
            glVertex2f(SittingCharacterOffset, h03);
            glTexCoord2f(1.0f, h3mod);
            glVertex2f(widthOffset, h03);
        }

        if (h6mod != 0.0f)
        {
            if (h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(SittingCharacterOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(width + SittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, h6mod);
            glVertex2f(0, h06);
            glTexCoord2f(1.0f, h6mod);
            glVertex2f(width, h06);
        }

        if (h9mod != 0.0f)
        {
            if (h6mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(0, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(width, 0);
            }

            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(0, h09);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(width, h09);
        }
    }

    glEnd();

    glTranslatef((GLfloat)-x, (GLfloat)-y, 0.0f);
}

void CGLEngine::DrawShadow(const CGLTexture &texture, int x, int y, bool mirror)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    float width = (float)texture.Width;
    float height = texture.Height / 2.0f;

    GLfloat translateY = (GLfloat)(y + height * 0.75);

    glTranslatef((GLfloat)x, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);

    float ratio = height / width;

    if (mirror)
    {
        glTexCoord2f(0, 1);
        glVertex2f(width, height);
        glTexCoord2f(1, 1);
        glVertex2f(0, height);
        glTexCoord2f(0, 0);
        glVertex2f(width * (ratio + 1.0f), 0);
        glTexCoord2f(1, 0);
        glVertex2f(width * ratio, 0);
    }
    else
    {
        glTexCoord2f(0, 1);
        glVertex2f(0, height);
        glTexCoord2f(1, 1);
        glVertex2f(width, height);
        glTexCoord2f(0, 0);
        glVertex2f(width * ratio, 0);
        glTexCoord2f(1, 0);
        glVertex2f(width * (ratio + 1.0f), 0);
    }

    glEnd();

    glTranslatef((GLfloat)-x, -translateY, 0.0f);
}

void CGLEngine::DrawStretched(
    const CGLTexture &texture, int x, int y, int drawWidth, int drawHeight)
{
    ScopedPerfMarker(__FUNCTION__);

    BindTexture(texture.Texture);

    int width = texture.Width;
    int height = texture.Height;

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    float drawCountX = drawWidth / (float)width;
    float drawCountY = drawHeight / (float)height;

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, drawCountY);
    glVertex2i(0, drawHeight);
    glTexCoord2f(drawCountX, drawCountY);
    glVertex2i(drawWidth, drawHeight);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(0, 0);
    glTexCoord2f(drawCountX, 0.0f);
    glVertex2i(drawWidth, 0);
    glEnd();

    glTranslatef((GLfloat)-x, (GLfloat)-y, 0.0f);
}

void CGLEngine::DrawResizepic(CGLTexture **th, int x, int y, int width, int height)
{
    int offsetTop = std::max(th[0]->Height, th[2]->Height) - th[1]->Height;
    int offsetBottom = std::max(th[5]->Height, th[7]->Height) - th[6]->Height;
    int offsetLeft = std::max(th[0]->Width, th[5]->Width) - th[3]->Width;
    int offsetRight = std::max(th[2]->Width, th[7]->Width) - th[4]->Width;

    for (int i = 0; i < 9; i++)
    {
        BindTexture(th[i]->Texture);
        int drawWidth = th[i]->Width;
        int drawHeight = th[i]->Height;
        float drawCountX = 1.0f;
        float drawCountY = 1.0f;
        int drawX = x;
        int drawY = y;

        switch (i)
        {
            case 1:
            {
                drawX += th[0]->Width;

                drawWidth = width - th[0]->Width - th[2]->Width;

                drawCountX = drawWidth / (float)th[i]->Width;

                break;
            }
            case 2:
            {
                drawX += width - drawWidth;
                drawY += offsetTop;

                break;
            }
            case 3:
            {
                drawY += th[0]->Height;
                drawX += offsetLeft;

                drawHeight = height - th[0]->Height - th[5]->Height;

                drawCountY = drawHeight / (float)th[i]->Height;

                break;
            }
            case 4:
            {
                drawX += width - drawWidth - offsetRight;
                drawY += th[2]->Height;

                drawHeight = height - th[2]->Height - th[7]->Height;

                drawCountY = drawHeight / (float)th[i]->Height;

                break;
            }
            case 5:
            {
                drawY += height - drawHeight;

                break;
            }
            case 6:
            {
                drawX += th[5]->Width;
                drawY += height - drawHeight - offsetBottom;

                drawWidth = width - th[5]->Width - th[7]->Width;

                drawCountX = drawWidth / (float)th[i]->Width;

                break;
            }
            case 7:
            {
                drawX += width - drawWidth;
                drawY += height - drawHeight;

                break;
            }
            case 8:
            {
                drawX += th[0]->Width;
                drawY += th[0]->Height;

                drawWidth = width - th[0]->Width - th[2]->Width;

                drawHeight = height - th[2]->Height - th[7]->Height;

                drawCountX = drawWidth / (float)th[i]->Width;
                drawCountY = drawHeight / (float)th[i]->Height;

                break;
            }
            default:
                break;
        }

        if (drawWidth < 1 || drawHeight < 1)
        {
            continue;
        }

        glTranslatef((GLfloat)drawX, (GLfloat)drawY, 0.0f);

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f, drawCountY);
        glVertex2i(0, drawHeight);
        glTexCoord2f(drawCountX, drawCountY);
        glVertex2i(drawWidth, drawHeight);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(drawCountX, 0.0f);
        glVertex2i(drawWidth, 0);
        glEnd();

        glTranslatef((GLfloat)-drawX, (GLfloat)-drawY, 0.0f);
    }
}

#endif // NEW_RENDERER_ENABLED
