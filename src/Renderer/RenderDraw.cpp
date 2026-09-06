// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#include <cmath> // cos, sin
#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Utility/PerfMarker.h"
#include <external/gfx/gfx.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <assert.h>
#include <string.h> // memcmp, memcpy
#define XUO_M_PI 3.14159265358979323846264338327950288

#define MATCH_CASE_DRAW(type, cmd, state)                                                          \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        RenderDraw_##type(*(const type##Cmd *)cmd, state);                                         \
        cmd += sizeof(type##Cmd);                                                                  \
        break;                                                                                     \
    }

#if defined(USE_GLES) || defined(USE_GL3)
extern uint32_t _defaultTex;
extern int _inPos;
extern int _inColor;
extern int _inUV;
extern int _uProjectionView;
extern int _uModel;
extern int _uTex;
extern int _pProg;
extern int _pProgLand;
extern int _inNormalLand;
extern int _uDrawModeLand;
extern int _uColorsLand;
#endif

#include <queue>
static std::deque<SetScissorCmd> s_ScissorList;

void Render_PushScissor(int x, int y, uint32_t w, uint32_t h)
{
    ScopedPerfMarker(__FUNCTION__);
    auto cmd = SetScissorCmd{ x, y, w, h };
    s_ScissorList.push_back(cmd);
    RenderAdd_SetScissor(g_renderCmdList, cmd);
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
        RenderAdd_DisableScissor(g_renderCmdList);
    }
    else
    {
        SetScissorCmd &cmd = s_ScissorList.back();
        RenderAdd_SetScissor(g_renderCmdList, cmd);
    }
}

bool RenderDraw_SetTexture(const SetTextureCmd &cmd, RenderState *state)
{
    return RenderState_SetTexture(state, cmd.type, cmd.texture);
}

bool RenderDraw_SetFrameBuffer(const SetFrameBufferCmd &cmd, RenderState *state)
{
    return RenderState_SetFrameBuffer(state, cmd.frameBuffer);
}

bool RenderDraw_DrawQuad(const DrawQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    if (cmd.rgba != g_ColorInvalid)
    {
        RenderState_SetColor(state, cmd.rgba);
    }
    // clang-format off
    const float uv[] = {
         0.0f, cmd.v,
        cmd.u, cmd.v,
         0.0f, 0.0f,
        cmd.u, 0.0f,
    };
    const float v[] = {
        0.0f, float(cmd.height),
        float(cmd.width), float(cmd.height),
        0.0f, 0.0f,
        float(cmd.width), 0.0f,
    };
    const float v_mirrored[] = {
        float(cmd.width), float(cmd.height),
        0.0f, float(cmd.height),
        float(cmd.width), 0.0f,
        0.0f, 0.0f,
    };
    // clang-format on
    const auto &vb = cmd.mirrored ? v_mirrored : v;
#if defined(USE_GL2)
    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i < sizeof(v); i += 2)
    {
        glTexCoord2f(uv[i], uv[i + 1]);
        glVertex2i(vb[i], vb[i + 1]);
    }
    /*
    if (!cmd.mirrored)
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(0, GLint(cmd.height));
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(cmd.width, GLint(cmd.height));
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(cmd.width, 0);
    }
    else
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(cmd.width, GLint(cmd.height));
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(0, GLint(cmd.height));
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(cmd.width, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(0, 0);
    }
    */
    glEnd();
    glTranslatef(-(GLfloat)cmd.x, -(GLfloat)cmd.y, 0.0f);
#else
    // TODO: gles - quad
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(cmd.x, cmd.y, 0.0f));

    const GenericVertex data[] = {
        { { -1.0f, -1.0f }, { 0.0f, 0.0f }, 0xff00ffff },
        { { -1.0f, 1.0f }, { 0.0f, 1.0f }, 0xff00ffff },
        { { 1.0f, 1.0f }, { 1.0f, 1.0f }, 0xff00ffff },
        { { 1.0f, -1.0f }, { 1.0f, 1.0f }, 0xff00ffff },
    };
    const unsigned int idx[] = { 0, 1, 2, 3 };
