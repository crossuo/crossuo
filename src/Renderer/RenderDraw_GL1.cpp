// AGPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#if defined(NEW_RENDERER_ENABLED) && defined(RENDERER_LEGACY) && defined(USE_GL1)
#include "Globals.h"
#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "Debug/RenderDebug.h"
#include "../Utility/PerfMarker.h"
#include "../GameWindow.h"
#include <external/gfx/gfx.h>
#include <GL/gl.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <deque>

#define XUO_M_PI 3.14159265358979323846264338327950288
static float s_palette[96] = {};
static std::deque<SetScissorCmd> s_ScissorList;

struct
{
    SDL_GLContext context = nullptr;
    SDL_Window *window = nullptr;
} g_render;

void Render_PushScissor(int x, int y, uint32_t w, uint32_t h)
{
    ScopedPerfMarker(__FUNCTION__);
    auto cmd = SetScissorCmd{ x, y, w, h };
    s_ScissorList.push_back(cmd);
    RenderDraw_SetScissor(cmd, nullptr);
}

void Render_PopScissor()
{
    ScopedPerfMarker(__FUNCTION__);
    if (!s_ScissorList.empty())
    {
        s_ScissorList.pop_back();
    }

    if (s_ScissorList.empty())
    {
        DisableScissorCmd cmd;
        RenderDraw_DisableScissor(cmd, nullptr);
    }
    else
    {
        SetScissorCmd &cmd = s_ScissorList.back();
        RenderDraw_SetScissor(cmd, nullptr);
    }
}

bool RenderDraw_SetTexture(const SetTextureCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glBindTexture(GL_TEXTURE_2D, cmd.texture));
    return true;
}

bool RenderDraw_SetFrameBuffer(const SetFrameBufferCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    // Framebuffers not supported in GL1/GL2 fixed pipeline
    return true;
}

