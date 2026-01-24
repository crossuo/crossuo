// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#if defined(NEW_RENDERER_ENABLED) && !defined(RENDERER_LEGACY) && (defined(USE_GL3) || defined(USE_GLES))
#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "Debug/RenderDebug.h"
#include "../Utility/PerfMarker.h"
#include <external/gfx/gfx.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <assert.h>
#include <string.h> // memcmp, memcpy
#include <deque> // deque

#define XUO_M_PI 3.14159265358979323846264338327950288
#define NORMAL_IDENTITY { 0.0f, 0.0f, 1.0f }
#define MATCH_CASE_DRAW(type, cmd, state)                                                          \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        RenderDraw_##type(*(const type##Cmd *)cmd, state);                                         \
        cmd += sizeof(type##Cmd);                                                                  \
        break;                                                                                     \
    }

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
    const bool colored = (cmd.color != g_ColorInvalid);
    const uint32_t col = colored ?
        (((uint32_t)(cmd.color[0] * 255) << 0) |
         ((uint32_t)(cmd.color[1] * 255) << 8) |
         ((uint32_t)(cmd.color[2] * 255) << 16) |
         ((uint32_t)(cmd.color[3] * 255) << 24)) : 0xffffffff; // BUG with light - g_LightBuffer.Draw

    const GenericVertex data[] = {
        { { vb[0], vb[1] }, { uv[0], uv[1] }, col, NORMAL_IDENTITY },
        { { vb[2], vb[3] }, { uv[2], uv[3] }, col, NORMAL_IDENTITY },
        { { vb[4], vb[5] }, { uv[4], uv[5] }, col, NORMAL_IDENTITY },
        { { vb[6], vb[7] }, { uv[6], uv[7] }, col, NORMAL_IDENTITY },
    };

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));
    model = glm::translate(model, glm::vec3(cmd.x, cmd.y, 0.0f));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(GenericVertex), data));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawQuad");
    RenderState_ResetAllStates(state);

    // FIXME: FORCE RESET STATES TO AVOID GRAPHICAL ISSUES
    // Need to figure out which object is "leaking" state here
    //RenderState_SetBlendEnabled(state, false);

    return true;
}

bool RenderDraw_DrawRotatedQuad(const DrawRotatedQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
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
    const bool colored = (cmd.color != g_ColorInvalid);
    const uint32_t col = colored ?
        (((uint32_t)(cmd.color[0] * 255) << 0) |
         ((uint32_t)(cmd.color[1] * 255) << 8) |
         ((uint32_t)(cmd.color[2] * 255) << 16) |
         ((uint32_t)(cmd.color[3] * 255) << 24)) : 0xffffffff;

    const GenericVertex data[] = {
        { { vb[0], vb[1] }, { uv[0], uv[1] }, col, NORMAL_IDENTITY },
        { { vb[2], vb[3] }, { uv[2], uv[3] }, col, NORMAL_IDENTITY },
        { { vb[4], vb[5] }, { uv[4], uv[5] }, col, NORMAL_IDENTITY },
        { { vb[6], vb[7] }, { uv[6], uv[7] }, col, NORMAL_IDENTITY },
    };

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));
    model = glm::translate(model, glm::vec3(cmd.x, cmd.y, 0.0f));
    model = glm::rotate(model, glm::radians(cmd.angle), glm::vec3(0.0f, 0.0f, 1.0f));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(GenericVertex), data));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawRotatedQuad");
    RenderState_ResetAllStates(state);
    return true;
}