#if !defined(USE_GLES2)
    uint32_t vao;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glBindVertexArray(vao));
#endif // #if !defined(USE_GLES2)
    uint32_t buffers[2];
    GL_CHECK(glGenBuffers(2, buffers));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GenericVertex), data, GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(unsigned int), idx, GL_STATIC_DRAW));

    GL_CHECK(glUseProgram(_pProg));
    GL_CHECK(glUniformMatrix4fv(_uModel, 1, false, glm::value_ptr(model)));
    //GL_CHECK(glVertexAttribPointer(_inUV, 2, GL_FLOAT, GL_FALSE, 0, uv));
    //GL_CHECK(glEnableVertexAttribArray(_inUV));
    //GL_CHECK(glVertexAttribPointer(_inPos, 2, GL_FLOAT, GL_FALSE, 0, vb));
    //GL_CHECK(glEnableVertexAttribArray(_inPos));
    GL_CHECK(glUniform1i(_uTex, 0));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    GL_CHECK(glUseProgram(0));

    GL_CHECK(glDeleteBuffers(2, buffers));
#if !defined(USE_GLES2)
    GL_CHECK(glDeleteVertexArrays(1, &vao));
#endif // #if !defined(USE_GLES2)
#endif
    return true;
}

bool RenderDraw_DrawRotatedQuad(const DrawRotatedQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    if (cmd.rgba != g_ColorInvalid)
    {
        RenderState_SetColor(state, cmd.rgba);
    }
    // clang-format off
    const float uv[] = {
         0.0f, cmd.v,
        cmd.u, cmd.v,
         0.0f, 0.0f,
        cmd.u, 0.0f,
    };
    const float v[] = {
        0.0f, float(cmd.height),
        float(cmd.width), float(cmd.height),
        0.0f, 0.0f,
        float(cmd.width), 0.0f,
    };
    const float v_mirrored[] = {
        float(cmd.width), float(cmd.height),
        0.0f, float(cmd.height),
        float(cmd.width), 0.0f,
        0.0f, 0.0f,
    };
    // clang-format on
    const auto &vb = cmd.mirrored ? v_mirrored : v;
#if defined(USE_GL2)
    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);
    glRotatef(cmd.angle, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i < sizeof(v); i += 2)
    {
        glTexCoord2f(uv[i], uv[i + 1]);
        glVertex2i(vb[i], vb[i + 1]);
    }
    /*if (!cmd.mirrored)
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(0, cmd.height);
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(cmd.width, cmd.height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(cmd.width, 0);
    }
    else
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(cmd.width, cmd.height);
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(0, cmd.height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(cmd.width, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(0, 0);
    }*/
    glEnd();
    glTranslatef(-(GLfloat)cmd.x, -(GLfloat)cmd.y, 0.0f);
    glRotatef(cmd.angle, 0.0f, 0.0f, -1.0f);
#else
    // TODO: gles - rotated quad
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(cmd.x, cmd.y, 0.0f));
    model = glm::rotate(model, cmd.angle, glm::vec3(0.0f, 0.0f, 1.0f));

    const GenericVertex data[] = {
        { { -1.0f, -1.0f }, { 0.0f, 0.0f }, 0xff00ffff },
        { { -1.0f, 1.0f }, { 0.0f, 1.0f }, 0xff00ffff },
        { { 1.0f, 1.0f }, { 1.0f, 1.0f }, 0xff00ffff },
        { { 1.0f, -1.0f }, { 1.0f, 1.0f }, 0xff00ffff },
    };
    const unsigned int idx[] = { 0, 1, 2, 3 };
#if !defined(USE_GLES2)
    uint32_t vao;
    GL_CHECK(glGenVertexArrays(1, &vao));
    GL_CHECK(glBindVertexArray(vao));