bool RenderDraw_DrawQuad(const DrawQuadCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    glBindTexture(GL_TEXTURE_2D, cmd.texture);

    //Info(Renderer, "GL_CALL: translate = %f, %f (%f, %f)", (float)cmd.x, (float)cmd.y, (float)cmd.width, (float)cmd.height);
    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);

    const float drawCountX = cmd.u;
    const float drawCountY = cmd.v;

    glBegin(GL_TRIANGLE_STRIP);
    if (cmd.mirrored)
    {
        glTexCoord2f(0.0f, drawCountY);
        glVertex2i((int)cmd.width, (int)cmd.height);
        glTexCoord2f(drawCountX, drawCountY);
        glVertex2i(0, (int)cmd.height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i((int)cmd.width, 0);
        glTexCoord2f(drawCountX, 0.0f);
        glVertex2i(0, 0);
    }
    else
    {
        glTexCoord2f(0.0f, drawCountY);
        glVertex2i(0, (int)cmd.height);
        glTexCoord2f(drawCountX, drawCountY);
        glVertex2i((int)cmd.width, (int)cmd.height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(drawCountX, 0.0f);
        glVertex2i((int)cmd.width, 0);
    }
    glEnd();

    glTranslatef((GLfloat)-cmd.x, (GLfloat)-cmd.y, 0.0f);

    return true;
}

bool RenderDraw_DrawRotatedQuad(const DrawRotatedQuadCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    glBindTexture(GL_TEXTURE_2D, cmd.texture);

    const float translateY = (float)(cmd.y - (int)cmd.height);
    const float width = (float)cmd.width;
    const float height = (float)cmd.height;

    glTranslatef((GLfloat)cmd.x, translateY, 0.0f);

    glRotatef(cmd.angle, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, cmd.v);
    glVertex2f(0.0f, height);
    glTexCoord2f(cmd.u, cmd.v);
    glVertex2f(width, height);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(cmd.u, 0.0f);
    glVertex2f(width, 0.0f);
    glEnd();

    glRotatef(cmd.angle, 0.0f, 0.0f, -1.0f);
    glTranslatef((GLfloat)-cmd.x, -translateY, 0.0f);

    return true;
}

bool RenderDraw_DrawCharacterSitting(const DrawCharacterSittingCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    static const auto s_sittingCharacterOffset = 8.0f;
    const auto x = (GLfloat)cmd.x;
    const auto y = (GLfloat)cmd.y;
    const float width = (float)cmd.width;
    const float height = (float)cmd.height;
    const float h03 = height * cmd.h3mod;
    const float h06 = height * cmd.h6mod;
    const float h09 = height * cmd.h9mod;
    const float widthOffset = (float)(width + s_sittingCharacterOffset);

    glBindTexture(GL_TEXTURE_2D, cmd.texture);

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);

    if (cmd.mirror)
    {
        if (cmd.h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(width, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(0, 0);
            glTexCoord2f(0.0f, cmd.h3mod);
            glVertex2f(width, h03);
            glTexCoord2f(1.0f, cmd.h3mod);
            glVertex2f(0, h03);
        }

        if (cmd.h6mod != 0.0f)
        {
            if (cmd.h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(width, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(0, 0);
            }

            glTexCoord2f(0.0f, cmd.h6mod);
            glVertex2f(widthOffset, h06);
            glTexCoord2f(1.0f, cmd.h6mod);
            glVertex2f(s_sittingCharacterOffset, h06);
        }

        if (cmd.h9mod != 0.0f)
        {
            if (cmd.h6mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(widthOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(s_sittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(widthOffset, h09);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(s_sittingCharacterOffset, h09);
        }
    }
    else
    {
        if (cmd.h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(s_sittingCharacterOffset, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(widthOffset, 0);
            glTexCoord2f(0.0f, cmd.h3mod);
            glVertex2f(s_sittingCharacterOffset, h03);
            glTexCoord2f(1.0f, cmd.h3mod);
            glVertex2f(widthOffset, h03);
        }

        if (cmd.h6mod != 0.0f)
        {
            if (cmd.h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(s_sittingCharacterOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(width + s_sittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, cmd.h6mod);
            glVertex2f(0, h06);
            glTexCoord2f(1.0f, cmd.h6mod);
            glVertex2f(width, h06);
        }

        if (cmd.h9mod != 0.0f)
        {
            if (cmd.h6mod == 0.0f)
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

    return true;
}

bool RenderDraw_DrawLandTile(const DrawLandTileCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    const float translateX = cmd.x - 22.0f;
    const float translateY = cmd.y - 22.0f;
    const auto &rc = cmd.rect;

    glBindTexture(GL_TEXTURE_2D, cmd.texture);

    glTranslatef(translateX, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(cmd.normals[0][0], cmd.normals[0][1], cmd.normals[0][2]);
    glTexCoord2i(0, 0);
    glVertex2i(22, -rc.x); //^

    glNormal3f(cmd.normals[3][0], cmd.normals[3][1], cmd.normals[3][2]);
    glTexCoord2i(0, 1);
    glVertex2i(0, 22 - rc.y); //<

    glNormal3f(cmd.normals[1][0], cmd.normals[1][1], cmd.normals[1][2]);
    glTexCoord2i(1, 0);
    glVertex2i(44, 22 - rc.h); //>

    glNormal3f(cmd.normals[2][0], cmd.normals[2][1], cmd.normals[2][2]);
    glTexCoord2i(1, 1);
    glVertex2i(22, 44 - rc.w); //v
    glEnd();

    glTranslatef(-translateX, -translateY, 0.0f);

    return true;
}

bool RenderDraw_DrawShadow(const DrawShadowCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    const int width = (int)cmd.width;
    const int height = (int)cmd.height;
    const int x = cmd.x;
    const int y = cmd.y;
    const float ratio = 0.7f;

    glBindTexture(GL_TEXTURE_2D, cmd.texture);

    GLfloat translateY = (GLfloat)(y + height * 0.75);
    glTranslatef((GLfloat)x, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    if (cmd.mirror)
    {
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f((float)width * ratio, (float)height);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(0.0f, (float)height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f((float)width * ratio, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
    }
    else
    {
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, (float)height);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f((float)width * ratio, (float)height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f((float)width * ratio, 0.0f);
    }
    glEnd();

    glTranslatef((GLfloat)-x, -translateY, 0.0f);

    return true;
}

bool RenderDraw_DrawCircle(const DrawCircleCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    const float pi = XUO_M_PI;
    const float radius = cmd.radius;
    const int segments = 32;
    const float centerX = (float)cmd.x;
    const float centerY = (float)cmd.y;
    const uint32_t centerColor = 0xFFFFFFFF;
    const uint32_t edgeColor = 0xFFFFFFFF;

    glTranslatef(centerX, centerY, 0.0f);

    glBegin(GL_TRIANGLE_FAN);
    glColor4ub(centerColor & 0xFF, (centerColor >> 8) & 0xFF, (centerColor >> 16) & 0xFF, (centerColor >> 24) & 0xFF);
    glVertex2f(0.0f, 0.0f);

    glColor4ub(edgeColor & 0xFF, (edgeColor >> 8) & 0xFF, (edgeColor >> 16) & 0xFF, (edgeColor >> 24) & 0xFF);
    for (int i = 0; i <= segments; i++)
    {
        float a = (float)(i * 2.0 * pi / segments);
        glVertex2f(radius * cosf(a), radius * sinf(a));
    }
    glEnd();

    glTranslatef(-centerX, -centerY, 0.0f);

    return true;
}

bool RenderDraw_DrawUntexturedQuad(const DrawUntexturedQuadCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    const int x = cmd.x;
    const int y = cmd.y;
    const int width = (int)cmd.width;
    const int height = (int)cmd.height;
    glColor4f(cmd.color[0], cmd.color[1], cmd.color[2], cmd.color[3]));
    glDisable(GL_TEXTURE_2D));

    glTranslatef((GLfloat)x, (GLfloat)y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2i(0, height);
    glVertex2i(width, height);
    glVertex2i(0, 0);
    glVertex2i(width, 0);
    glEnd();

    glTranslatef((GLfloat)-x, (GLfloat)-y, 0.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);

    return true;
}

bool RenderDraw_DrawLine(const DrawLineCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    const uint32_t col = 0xFFFFFFFF;

    glBegin(GL_LINES);
    glColor4ub(col & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF, (col >> 24) & 0xFF);
    glVertex2i(cmd.x0, cmd.y0);
    glVertex2i(cmd.x1, cmd.y1);
    glEnd();

    return true;
}

bool RenderDraw_PushDebugMarker(const PushDebugMarkerCmd &cmd, RenderState *)
{
    // Debug markers not supported in GL1/GL2
    return true;
}

bool RenderDraw_PopDebugMarker(const PopDebugMarkerCmd &cmd, RenderState *)
{
    // Debug markers not supported in GL1/GL2
    return true;
}

bool RenderDraw_FlushState(const FlushStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    Info(Renderer, "GL_CALL: *** BEGIN FRAME ***");
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    GL_CALL(glLoadIdentity());

    GL_CALL(glDisable(GL_DEPTH_TEST));
    GL_CALL(glDepthFunc(GL_LEQUAL));
    GL_CALL(glDisable(GL_STENCIL_TEST));
    GL_CALL(glDisable(GL_BLEND));

    GL_CALL(glEnable(GL_ALPHA_TEST));
    GL_CALL(glAlphaFunc(GL_GREATER, 0.0f));
    return true;
}

bool RenderDraw_SetViewParams(const SetViewParamsCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GLdouble left = (GLdouble)cmd.scene_x;
    GLdouble right = (GLdouble)(cmd.scene_x + cmd.scene_width);
    GLdouble top = (GLdouble)cmd.scene_y;
    GLdouble bottom = (GLdouble)(cmd.scene_y + cmd.scene_height);

    int viewX = cmd.scene_x;
    int viewY = cmd.window_height - cmd.scene_y - cmd.scene_height;
    int viewW = cmd.scene_width;
    int viewH = cmd.scene_height;
    int newBottom = bottom;
    int newRight = right;
    if (cmd.proj_flipped_y)
    {
        Info(Renderer, "GL_CALL: FLIPPED");
        // Frame buffers are already in OpenGL coordinate space (bottom-left origin)
        // No Y-flip needed, no scaling
    }
    else
    {
        if (cmd.scene_scale != 1.0f)
            Info(Renderer, "GL_CALL: SCALED");
        else
            Info(Renderer, "GL_CALL: NORMAL");

        // Apply global scaling like ViewPortScaled does
        newRight = right * cmd.scene_scale;
        newBottom = bottom * cmd.scene_scale;

        left = (left * cmd.scene_scale) - (newRight - right);
        top = (top * cmd.scene_scale) - (newBottom - bottom);
    }

    Info(Renderer, "GL_CALL: glViewport args: x = %d, y = %d, width = %d, height = %d", viewX, viewY, viewW, viewH);
    glViewport(viewX, viewY, viewW, viewH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Info(Renderer, "GL_CALL: glOrtho args: left = %d, right = %d, bottom = %d, top = %d", (int)left, (int)newRight, (int)newBottom, (int)top);
    glOrtho(left, newRight, newBottom, top, (GLdouble)cmd.camera_nearZ, (GLdouble)cmd.camera_farZ);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    return true;
}

bool HACKRender_SetViewParams(const SetViewParamsCmd &cmd)
{
    ScopedPerfMarker(__FUNCTION__);

    int viewX = cmd.scene_x;
    int viewY = cmd.scene_y;
    int viewW = cmd.window_width;
    int viewH = cmd.window_height;

    Info(Renderer, "GL_CALL: HACKRender_SetViewParams glViewport args: x = %d, y = %d, width = %d, height = %d", viewX, viewY, viewW, viewH);
    glViewport(viewX, viewY, viewW, viewH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Info(Renderer, "GL_CALL: HACKRender_SetViewParams glOrtho args: left = %d, right = %d, bottom = %d, top = %d", viewX, viewY, viewW, viewH);
    glOrtho(viewX, viewY, viewW, viewH, (GLdouble)cmd.camera_nearZ, (GLdouble)cmd.camera_farZ);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    return true;
}



bool RenderDraw_SetModelViewTranslation(const SetModelViewTranslationCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    glTranslatef(
        (GLfloat)cmd.pos[0],
        (GLfloat)cmd.pos[1],
        (GLfloat)cmd.pos[2]);
    return true;
}

bool RenderDraw_SetScissor(const SetScissorCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glEnable(GL_SCISSOR_TEST));
    // Transform scissor coordinates from screen space to OpenGL space
    // Screen space: Y=0 is at top, increases downward
    // OpenGL space: Y=0 is at bottom, increases upward
    int gl_scissor_y = cmd.height - cmd.y;

    Info(Renderer, "GL_CALL: glScissor args: left = %f, right = %f, bottom = %f, top = %f", (float)cmd.x, (float)gl_scissor_y, (float)cmd.width, (float)cmd.height);
    GL_CALL(glScissor(cmd.x, gl_scissor_y, cmd.width, cmd.height));
    return true;
}

bool RenderDraw_DisableScissor(const DisableScissorCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glDisable(GL_SCISSOR_TEST));
    return true;
}

bool RenderDraw_AlphaTest(const AlphaTestCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GLenum func = GL_NEVER;
    switch (cmd.func)
    {
        case AlphaTestFunc::AlphaTestFunc_NeverPass:
            func = GL_NEVER;
            break;
        case AlphaTestFunc::AlphaTestFunc_Less:
            func = GL_LESS;
            break;
        case AlphaTestFunc::AlphaTestFunc_Equal:
            func = GL_EQUAL;
            break;
        case AlphaTestFunc::AlphaTestFunc_LessOrEqual:
            func = GL_LEQUAL;
            break;
        case AlphaTestFunc::AlphaTestFunc_Greater:
            func = GL_GREATER;
            break;
        case AlphaTestFunc::AlphaTestFunc_Different:
            func = GL_NOTEQUAL;
            break;
        case AlphaTestFunc::AlphaTestFunc_GreaterOrEqual:
            func = GL_GEQUAL;
            break;
        case AlphaTestFunc::AlphaTestFunc_AlwaysPass:
            func = GL_ALWAYS;
            break;
        default:
            break;
    }

    GL_CALL(glEnable(GL_ALPHA_TEST));
    GL_CALL(glAlphaFunc(func, cmd.ref));

    return true;
}

bool RenderDraw_DisableAlphaTest(const DisableAlphaTestCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glDisable(GL_ALPHA_TEST));
    return true;
}

bool RenderDraw_BlendState(const BlendStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GLenum src = GL_ONE;
    GLenum dst = GL_ZERO;

    switch (cmd.src)
    {
        case BlendFactor::BlendFactor_Zero:
            src = GL_ZERO;
            break;
        case BlendFactor::BlendFactor_One:
            src = GL_ONE;
            break;
        case BlendFactor::BlendFactor_SrcColor:
            src = GL_SRC_COLOR;
            break;
        case BlendFactor::BlendFactor_OneMinusSrcColor:
            src = GL_ONE_MINUS_SRC_COLOR;
            break;
        case BlendFactor::BlendFactor_DstColor:
            src = GL_DST_COLOR;
            break;
        case BlendFactor::BlendFactor_SrcAlpha:
            src = GL_SRC_ALPHA;
            break;
        case BlendFactor::BlendFactor_OneMinusSrcAlpha:
            src = GL_ONE_MINUS_SRC_ALPHA;
            break;
        default:
            break;
    }

    switch (cmd.dst)
    {
        case BlendFactor::BlendFactor_Zero:
            dst = GL_ZERO;
            break;
        case BlendFactor::BlendFactor_One:
            dst = GL_ONE;
            break;
        case BlendFactor::BlendFactor_SrcColor:
            dst = GL_SRC_COLOR;
            break;
        case BlendFactor::BlendFactor_OneMinusSrcColor:
            dst = GL_ONE_MINUS_SRC_COLOR;
            break;
        case BlendFactor::BlendFactor_DstColor:
            dst = GL_DST_COLOR;
            break;
        case BlendFactor::BlendFactor_SrcAlpha:
            dst = GL_SRC_ALPHA;
            break;
        case BlendFactor::BlendFactor_OneMinusSrcAlpha:
            dst = GL_ONE_MINUS_SRC_ALPHA;
            break;
        default:
            break;
    }

    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(src, dst));

    return true;
}

bool RenderDraw_DisableBlendState(const DisableBlendStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glDisable(GL_BLEND));
    return true;
}

bool RenderDraw_StencilState(const StencilStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GLenum func = GL_NEVER;
    switch (cmd.func)
    {
        case StencilFunc::StencilFunc_NeverPass:
            func = GL_NEVER;
            break;
        case StencilFunc::StencilFunc_Less:
            func = GL_LESS;
            break;
        case StencilFunc::StencilFunc_Equal:
            func = GL_EQUAL;
            break;
        case StencilFunc::StencilFunc_LessOrEqual:
            func = GL_LEQUAL;
            break;
        case StencilFunc::StencilFunc_Greater:
            func = GL_GREATER;
            break;
        case StencilFunc::StencilFunc_Different:
            func = GL_NOTEQUAL;
            break;
        case StencilFunc::StencilFunc_GreaterOrEqual:
            func = GL_GEQUAL;
            break;
        case StencilFunc::StencilFunc_AlwaysPass:
            func = GL_ALWAYS;
            break;
        default:
            break;
    }

    GLenum sfail = GL_KEEP;
    switch (cmd.stencilFail)
    {
        case StencilOp::StencilOp_Keep:
            sfail = GL_KEEP;
            break;
        case StencilOp::StencilOp_Zero:
            sfail = GL_ZERO;
            break;
        case StencilOp::StencilOp_Replace:
            sfail = GL_REPLACE;
            break;
        case StencilOp::StencilOp_IncrementClamp:
            sfail = GL_INCR;
            break;
        case StencilOp::StencilOp_DecrementClamp:
            sfail = GL_DECR;
            break;
        case StencilOp::StencilOp_IncrementWrap:
            sfail = GL_INCR_WRAP;
            break;
        case StencilOp::StencilOp_DecrementWrap:
            sfail = GL_DECR_WRAP;
            break;
        case StencilOp::StencilOp_Invert:
            sfail = GL_INVERT;
            break;
        default:
            break;
    }

    GLenum dpfail = GL_KEEP;
    switch (cmd.depthFail)
    {
        case StencilOp::StencilOp_Keep:
            dpfail = GL_KEEP;
            break;
        case StencilOp::StencilOp_Zero:
            dpfail = GL_ZERO;
            break;
        case StencilOp::StencilOp_Replace:
            dpfail = GL_REPLACE;
            break;
        case StencilOp::StencilOp_IncrementClamp:
            dpfail = GL_INCR;
            break;
        case StencilOp::StencilOp_DecrementClamp:
            dpfail = GL_DECR;
            break;
        case StencilOp::StencilOp_IncrementWrap:
            dpfail = GL_INCR_WRAP;
            break;
        case StencilOp::StencilOp_DecrementWrap:
            dpfail = GL_DECR_WRAP;
            break;
        case StencilOp::StencilOp_Invert:
            dpfail = GL_INVERT;
            break;
        default:
            break;
    }

    GLenum dppass = GL_KEEP;
    switch (cmd.bothFail)
    {
        case StencilOp::StencilOp_Keep:
            dppass = GL_KEEP;
            break;
        case StencilOp::StencilOp_Zero:
            dppass = GL_ZERO;
            break;
        case StencilOp::StencilOp_Replace:
            dppass = GL_REPLACE;
            break;
        case StencilOp::StencilOp_IncrementClamp:
            dppass = GL_INCR;
            break;
        case StencilOp::StencilOp_DecrementClamp:
            dppass = GL_DECR;
            break;
        case StencilOp::StencilOp_IncrementWrap:
            dppass = GL_INCR_WRAP;
            break;
        case StencilOp::StencilOp_DecrementWrap:
            dppass = GL_DECR_WRAP;
            break;
        case StencilOp::StencilOp_Invert:
            dppass = GL_INVERT;
            break;
        default:
            break;
    }

    GL_CALL(glEnable(GL_STENCIL_TEST));
    GL_CALL(glStencilFunc(func, cmd.ref, cmd.mask));
    GL_CALL(glStencilOp(sfail, dpfail, dppass));

    return true;
}

bool RenderDraw_DisableStencilState(const DisableStencilStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glDisable(GL_STENCIL_TEST));
    return true;
}

bool RenderDraw_EnableStencilState(const EnableStencilStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glEnable(GL_STENCIL_TEST));
    return true;
}

bool RenderDraw_DepthState(const DepthStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GLenum func = GL_NEVER;
    switch (cmd.func)
    {
        case DepthFunc::DepthFunc_NeverPass:
            func = GL_NEVER;
            break;
        case DepthFunc::DepthFunc_Less:
            func = GL_LESS;
            break;
        case DepthFunc::DepthFunc_Equal:
            func = GL_EQUAL;
            break;
        case DepthFunc::DepthFunc_LessOrEqual:
            func = GL_LEQUAL;
            break;
        case DepthFunc::DepthFunc_Greater:
            func = GL_GREATER;
            break;
        case DepthFunc::DepthFunc_Different:
            func = GL_NOTEQUAL;
            break;
        case DepthFunc::DepthFunc_GreaterOrEqual:
            func = GL_GEQUAL;
            break;
        case DepthFunc::DepthFunc_AlwaysPass:
            func = GL_ALWAYS;
            break;
        default:
            break;
    }

    GL_CALL(glEnable(GL_DEPTH_TEST));
    GL_CALL(glDepthFunc(func));

    return true;
}

bool RenderDraw_DisableDepthState(const DisableDepthStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glDisable(GL_DEPTH_TEST));
    return true;
}

bool RenderDraw_EnableDepthState(const EnableDepthStateCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glEnable(GL_DEPTH_TEST));
    return true;
}

bool RenderDraw_SetDrawMode(const SetDrawModeCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    // Shaders not supported in GL1/GL2 fixed pipeline - SetDrawMode is a no-op
    return true;
}

bool RenderDraw_SetColorMask(const SetColorMaskCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GLboolean r = (cmd.mask & ColorMask::ColorMask_Red) != 0;
    GLboolean g = (cmd.mask & ColorMask::ColorMask_Green) != 0;
    GLboolean b = (cmd.mask & ColorMask::ColorMask_Blue) != 0;
    GLboolean a = (cmd.mask & ColorMask::ColorMask_Alpha) != 0;
    GL_CALL(glColorMask(r, g, b, a));
    return true;
}

bool RenderDraw_SetColor(const SetColorCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glColor4f(cmd.color[0], cmd.color[1], cmd.color[2], cmd.color[3]));
    return true;
}

bool RenderDraw_SetClearColor(const SetClearColorCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glClearColor(cmd.color[0], cmd.color[1], cmd.color[2], cmd.color[3]));
    return true;
}

bool RenderDraw_SetColorPalette(const SetColorPaletteCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    memcpy(s_palette, cmd.palette, sizeof(s_palette));
    return true;
}

bool RenderDraw_ClearRT(const ClearRTCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    auto mask = 0;
    if (cmd.clearMask & ClearRT::ClearRT_Color)
    {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (cmd.clearMask & ClearRT::ClearRT_Depth)
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (cmd.clearMask & ClearRT::ClearRT_Stencil)
    {
        mask |= GL_STENCIL_BUFFER_BIT;
    }
    GL_CALL(glClear(mask));
    return true;
}

bool RenderDraw_ShaderUniform(const ShaderUniformCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    // Limited shader uniform support in GL1/GL2
    return true;
}

bool RenderDraw_ShaderLargeUniform(const ShaderLargeUniformCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    // Limited shader uniform support in GL1/GL2
    return true;
}

bool RenderDraw_ShaderPipeline(const ShaderPipelineCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    // Shader pipelines not supported in GL1/GL2 fixed pipeline
    return true;
}

bool RenderDraw_DisableShaderPipeline(const DisableShaderPipelineCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    // Shader pipelines not supported in GL1/GL2 fixed pipeline
    return true;
}

bool RenderDraw_GetFrameBufferPixels(const GetFrameBufferPixelsCmd &cmd, RenderState *)
{
    ScopedPerfMarker(__FUNCTION__);
    auto format = GL_RGBA;
    auto bpp = 4;
    auto neededSize = cmd.width * cmd.height * bpp;

    if (cmd.dataSize >= neededSize)
    {
        GL_CALL(glReadPixels(cmd.x, cmd.y, cmd.width, cmd.height, format, GL_UNSIGNED_BYTE, cmd.data));
    }

    return true;
}

texture_handle_t Render_CreateTexture2D(
    uint32_t width,
    uint32_t height,
    TextureGPUFormat gpuFormat,
    void *pixels,
    TextureFormat pixelsFormat)
{
    static GLenum s_gpuFormatToOGLFormat[] = {
        GL_RGBA4,   // RGBA4
        GL_RGB5_A1, // RGB5_A1
    };

    static GLenum s_pixelFormatToOGLFormat[] = {
        GL_UNSIGNED_INT_8_8_8_8,       // TextureFormat_Unsigned_RGBA8
        GL_UNSIGNED_SHORT_1_5_5_5_REV, // TextureFormat_Unsigned_A1_BGR5
    };
    const auto imgFormat = GL_BGRA;

    texture_handle_t tex = RENDER_TEXTUREHANDLE_INVALID;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glBindTexture(GL_TEXTURE_2D, 0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        s_gpuFormatToOGLFormat[gpuFormat],
        width,
        height,
        0,
        imgFormat,
        s_pixelFormatToOGLFormat[pixelsFormat],
        pixels);

    return tex;
}

bool Render_DestroyTexture(texture_handle_t texture)
{
    if (texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        glDeleteTextures(1, &texture);
        return true;
    }
    return false;
}

frame_buffer_t Render_CreateFrameBuffer(uint32_t width, uint32_t height)
{
    texture_handle_t texture;
    framebuffer_handle_t handle;

    GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    //GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glGenTextures(1, &texture));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
    Info(Renderer, "Texture: %d", texture);
    GL_CALL(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8,
        nullptr));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

    GLint currentFrameBuffer = 0;
    GL_CALL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFrameBuffer));
    GL_CALL(glGenFramebuffers(1, &handle));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, handle));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0));

    int glresult;
    GL_CALL(glresult = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    assert(glresult == GL_FRAMEBUFFER_COMPLETE);
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, currentFrameBuffer));

    return frame_buffer_t{ texture, handle };
}