bool RenderDraw_DrawCharacterSitting(const DrawCharacterSittingCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    static const auto s_sittingCharacterOffset = 8.f;
    const auto x = (GLfloat)cmd.x;
    const auto y = (GLfloat)cmd.y;
    const float width = (float)cmd.width;
    const float height = (float)cmd.height;
    const float h03 = height * cmd.h3mod;
    const float h06 = height * cmd.h6mod;
    const float h09 = height * cmd.h9mod;
    const float widthOffset = (float)(width + s_sittingCharacterOffset);

    GenericVertex vertices[10]; // Max 10 vertices for 3 segments
    int vertexCount = 0;
    // Use actual color from state instead of hardcoded white
    const uint32_t col =
        (((uint32_t)(state->color[0] * 255) << 0) |
         ((uint32_t)(state->color[1] * 255) << 8) |
         ((uint32_t)(state->color[2] * 255) << 16) |
         ((uint32_t)(state->color[3] * 255) << 24));

    if (cmd.mirror)
    {
        if (cmd.h3mod != 0.0f)
        {
            vertices[vertexCount++] = { { width, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { width, h03 }, { 0.0f, cmd.h3mod }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { 0.0f, h03 }, { 1.0f, cmd.h3mod }, col, NORMAL_IDENTITY };
        }

        if (cmd.h6mod != 0.0f)
        {
            if (cmd.h3mod == 0.0f)
            {
                vertices[vertexCount++] = { { width, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
                vertices[vertexCount++] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
            }
            vertices[vertexCount++] = { { widthOffset, h06 }, { 0.0f, cmd.h6mod }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { s_sittingCharacterOffset, h06 }, { 1.0f, cmd.h6mod }, col, NORMAL_IDENTITY };
        }

        if (cmd.h9mod != 0.0f)
        {
            if (cmd.h6mod == 0.0f)
            {
                vertices[vertexCount++] = { { widthOffset, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
                vertices[vertexCount++] = { { s_sittingCharacterOffset, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
            }
            vertices[vertexCount++] = { { widthOffset, h09 }, { 0.0f, 1.0f }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { s_sittingCharacterOffset, h09 }, { 1.0f, 1.0f }, col, NORMAL_IDENTITY };
        }
    }
    else
    {
        if (cmd.h3mod != 0.0f)
        {
            vertices[vertexCount++] = { { s_sittingCharacterOffset, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { widthOffset, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { s_sittingCharacterOffset, h03 }, { 0.0f, cmd.h3mod }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { widthOffset, h03 }, { 1.0f, cmd.h3mod }, col, NORMAL_IDENTITY };
        }

        if (cmd.h6mod != 0.0f)
        {
            if (cmd.h3mod == 0.0f)
            {
                vertices[vertexCount++] = { { s_sittingCharacterOffset, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
                vertices[vertexCount++] = { { widthOffset, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
            }
            vertices[vertexCount++] = { { 0.0f, h06 }, { 0.0f, cmd.h6mod }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { width, h06 }, { 1.0f, cmd.h6mod }, col, NORMAL_IDENTITY };
        }

        if (cmd.h9mod != 0.0f)
        {
            if (cmd.h6mod == 0.0f)
            {
                vertices[vertexCount++] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
                vertices[vertexCount++] = { { width, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
            }
            vertices[vertexCount++] = { { 0.0f, h09 }, { 0.0f, 1.0f }, col, NORMAL_IDENTITY };
            vertices[vertexCount++] = { { width, h09 }, { 1.0f, 1.0f }, col, NORMAL_IDENTITY };
        }
    }

    if (vertexCount > 0)
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));
        model = glm::translate(model, glm::vec3(x, y, 0.0f));

        RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
        RenderState_SetupCachedState(state, model);
        RENDER_STATE_DUMP_BEFORE(state);
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
        GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(GenericVertex), vertices));
        GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexCount));
        RENDER_STATE_DUMP_AFTER(state);
        RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawCharacterSitting");
        RenderState_ResetAllStates(state);
    }

    return true;
}

bool RenderDraw_DrawLandTile(const DrawLandTileCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    const float translateX = cmd.x - 22.0f;
    const float translateY = cmd.y - 22.0f;
    const auto &rc = cmd.rect;

    // Build vertex data for land tile (triangle strip)
    const uint32_t col =
        (((uint32_t)(state->color[0] * 255) << 0) |
        ((uint32_t)(state->color[1] * 255) << 8) |
        ((uint32_t)(state->color[2] * 255) << 16) |
        ((uint32_t)(state->color[3] * 255) << 24));

    GenericVertex vertices[4] = {
        // Vertex 0: Bottom-left
        {
            {22.0f, (float)-rc.x},           // pos
            {0.0f, 0.0f},                    // uv
            col,
            {
                (float)cmd.normals[0][0],
                (float)cmd.normals[0][1],
                (float)cmd.normals[0][2]
            }                                 // normal
        },
        // Vertex 1: Top-left
        {
            {0.0f, (float)(22 - rc.y)},      // pos
            {0.0f, 1.0f},                    // uv
            col,
            {
                (float)cmd.normals[3][0],
                (float)cmd.normals[3][1],
                (float)cmd.normals[3][2]
            }
        },
        // Vertex 2: Bottom-right
        {
            {44.0f, (float)(22 - rc.h)},      // pos
            {1.0f, 0.0f},                    // uv
            col,
            {
                (float)cmd.normals[1][0],
                (float)cmd.normals[1][1],
                (float)cmd.normals[1][2]
            }
        },
        // Vertex 3: Top-right
        {
            {22.0f, (float)(44 - rc.w)},      // pos
            {1.0f, 1.0f},                    // uv
            col,
            {
                (float)cmd.normals[2][0],
                (float)cmd.normals[2][1],
                (float)cmd.normals[2][2]
            }
        }
    };

    // Set up model matrix with translation
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0],
                                           state->modelTranslation[1],
                                           state->modelTranslation[2]));
    model = glm::translate(model, glm::vec3(translateX, translateY, 0.0f));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(GenericVertex), vertices));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawLandTile");
    RenderState_ResetAllStates(state);
    return true;
}

bool RenderDraw_DrawShadow(const DrawShadowCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    const auto width = (float)cmd.width;
    const auto height = cmd.height / 2.0f;
    const auto x = GLfloat(cmd.x);
    const auto translateY = GLfloat(cmd.y + height * 0.75);
    const float ratio = height / width;
    // Use actual color from state
    const uint32_t col =
        (((uint32_t)(state->color[0] * 255) << 0) |
         ((uint32_t)(state->color[1] * 255) << 8) |
         ((uint32_t)(state->color[2] * 255) << 16) |
         ((uint32_t)(state->color[3] * 255) << 24));
    GenericVertex data[4];

    if (cmd.mirror)
    {
        data[0] = { { width, height }, { 0.0f, 1.0f }, col, NORMAL_IDENTITY };
        data[1] = { { 0.0f, height }, { 1.0f, 1.0f }, col, NORMAL_IDENTITY };
        data[2] = { { width * (ratio + 1.0f), 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
        data[3] = { { width * ratio, 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
    }
    else
    {
        data[0] = { { 0.0f, height }, { 0.0f, 1.0f }, col, NORMAL_IDENTITY };
        data[1] = { { width, height }, { 1.0f, 1.0f }, col, NORMAL_IDENTITY };
        data[2] = { { width * ratio, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY };
        data[3] = { { width * (ratio + 1.0f), 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY };
    }
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));
    model = glm::translate(model, glm::vec3(x, translateY, 0.0f));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    RenderState_SetBlend(
        state,
        true,
        BlendFactor::BlendFactor_DstColor,
        BlendFactor::BlendFactor_Zero,
        BlendEquation::BlendEquation_Add);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(GenericVertex), data));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    if (cmd.keepBlend)
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
        RenderState_SetBlendEnabled(state, false);
    }
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawShadow");
    RenderState_ResetAllStates(state);
    return true;
}

bool RenderDraw_DrawCircle(const DrawCircleCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    const float pi = (float)XUO_M_PI * 2.0f;
    const auto radius = cmd.radius;
    // Build circle vertices (triangle fan: center + perimeter)
    const int segments = 361; // 0 to 360 degrees
    GenericVertex vertices[segments + 1];
    // Use actual color from state instead of hardcoded white
    const uint32_t centerColor =
        (((uint32_t)(state->color[0] * 255) << 0) |
         ((uint32_t)(state->color[1] * 255) << 8) |
         ((uint32_t)(state->color[2] * 255) << 16) |
         ((uint32_t)(state->color[3] * 255) << 24));
    // Edge color: black if gradient mode is on, otherwise use state color
    const uint32_t edgeColor = cmd.gradientMode != 0 ? 0x00000000 : centerColor;

    // Center vertex
    vertices[0] = { { 0.0f, 0.0f }, { 0.5f, 0.5f }, centerColor, NORMAL_IDENTITY };

    // Perimeter vertices
    for (int i = 0; i <= 360; i++)
    {
        float a = (i / 180.0f) * pi;
        vertices[i + 1] = { { float(cos(a) * radius), float(sin(a) * radius) }, { 0.5f, 0.5f }, edgeColor, NORMAL_IDENTITY };
    }

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));
    model = glm::translate(model, glm::vec3(cmd.x, cmd.y, 0.0f));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, _defaultTex);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, (segments + 1) * sizeof(GenericVertex), vertices));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 1));
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawCircle");
    RenderState_ResetAllStates(state);
    return true;
}