#endif // #if !defined(USE_GLES2)
    uint32_t buffers[2];
    GL_CHECK(glGenBuffers(2, buffers));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GenericVertex), data, GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(unsigned int), idx, GL_STATIC_DRAW));

    GL_CHECK(glUseProgram(_pProg));
    GL_CHECK(glUniformMatrix4fv(_uModel, 1, false, glm::value_ptr(model)));
    //GL_CHECK(glVertexAttribPointer(_inUV, 2, GL_FLOAT, GL_FALSE, 0, uv));
    //GL_CHECK(glEnableVertexAttribArray(_inUV));
    //GL_CHECK(glVertexAttribPointer(_inPos, 2, GL_FLOAT, GL_FALSE, 0, vb));
    //GL_CHECK(glEnableVertexAttribArray(_inPos));
    GL_CHECK(glUniform1i(_uTex, 0));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    GL_CHECK(glUseProgram(0));

    GL_CHECK(glDeleteBuffers(2, buffers));
#if !defined(USE_GLES2)
    GL_CHECK(glDeleteVertexArrays(1, &vao));
#endif // #if !defined(USE_GLES2)
#endif
    return true;
}

bool RenderDraw_DrawCharacterSitting(const DrawCharacterSittingCmd &cmd, RenderState *state)
{
    static const auto s_sittingCharacterOffset = 8.f;

    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);

    const auto x = (GLfloat)cmd.x;
    const auto y = (GLfloat)cmd.y;
    const float width = (float)cmd.width;
    const float height = (float)cmd.height;
    const float h03 = height * cmd.h3mod;
    const float h06 = height * cmd.h6mod;
    const float h09 = height * cmd.h9mod;
    const float widthOffset = (float)(width + s_sittingCharacterOffset);

#if defined(USE_GL2)
    glTranslatef(x, y, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);
    if (cmd.mirror)
    {
        // TODO this won't end well... add a isAlmostZeroF(float val)
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
    glTranslatef(-x, -y, 0.0f);
#else
    // TODO: gles
#endif

    return true;
}