bool Render_DestroyFrameBuffer(frame_buffer_t fb)
{
    if (fb.texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        glDeleteTextures(1, &fb.texture);
    }

    if (fb.handle != RENDER_FRAMEBUFFER_INVALID)
    {
        glDeleteFramebuffers(1, &fb.handle);
    }

    return true;
}

bool Render_CreateShaderPipeline(
    const char *vertexShaderSource,
    const char *fragmentShaderSource,
    ShaderPipeline *pipeline)
{
    // GL1 fixed pipeline doesn't support shaders, return false
    (void)vertexShaderSource;
    (void)fragmentShaderSource;
    (void)pipeline;
    return false;
}

bool Render_DestroyShaderPipeline(ShaderPipeline *pipeline)
{
    // GL1 fixed pipeline doesn't support shaders
    (void)pipeline;
    return true;
}

uint32_t Render_GetUniformId(ShaderPipeline *pipeline, const char *uniform)
{
    // GL1 fixed pipeline doesn't support shaders
    (void)pipeline;
    (void)uniform;
    return 0;
}

bool Render_Init(SDL_Window *window)
{
#ifdef OGL_DEBUGCONTEXT_ENABLED
    auto debugContext = true;
#else
    auto debugContext = false;
#endif
    win_gfx_context_attrbutes(debugContext);
    g_render.window = window;
    g_render.context = SDL_GL_CreateContext(window);
    GL_CALL(SDL_GL_MakeCurrent(window, g_render.context));

    int glewInitResult = glewInit();
    if (glewInitResult != 0)
    {
        Error(Renderer, "glewInit: %s", glewGetErrorString(glewInitResult));
        return false;
    }
    Info(
        Renderer,
        "glew(%s), fb=%i v(%s) (shader: %i)",
        glewGetString(GLEW_VERSION),
        GL_ARB_framebuffer_object,
        glGetString(GL_VERSION),
        GL_ARB_shader_objects);

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
        SDL_GL_DeleteContext(g_render.context);
        g_render.context = nullptr;
        Error(Client, "Your graphics card does not support Frame Buffers");
    }
    GL_CALL(glClearStencil(0));
    GL_CALL(glClearDepthf(1.0));
    GL_CALL(glEnable(GL_TEXTURE_2D));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f)); // Black Background
    GL_CALL(glShadeModel(GL_SMOOTH));              // Enables Smooth Color Shading
    GL_CALL(glClearDepth(1.0));                    // Depth Buffer Setup
    GL_CALL(glDisable(GL_DITHER));

    //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);   //Realy Nice perspective calculations
    GL_CALL(glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST));

    GL_CALL(glEnable(GL_TEXTURE_2D));

    GL_CALL(SDL_GL_SetSwapInterval(0)); // 1 vsync

    GL_CALL(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL));
    GL_CALL(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE));

    GL_CALL(glClearStencil(0));
    GL_CALL(glStencilMask(1));

    GL_CALL(glEnable(GL_LIGHT0));

    GLfloat lightPosition[] = { -1.0f, -1.0f, 0.5f, 0.0f };
    GL_CALL(glLightfv(GL_LIGHT0, GL_POSITION, &lightPosition[0]));

    GLfloat lightAmbient[] = { 2.0f, 2.0f, 2.0f, 1.0f };
    GL_CALL(glLightfv(GL_LIGHT0, GL_AMBIENT, &lightAmbient[0]));

    GLfloat lav = 0.8f;
    GLfloat lightAmbientValues[] = { lav, lav, lav, lav };
    GL_CALL(glLightModelfv(GL_LIGHT_MODEL_AMBIENT, &lightAmbientValues[0]));

    GL_CALL(glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE));

    const auto size = g_GameWindow.GetSize();
    const auto width = size.Width;
    const auto height = size.Height;
    Info(Renderer, "GL_CALL: glViewport args: x = %f, y = %f, width = %f, height = %f", (float)0, (float)0, (float)width, (float)height);
    GL_CALL(glViewport(0, 0, width, height));
    GL_CALL(glMatrixMode(GL_PROJECTION));
    GL_CALL(glLoadIdentity());
    Info(Renderer, "GL_CALL: glOrtho args: left = %f, right = %f, bottom = %f, top = %f, nZ = %f, fZ = %f", (float)0, (float)width, (float)height, (float)0, (float)-150.0, (float)150.0);
    GL_CALL(glOrtho(0, width, height, 0, -150.0, 150.0));
    GL_CALL(glMatrixMode(GL_MODELVIEW));

    return true;
}