bool RenderDraw_DrawUntexturedQuad(const DrawUntexturedQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    const bool colored = (cmd.color != g_ColorInvalid);
    const uint32_t col = colored ?
        (((uint32_t)(cmd.color[0] * 255) << 0) |
         ((uint32_t)(cmd.color[1] * 255) << 8) |
         ((uint32_t)(cmd.color[2] * 255) << 16) |
         ((uint32_t)(cmd.color[3] * 255) << 24)) : 0xffffffff;

    const GenericVertex data[] = {
        { { 0.0f, float(cmd.height) }, { 0.0f, 1.0f }, col, NORMAL_IDENTITY },
        { { float(cmd.width), float(cmd.height) }, { 1.0f, 1.0f }, col, NORMAL_IDENTITY },
        { { 0.0f, 0.0f }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY },
        { { float(cmd.width), 0.0f }, { 1.0f, 0.0f }, col, NORMAL_IDENTITY },
    };

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));
    model = glm::translate(model, glm::vec3(cmd.x, cmd.y, 0.0f));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, _defaultTex);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, 4 * sizeof(GenericVertex), data));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawUntexturedQuad");
    RenderState_ResetAllStates(state);
    return true;
}

bool RenderDraw_DrawLine(const DrawLineCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetColor(state, cmd.color);
    RenderState_SetBlend(
        state,
        true,
        BlendFactor::BlendFactor_SrcAlpha,
        BlendFactor::BlendFactor_OneMinusSrcAlpha,
        BlendEquation::BlendEquation_Add);

    const bool colored = (cmd.color != g_ColorInvalid);
    const uint32_t col = colored ?
        (((uint32_t)(cmd.color[0] * 255) << 0) |
         ((uint32_t)(cmd.color[1] * 255) << 8) |
         ((uint32_t)(cmd.color[2] * 255) << 16) |
         ((uint32_t)(cmd.color[3] * 255) << 24)) : 0xffffffff;

    const GenericVertex data[] = {
        { { float(cmd.x0), float(cmd.y0) }, { 0.0f, 0.0f }, col, NORMAL_IDENTITY },
        { { float(cmd.x1), float(cmd.y1) }, { 1.0f, 1.0f }, col, NORMAL_IDENTITY },
    };

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(state->modelTranslation[0], state->modelTranslation[1], state->modelTranslation[2]));

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, _defaultTex);
    RenderState_SetupCachedState(state, model);
    RENDER_STATE_DUMP_BEFORE(state);
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(GenericVertex), data));
    GL_CHECK(glDrawArrays(GL_LINES, 0, 2));
    RenderState_SetBlendEnabled(state, false);
    RENDER_STATE_DUMP_AFTER(state);
    RenderDebug_CheckStateLeaks(state, "RenderDraw_DrawLine");
    RenderState_ResetAllStates(state);
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
    return RenderState_SetBlendEnabled(state, false);
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