bool RenderDraw_DrawLandTile(const DrawLandTileCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    const float translateX = cmd.x - 22.0f;
    const float translateY = cmd.y - 22.0f;
    const auto &rc = cmd.rect;

#if defined(USE_GL2)
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
    glVertex2i(44, 22 - rc.height); //>

    glNormal3f(cmd.normals[2][0], cmd.normals[2][1], cmd.normals[2][2]);
    glTexCoord2i(1, 1);
    glVertex2i(22, 44 - rc.width); //v
    glEnd();

    glTranslatef(-translateX, -translateY, 0.0f);
#else
    // GLES2 implementation with normals and lighting
    struct LandVertex
    {
        float pos[2];
        float uv[2];
        uint32_t color;
        float normal[3];
    };

    // Build vertex data for the land tile quad (triangle strip)
    LandVertex vertices[4];
    const float4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t whiteColor = *(uint32_t *)&white;

    // Vertex 0 (top)
    vertices[0].pos[0] = 22.0f;
    vertices[0].pos[1] = -rc.x;
    vertices[0].uv[0] = 0.0f;
    vertices[0].uv[1] = 0.0f;
    vertices[0].color = whiteColor;
    vertices[0].normal[0] = cmd.normals[0][0];
    vertices[0].normal[1] = cmd.normals[0][1];
    vertices[0].normal[2] = cmd.normals[0][2];

    // Vertex 1 (left)
    vertices[1].pos[0] = 0.0f;
    vertices[1].pos[1] = 22.0f - rc.y;
    vertices[1].uv[0] = 0.0f;
    vertices[1].uv[1] = 1.0f;
    vertices[1].color = whiteColor;
    vertices[1].normal[0] = cmd.normals[3][0];
    vertices[1].normal[1] = cmd.normals[3][1];
    vertices[1].normal[2] = cmd.normals[3][2];

    // Vertex 2 (right)
    vertices[2].pos[0] = 44.0f;
    vertices[2].pos[1] = 22.0f - rc.height;
    vertices[2].uv[0] = 1.0f;
    vertices[2].uv[1] = 0.0f;
    vertices[2].color = whiteColor;
    vertices[2].normal[0] = cmd.normals[1][0];
    vertices[2].normal[1] = cmd.normals[1][1];
    vertices[2].normal[2] = cmd.normals[1][2];

    // Vertex 3 (bottom)
    vertices[3].pos[0] = 22.0f;
    vertices[3].pos[1] = 44.0f - rc.width;
    vertices[3].uv[0] = 1.0f;
    vertices[3].uv[1] = 1.0f;
    vertices[3].color = whiteColor;
    vertices[3].normal[0] = cmd.normals[2][0];
    vertices[3].normal[1] = cmd.normals[2][1];
    vertices[3].normal[2] = cmd.normals[2][2];

    // Create and bind vertex buffer
    uint32_t vbo;
    GL_CHECK(glGenBuffers(1, &vbo));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(LandVertex), vertices, GL_STATIC_DRAW));

    // Use the land tile shader
    GL_CHECK(glUseProgram(_pProgLand));

    // Set up model matrix with translation
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(translateX, translateY, 0.0f));
    GL_CHECK(glUniformMatrix4fv(_uModel, 1, false, glm::value_ptr(model)));

    // Set draw mode uniform
    GL_CHECK(glUniform1i(_uDrawModeLand, cmd.drawMode));

    // Set up vertex attributes
    GL_CHECK(glEnableVertexAttribArray(_inPos));
    GL_CHECK(glVertexAttribPointer(_inPos, 2, GL_FLOAT, GL_FALSE, sizeof(LandVertex), (void*)0));

    GL_CHECK(glEnableVertexAttribArray(_inUV));
    GL_CHECK(glVertexAttribPointer(_inUV, 2, GL_FLOAT, GL_FALSE, sizeof(LandVertex), (void*)offsetof(LandVertex, uv)));

    GL_CHECK(glEnableVertexAttribArray(_inColor));
    GL_CHECK(glVertexAttribPointer(_inColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LandVertex), (void*)offsetof(LandVertex, color)));

    GL_CHECK(glEnableVertexAttribArray(_inNormalLand));
    GL_CHECK(glVertexAttribPointer(_inNormalLand, 3, GL_FLOAT, GL_FALSE, sizeof(LandVertex), (void*)offsetof(LandVertex, normal)));

    // Draw the quad
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    // Cleanup
    GL_CHECK(glDisableVertexAttribArray(_inPos));
    GL_CHECK(glDisableVertexAttribArray(_inUV));
    GL_CHECK(glDisableVertexAttribArray(_inColor));
    GL_CHECK(glDisableVertexAttribArray(_inNormalLand));
    GL_CHECK(glDeleteBuffers(1, &vbo));
    GL_CHECK(glUseProgram(0));
#endif

    return true;
}

bool RenderDraw_DrawShadow(const DrawShadowCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetShaderUniform(
        state, cmd.uniformId, &cmd.uniformValue, ShaderUniformType::ShaderUniformType_Int1);
    RenderState_SetBlend(
        state,
        true,
        BlendFactor::BlendFactor_DstColor,
        BlendFactor::BlendFactor_Zero,
        BlendEquation::BlendEquation_Add);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);

    const auto width = (float)cmd.width;
    const auto height = cmd.height / 2.0f;
    const auto x = GLfloat(cmd.x);
    const auto translateY = GLfloat(cmd.y + height * 0.75);
    const float ratio = height / width;

#if defined(USE_GL2)
    glTranslatef(x, translateY, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);
    if (cmd.mirror)
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
    glTranslatef(-x, -translateY, 0.0f);
#else
    // TODO: gles