void Render_Shutdown()
{
    if (g_render.context != nullptr)
    {
        SDL_GL_DeleteContext(g_render.context);
        g_render.context = nullptr;
    }
}

void Render_SwapBuffers()
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glDisable(GL_ALPHA_TEST));
    GL_CALL(SDL_GL_SwapWindow(g_render.window));
    Info(Renderer, "GL_CALL: *** END FRAME ***");
}

RenderState Render_DefaultState()
{
    RenderState state = {};
    state.color = g_ColorWhite;
    state.clearColor = g_ColorBlack;
    state.colorMask = ColorMask::ColorMask_All;
    return state;
}

bool Render_ResetState()
{
    ScopedPerfMarker(__FUNCTION__);
    GL_CALL(glEnable(GL_TEXTURE_2D));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CALL(glDisable(GL_DEPTH_TEST));
    GL_CALL(glDisable(GL_STENCIL_TEST));
    GL_CALL(glDisable(GL_ALPHA_TEST));
    GL_CALL(glDisable(GL_SCISSOR_TEST));
    GL_CALL(glColor4f(1.0f, 1.0f, 1.0f, 1.0f));
    return true;
}

bool HACKRender_GetFrameBuffer(RenderCmdList *, frame_buffer_t *)
{
    return true;
}

void Render_ResetCmdList(RenderCmdList *, RenderState)
{
}

bool Render_AppendCmd(RenderCmdList *, const void *, uint32_t )
{
    return true;
}

bool Render_AppendCmdType(
    RenderCmdList *, RenderCommandType, const void *, uint32_t)
{
    return true;
}

#endif // defined(NEW_RENDERER_ENABLED) && defined(RENDERER_LEGACY) && defined(USE_GL1)