bool RenderDraw_SetDrawMode(const SetDrawModeCmd &cmd, RenderState *state)
{
    return RenderState_SetDrawMode(state, cmd.drawMode);
}

bool RenderDraw_SetColor(const SetColorCmd &cmd, RenderState *state)
{
    return RenderState_SetColor(state, cmd.color);
}

bool RenderDraw_SetClearColor(const SetClearColorCmd &cmd, RenderState *state)
{
    return RenderState_SetClearColor(state, cmd.color);
}

bool RenderDraw_SetColorPalette(const SetColorPaletteCmd &cmd, RenderState *state)
{
    return RenderState_SetColorPalette(state, cmd.palette);
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
    const auto format = GL_UNSIGNED_BYTE;
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
            MATCH_CASE_DRAW(SetDrawMode, cmd, &cmdList->state)
            MATCH_CASE_DRAW(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(EnableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(EnableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColorMask, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColor, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetClearColor, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColorPalette, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetViewParams, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetModelViewTranslation, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetScissor, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableScissor, cmd, &cmdList->state)

            MATCH_CASE_DRAW(ShaderUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderLargeUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderPipeline, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableShaderPipeline, cmd, &cmdList->state)

            MATCH_CASE_DRAW(GetFrameBufferPixels, cmd, &cmdList->state)
            MATCH_CASE_DRAW(PushDebugMarker, cmd, &cmdList->state)
            MATCH_CASE_DRAW(PopDebugMarker, cmd, &cmdList->state)

            default:
                assert(false);
                break;
        }
    }

    return true;
}
#endif // #if defined(NEW_RENDERER_ENABLED) && !defined(RENDERER_LEGACY) && (defined(USE_GL3) || defined(USE_GLES))