#endif

    if (cmd.restoreBlendFunc)
    {
        RenderState_SetBlend(
            state,
            true,
            BlendFactor::BlendFactor_SrcAlpha,
            BlendFactor::BlendFactor_OneMinusSrcAlpha,
            BlendEquation::BlendEquation_Add);
    }
    else
    {
        RenderState_SetBlend(
            state,
            false,
            BlendFactor::BlendFactor_Invalid,
            BlendFactor::BlendFactor_Invalid,
            BlendEquation::BlendEquation_Invalid);
    }

    return true;
}

bool RenderDraw_DrawCircle(const DrawCircleCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    const float pi = (float)XUO_M_PI * 2.0f;
    const auto radius = cmd.radius;

#if defined(USE_GL2)
    glDisable(GL_TEXTURE_2D);
    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(0, 0);
    if (cmd.gradientMode != 0)
    {
        RenderState_SetColor(state, { 0.f, 0.f, 0.f, 0.f });
    }
    for (int i = 0; i <= 360; i++)
    {
        float a = (i / 180.0f) * pi;
        glVertex2f(cos(a) * radius, sin(a) * radius);
    }
    glEnd();
    glTranslatef((GLfloat)-cmd.x, (GLfloat)-cmd.y, 0.0f);
    glEnable(GL_TEXTURE_2D);
#else
    // TODO: gles
#endif

    return true;
}

bool RenderDraw_DrawUntexturedQuad(const DrawUntexturedQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    const auto colored =
        memcmp(g_ColorInvalid.rgba, cmd.color.rgba, sizeof(g_ColorInvalid.rgba)) != 0;
    const auto blend = colored && cmd.color[3] < 1.f;
    if (colored)
    {
        RenderState_SetColor(state, cmd.color);

        if (blend)
        {
            RenderState_SetBlend(
                state,
                true,
                BlendFactor::BlendFactor_SrcAlpha,
                BlendFactor::BlendFactor_OneMinusSrcAlpha,
                BlendEquation::BlendEquation_Add);
        }
    }

#if defined(USE_GL2)
    glDisable(GL_TEXTURE_2D);
    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2i(0, cmd.height);
    glVertex2i(cmd.width, cmd.height);
    glVertex2i(0, 0);
    glVertex2i(cmd.width, 0);
    glEnd();
    glTranslatef((GLfloat)-cmd.x, (GLfloat)-cmd.y, 0.0f);
    glEnable(GL_TEXTURE_2D);
#else
    // TODO: gles
#endif

    if (colored)
    {
        if (blend)
        {
            RenderState_SetBlend(
                state,
                false,
                BlendFactor::BlendFactor_Invalid,
                BlendFactor::BlendFactor_Invalid,
                BlendEquation::BlendEquation_Invalid);
        }
        RenderState_SetColor(state, g_ColorWhite);
    }
    return true;
}

bool RenderDraw_DrawLine(const DrawLineCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    auto colored = memcmp(g_ColorInvalid.rgba, cmd.color.rgba, sizeof(g_ColorInvalid.rgba)) != 0;
    auto blend = colored && cmd.color[3] < 1.f;
    if (colored)
    {
        RenderState_SetColor(state, cmd.color);

        if (blend)
        {
            RenderState_SetBlend(
                state,
                true,
                BlendFactor::BlendFactor_SrcAlpha,
                BlendFactor::BlendFactor_OneMinusSrcAlpha,
                BlendEquation::BlendEquation_Add);
        }
    }

#if defined(USE_GL2)
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    glVertex2i(cmd.x0, cmd.y0);
    glVertex2i(cmd.x1, cmd.y1);
    glEnd();
    glEnable(GL_TEXTURE_2D);
#else
    // TODO: gles
#endif

    if (colored)
    {
        if (blend)
        {
            RenderState_SetBlend(
                state,
                false,
                BlendFactor::BlendFactor_Invalid,
                BlendFactor::BlendFactor_Invalid,
                BlendEquation::BlendEquation_Invalid);
        }

        RenderState_SetColor(state, g_ColorWhite);
    }

    return true;
}

bool RenderDraw_AlphaTest(const AlphaTestCmd &cmd, RenderState *state)
{
    return RenderState_SetAlphaTest(state, true, cmd.func, cmd.ref);
}

bool RenderDraw_DisableAlphaTest(const DisableAlphaTestCmd &, RenderState *state)
{
    return RenderState_SetAlphaTest(state, false, AlphaTestFunc::AlphaTestFunc_Invalid, 0.f);
}

bool RenderDraw_BlendState(const BlendStateCmd &cmd, RenderState *state)
{
    return RenderState_SetBlend(state, true, cmd.src, cmd.dst, cmd.equation);
}

bool RenderDraw_DisableBlendState(const DisableBlendStateCmd &, RenderState *state)
{
    return RenderState_SetBlend(
        state,
        false,
        BlendFactor::BlendFactor_Invalid,
        BlendFactor::BlendFactor_Invalid,
        BlendEquation::BlendEquation_Invalid);
}

bool RenderDraw_FlushState(const FlushStateCmd &cmd, RenderState *state)
{
    return RenderState_FlushState(state);
}

bool RenderDraw_SetViewParams(const SetViewParamsCmd &cmd, RenderState *state)
{
    return RenderState_SetViewParams(
        state,
        cmd.scene_x,
        cmd.scene_y,
        cmd.scene_width,
        cmd.scene_height,
        cmd.window_width,
        cmd.window_height,
        cmd.camera_nearZ,
        cmd.camera_farZ,
        cmd.scene_scale,
        cmd.proj_flipped_y);
}

bool RenderDraw_SetModelViewTranslation(const SetModelViewTranslationCmd &cmd, RenderState *state)
{
    return RenderState_SetModelViewTranslation(state, cmd.pos);
}

bool RenderDraw_SetScissor(const SetScissorCmd &cmd, RenderState *state)
{
    return RenderState_SetScissor(state, true, cmd.x, cmd.y, cmd.width, cmd.height);
}

bool RenderDraw_DisableScissor(const DisableScissorCmd &cmd, RenderState *state)
{
    return RenderState_SetScissor(state, false, 0, 0, 0, 0);
}

bool RenderDraw_StencilState(const StencilStateCmd &cmd, RenderState *state)
{
    return RenderState_SetStencil(
        state, true, cmd.func, cmd.ref, cmd.mask, cmd.stencilFail, cmd.depthFail, cmd.bothFail);
}

bool RenderDraw_DisableStencilState(const DisableStencilStateCmd &, RenderState *state)
{
    return RenderState_SetStencilEnabled(state, false);
}

bool RenderDraw_EnableStencilState(const EnableStencilStateCmd &, RenderState *state)
{
    return RenderState_SetStencilEnabled(state, true);
}

bool RenderDraw_DepthState(const DepthStateCmd &cmd, RenderState *state)
{
    return RenderState_SetDepth(state, true, cmd.func);
}

bool RenderDraw_DisableDepthState(const DisableDepthStateCmd &, RenderState *state)
{
    return RenderState_SetDepthEnabled(state, false);
}

bool RenderDraw_EnableDepthState(const EnableDepthStateCmd &, RenderState *state)
{
    return RenderState_SetDepthEnabled(state, true);
}

bool RenderDraw_SetColorMask(const SetColorMaskCmd &cmd, RenderState *state)
{
    return RenderState_SetColorMask(state, cmd.mask);
}

bool RenderDraw_SetColor(const SetColorCmd &cmd, RenderState *state)
{
    return RenderState_SetColor(state, cmd.color);
}

bool RenderDraw_SetClearColor(const SetClearColorCmd &cmd, RenderState *state)
{
    return RenderState_SetClearColor(state, cmd.color);
}

bool RenderDraw_ClearRT(const ClearRTCmd &cmd, RenderState *)
{
    auto mask = (cmd.clearMask & ClearRT::ClearRT_Color) == ClearRT::ClearRT_Color ?
                    GL_COLOR_BUFFER_BIT :
                    0;
    mask |= (cmd.clearMask & ClearRT::ClearRT_Depth) == ClearRT::ClearRT_Depth ?
                GL_DEPTH_BUFFER_BIT :
                0;
    mask |= (cmd.clearMask & ClearRT::ClearRT_Stencil) == ClearRT::ClearRT_Stencil ?
                GL_STENCIL_BUFFER_BIT :
                0;
    glClear(mask);

    return true;
}

bool RenderDraw_ShaderUniform(const ShaderUniformCmd &cmd, RenderState *state)
{
    return RenderState_SetShaderUniform(state, cmd.id, cmd.value.data, cmd.uniform_type);
}

bool RenderDraw_ShaderLargeUniform(const ShaderLargeUniformCmd &cmd, RenderState *state)
{
    return RenderState_SetShaderLargeUniform(state, cmd.id, cmd.value, cmd.count, cmd.uniform_type);
}

bool RenderDraw_ShaderPipeline(const ShaderPipelineCmd &cmd, RenderState *state)
{
    return RenderState_SetShaderPipeline(state, cmd.pipeline);
}

bool RenderDraw_DisableShaderPipeline(const DisableShaderPipelineCmd &cmd, RenderState *state)
{
    return RenderState_DisableShaderPipeline(state);
}

bool RenderDraw_GetFrameBufferPixels(const GetFrameBufferPixelsCmd &cmd, RenderState *state)
{
#if defined(USE_GL)
    const auto format = GL_UNSIGNED_INT_8_8_8_8_REV;
#else
    const auto format = GL_UNSIGNED_BYTE;
#endif
    const auto bpp = 4;
    auto neededSize = (cmd.width * cmd.height) * bpp;
    assert(cmd.dataSize >= neededSize);
    if (cmd.dataSize < neededSize)
    {
        return false;
    }

    // game viewport isn't scaled, if the OS window is smaller than GameWindowPosY + GameWindowHeight, bottom will
    // be negative by this difference
    int needed_height = cmd.y + cmd.height;
    int bottom = cmd.window_height - needed_height;

    glReadPixels(cmd.x, bottom, cmd.width, cmd.height, GL_RGBA, format, cmd.data);

    return true;
}

bool RenderDraw_Execute(RenderCmdList *cmdList)
{
    if (cmdList->immediateMode)
    {
        return false;
    }

    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    while (cmd < listEnd)
    {
        // RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        // switch (cmdHeader.type)
        RenderCommandType type = *(RenderCommandType *)cmd;
        cmd += sizeof(type);
        switch (type)
        {
            MATCH_CASE_DRAW(DrawQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawRotatedQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawCharacterSitting, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawLandTile, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawShadow, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawCircle, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawUntexturedQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawLine, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ClearRT, cmd, &cmdList->state)

            MATCH_CASE_DRAW(FlushState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetTexture, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetFrameBuffer, cmd, &cmdList->state)
            MATCH_CASE_DRAW(AlphaTest, cmd, &cmdList->state)
            MATCH_CASE_DRAW(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(EnableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(EnableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColorMask, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColor, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetClearColor, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetViewParams, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetModelViewTranslation, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetScissor, cmd, &cmdList->state);
            MATCH_CASE_DRAW(DisableScissor, cmd, &cmdList->state);

            MATCH_CASE_DRAW(ShaderUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderLargeUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderPipeline, cmd, &cmdList->state);
            MATCH_CASE_DRAW(DisableShaderPipeline, cmd, &cmdList->state);

            MATCH_CASE_DRAW(GetFrameBufferPixels, cmd, &cmdList->state);

            default:
                assert(false);
                break;
        }
    }

    return true;
}