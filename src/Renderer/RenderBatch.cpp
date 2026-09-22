// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include "RenderBatch.h"
#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))

#include "RenderAPI.h"
#define RENDERER_INTERNAL
#include "RenderInternal.h"
#include "RenderCommands.h"
#include <external/gfx/gfx.h>
#include <glm/glm.hpp>
#include <stdio.h> // fprintf (XUO_BATCH_LOG)
#include <stdlib.h> // getenv
#include <string.h> // memcmp

namespace {

constexpr int MAX_BATCH_QUADS = 4096;

// Bit-exact float compare matching RenderState.cpp's f32eq: the render-state
// view-params cache compares this way, so the batch has to agree with it on
// what counts as "unchanged" or the two would disagree about flushing.
bool f32eq(float a, float b)
{
    static_assert(sizeof(float) == sizeof(uint32_t), "requires 32-bit float");
    uint32_t ba, bb;
    memcpy(&ba, &a, sizeof(ba));
    memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

struct alignas(4) RenderBatchKey
{
    texture_handle_t texture;
    int drawMode;
    uint32_t paletteVersion;
    float circleTrans[3];
    frame_buffer_t framebuffer;
    decltype(RenderState::alphaTest) alphaTest;
    decltype(RenderState::blend) blend;
    decltype(RenderState::stencil) stencil;
    decltype(RenderState::depth) depth;
    decltype(RenderState::scissor) scissor;
    decltype(RenderState::viewport) viewport;
    ColorMask colorMask;
    uint32_t program;
};

// pending batch state (4 vertices per quad in strip order, expanded to
// 6 triangle-list vertices at flush time only when the batch actually merges)
enum class BatchKind : uint8_t
{
    None,
    Quads,
    LandTiles,
};
RenderBatchKey s_key = {};
GenericVertex s_vertices[MAX_BATCH_QUADS * 4];
GenericVertex s_scratch[MAX_BATCH_QUADS * 6];
int s_quadCount = 0;
bool s_pending = false;
static bool g_batchLog = getenv("XUO_BATCH_LOG") != nullptr;
static float s_layerMin = 1e9f;
static float s_layerMax = -1e9f;
static BatchKind s_kind = BatchKind::None;

RenderBatchKey MakeKey(RenderState *state, texture_handle_t texture)
{
    RenderBatchKey key = {}; // zero-init: keeps memcmp padding-stable
    key.texture = texture;
    key.drawMode = state->currentDrawMode;
    key.paletteVersion = state->paletteVersion;
    key.circleTrans[0] = state->circleTrans[0];
    key.circleTrans[1] = state->circleTrans[1];
    key.circleTrans[2] = state->circleTrans[2];
    key.framebuffer = state->framebuffer;
    key.alphaTest = state->alphaTest;
    key.blend = state->blend;
    key.stencil = state->stencil;
    key.depth = state->depth;
    key.scissor = state->scissor;
    key.viewport = state->viewport;
    key.colorMask = state->colorMask;
    key.program = state->pipeline.program;
    return key;
}

static void AppendStripQuad(
    float mtX,
    float mtY,
    float x,
    float y,
    float w,
    float h,
    float u,
    float v,
    uint32_t col,
    const float *verts4,
    const float *uv4,
    float layer)
{
    if (s_quadCount >= MAX_BATCH_QUADS)
    {
        return;
    }

    // the draw handlers emit a 4 vertex triangle strip per quad
    GenericVertex *out = &s_vertices[s_quadCount * 4];
    for (int i = 0; i < 4; i++)
    {
        out[i].pos[0] = mtX + x + verts4[i * 2 + 0];
        out[i].pos[1] = mtY + y + verts4[i * 2 + 1];
        out[i].uv[0] = uv4[i * 2 + 0];
        out[i].uv[1] = uv4[i * 2 + 1];
        out[i].col = col;
        out[i].normal[0] = 0.0f;
        out[i].normal[1] = 0.0f;
        out[i].normal[2] = 1.0f;
        out[i].layer = layer;
    }
    if (layer < s_layerMin)
    {
        s_layerMin = layer;
    }
    if (layer > s_layerMax)
    {
        s_layerMax = layer;
    }
    s_quadCount++;
}

} // namespace

void RenderBatch_AddQuad(RenderState *state, const DrawQuadCmd &cmd)
{
    // land tiles and quads never overlap (ground vs objects), so the two
    // pending slots accumulate independently and flush on real state changes
    // only - no cross-kind flush here or in AddLandTile
    // array-placed sprites key on the shared array texture so they merge
    // with each other; the per-sprite handle is irrelevant
    const texture_handle_t batchTexture =
        (cmd.layer >= 0.0f) ? Render_ArtArrayTexture() : cmd.texture;
    RenderBatchKey key = MakeKey(state, batchTexture);
    if (s_pending && (memcmp(&key, &s_key, sizeof(key)) != 0 || s_quadCount >= MAX_BATCH_QUADS))
    {
        RenderBatch_Flush(state);
    }
    s_key = key;
    s_pending = true;
    s_kind = BatchKind::Quads;

    const bool colored = (cmd.color != g_ColorInvalid);
    const uint32_t col = colored ?
        (((uint32_t)(cmd.color[0] * 255) << 0) | ((uint32_t)(cmd.color[1] * 255) << 8) |
         ((uint32_t)(cmd.color[2] * 255) << 16) |
         ((uint32_t)(cmd.color[3] * 255) << 24)) :
        0xffffffff;

    // clang-format off
    const float uvs[8] = {
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
    const float vMirrored[] = {
        float(cmd.width), float(cmd.height),
        0.0f, float(cmd.height),
        float(cmd.width), 0.0f,
        0.0f, 0.0f,
    };
    // clang-format on
    AppendStripQuad(
        state->modelTranslation[0],
        state->modelTranslation[1],
        float(cmd.x),
        float(cmd.y),
        float(cmd.width),
        float(cmd.height),
        cmd.u,
        cmd.v,
        col,
        cmd.mirrored ? vMirrored : v,
        uvs,
        cmd.layer);
}

void RenderBatch_AddUntexturedQuad(RenderState *state, const DrawUntexturedQuadCmd &cmd)
{
    RenderBatchKey key = MakeKey(state, _whiteTex);
    if (s_pending && (memcmp(&key, &s_key, sizeof(key)) != 0 || s_quadCount >= MAX_BATCH_QUADS))
    {
        RenderBatch_Flush(state);
    }
    s_key = key;
    s_pending = true;

    const bool colored = (cmd.color != g_ColorInvalid);
    const uint32_t col = colored ?
        (uint32_t(cmd.color[0] * 255.f) | (uint32_t(cmd.color[1] * 255.f) << 8) |
         (uint32_t(cmd.color[2] * 255.f) << 16) | (uint32_t(cmd.color[3] * 255.f) << 24)) :
        0xffffffff;

    // clang-format off
    const float zeroUVs[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    const float v[] = {
        0.0f, float(cmd.height),
        float(cmd.width), float(cmd.height),
        0.0f, 0.0f,
        float(cmd.width), 0.0f,
    };
    // clang-format on
    AppendStripQuad(
        state->modelTranslation[0],
        state->modelTranslation[1],
        float(cmd.x),
        float(cmd.y),
        float(cmd.width),
        float(cmd.height),
        0.0f,
        0.0f,
        col,
        v,
        zeroUVs,
        -1.0f);
}

void RenderBatch_AddLandTile(RenderState *state, const DrawLandTileCmd &cmd)
{
    const texture_handle_t batchTexture =
        (cmd.layer >= 0) ? Render_ArtArrayTexture() : cmd.texture;
    RenderBatchKey key = MakeKey(state, batchTexture);
    if (s_pending && (memcmp(&key, &s_key, sizeof(key)) != 0 || s_quadCount >= MAX_BATCH_QUADS))
    {
        RenderBatch_Flush(state);
    }
    s_key = key;
    s_pending = true;
    s_kind = BatchKind::LandTiles;

    if (s_quadCount >= MAX_BATCH_QUADS)
    {
        return;
    }

    const uint32_t col =
        (((uint32_t)(state->color[0] * 255) << 0) |
        ((uint32_t)(state->color[1] * 255) << 8) |
        ((uint32_t)(state->color[2] * 255) << 16) |
        ((uint32_t)(state->color[3] * 255) << 24));

    const float tx = state->modelTranslation[0] + cmd.x - 22.0f;
    const float ty = state->modelTranslation[1] + cmd.y - 22.0f;

    GenericVertex *out = &s_vertices[s_quadCount * 4];
    // clang-format off
    const float uv[4][2] = { { 0.0f, 0.0f }, { 0.0f, cmd.v }, { cmd.u, 0.0f }, { cmd.u, cmd.v } };
    const float v[4][2] = {
        { 22.0f, float(-cmd.rect.x) },
        { 0.0f,  float(22 - cmd.rect.y) },
        { 44.0f, float(22 - cmd.rect.h) },
        { 22.0f, float(44 - cmd.rect.w) },
    };
    const int n[4] = { 0, 3, 1, 2 };
    // clang-format on
    for (int i = 0; i < 4; i++)
    {
        out[i].pos[0] = tx + v[i][0];
        out[i].pos[1] = ty + v[i][1];
        out[i].uv[0] = uv[i][0];
        out[i].uv[1] = uv[i][1];
        out[i].col = col;
        out[i].normal[0] = cmd.normals[n[i]][0];
        out[i].normal[1] = cmd.normals[n[i]][1];
        out[i].normal[2] = cmd.normals[n[i]][2];
        out[i].layer = float(cmd.layer);
        if (cmd.layer < s_layerMin)
        {
            s_layerMin = cmd.layer;
        }
        if (cmd.layer > s_layerMax)
        {
            s_layerMax = cmd.layer;
        }
    }
    s_quadCount++;
}

void RenderBatch_OnCommand(RenderState *state, RenderCommandType type, const void *cmd)
{
    if (!s_pending)
    {
        return;
    }

    switch (type)
    {
        // state commands that may invalidate the batch: flush only on an
        // actual change (no-op commands are frequent and keep batches alive)
        case RenderCommandType::Cmd_SetTexture:
        {
            auto texture = ((const SetTextureCmd *)cmd)->texture;
            if (texture != s_key.texture)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_SetDrawMode:
        {
            auto drawMode = ((const SetDrawModeCmd *)cmd)->drawMode;
            if (drawMode != s_key.drawMode)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_BlendState:
        {
            auto blend = ((const BlendStateCmd *)cmd);
            if (blend->src != s_key.blend.src || blend->dst != s_key.blend.dst ||
                blend->equation != s_key.blend.equation || !s_key.blend.enabled)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_DisableBlendState:
        {
            if (s_key.blend.enabled)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_AlphaTest:
        {
            auto alpha = ((const AlphaTestCmd *)cmd);
            if (!s_key.alphaTest.enabled || alpha->func != s_key.alphaTest.func ||
                !f32eq(alpha->ref, s_key.alphaTest.alphaRef))
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_DisableAlphaTest:
        {
            if (s_key.alphaTest.enabled)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_SetCircleTrans:
        {
            auto circle = ((const SetCircleTransCmd *)cmd);
            if (!f32eq(circle->x, s_key.circleTrans[0]) || !f32eq(circle->y, s_key.circleTrans[1]) ||
                !f32eq(circle->radius, s_key.circleTrans[2]))
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_SetColorPalette:
        {
            auto palette = ((const SetColorPaletteCmd *)cmd)->palette;
            if (memcmp(palette, state->currentColors, sizeof(state->currentColors)) != 0)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_SetScissor:
        {
            auto scissor = ((const SetScissorCmd *)cmd);
            if (!s_key.scissor.enabled || scissor->x != s_key.scissor.x ||
                scissor->y != s_key.scissor.y || scissor->width != s_key.scissor.width ||
                scissor->height != s_key.scissor.height)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_DisableScissor:
        {
            if (s_key.scissor.enabled)
            {
                RenderBatch_Flush(state);
            }
            break;
        }
        case RenderCommandType::Cmd_SetColorMask:
        {
            if (((const SetColorMaskCmd *)cmd)->mask != s_key.colorMask)
            {
                RenderBatch_Flush(state);
            }
            break;
        }

        // a framebuffer rebind only invalidates the batch when the target
        // actually changes; binding the framebuffer the pending batch was
        // already recorded against (the common "restore the previous FBO" at
        // the end of a pass) keeps the batch alive so its quads can still
        // merge with what follows
        case RenderCommandType::Cmd_SetFrameBuffer:
        {
            auto fb = ((const SetFrameBufferCmd *)cmd)->frameBuffer;
            if (fb.handle != s_key.framebuffer.handle || fb.texture != s_key.framebuffer.texture)
            {
                RenderBatch_Flush(state);
            }
            break;
        }

        // same idea for the projection/viewport: only a real change breaks the
        // batch, so a no-op re-issue (very common - every framebuffer release
        // restores the full-window view) keeps it alive
        case RenderCommandType::Cmd_SetViewParams:
        {
            auto vp = (const SetViewParamsCmd *)cmd;
            const int right = vp->scene_x + vp->scene_width;
            const int bottom = vp->window_height - (vp->scene_y + vp->scene_height);
            const auto &k = s_key.viewport;
            if (k.left != vp->scene_x || k.right != right || k.bottom != bottom ||
                k.top != vp->scene_y || k.nearZ != vp->camera_nearZ ||
                k.farZ != vp->camera_farZ || !f32eq(k.scale, vp->scene_scale) ||
                k.proj_flipped_y != vp->proj_flipped_y)
            {
                RenderBatch_Flush(state);
            }
            break;
        }

        // state commands that always invalidate the batch
        case RenderCommandType::Cmd_ClearRT:
        case RenderCommandType::Cmd_FlushState:
        case RenderCommandType::Cmd_GetFrameBufferPixels:
        case RenderCommandType::Cmd_ShaderUniform:
        case RenderCommandType::Cmd_ShaderLargeUniform:
        case RenderCommandType::Cmd_ShaderPipeline:
        case RenderCommandType::Cmd_DisableShaderPipeline:
        case RenderCommandType::Cmd_StencilState:
        case RenderCommandType::Cmd_EnableStencilState:
        case RenderCommandType::Cmd_DisableStencilState:
        case RenderCommandType::Cmd_DepthState:
        case RenderCommandType::Cmd_EnableDepthState:
        case RenderCommandType::Cmd_DisableDepthState:
        {
            RenderBatch_Flush(state);
            break;
        }

        // commands that do not affect a pending batch
        case RenderCommandType::Cmd_SetColor:            // quads carry vertex colors
        case RenderCommandType::Cmd_SetClearColor:       // only affects clears
        case RenderCommandType::Cmd_SetModelViewTranslation: // baked per vertex
        case RenderCommandType::Cmd_PushDebugMarker:
        case RenderCommandType::Cmd_PopDebugMarker:
        case RenderCommandType::Cmd_DrawQuad:
        case RenderCommandType::Cmd_DrawUntexturedQuad:
        case RenderCommandType::Cmd_DrawLandTile:
        {
            break;
        }

        default:
        {
            RenderBatch_Flush(state);
            break;
        }
    }
}

void RenderBatch_Flush(RenderState *state)
{
    if (s_pending && g_batchLog)
    {
        fprintf(
            stderr,
            "[batch] n=%d tex=%u layer=[%g..%g] kind=%d\n",
            s_quadCount,
            s_key.texture,
            (s_layerMin < 1e8f) ? s_layerMin : -1.0f,
            (s_layerMax > -1e8f) ? s_layerMax : -1.0f,
            (int)s_kind);
    }
    if (s_quadCount == 0)
    {
        s_pending = false;
        return;
    }

    // the land array lives persistently on texture unit 1; skip the sampler2D
    // bind for it (binding it to the GL_TEXTURE_2D target would be invalid)
    if (s_key.texture != Render_ArtArrayTexture())
    {
        RenderState_SetTexture(state, TextureType::TextureType_Texture2D, s_key.texture);
    }

    // vertex positions are baked with the model translation at add time
    const glm::mat4 model(1.0f);
    RenderState_SetupCachedState(state, model);

    if (s_quadCount == 1)
    {
        // single quad: identical to the non-batched draw (triangle strip),
        // merging it would only cost the extra triangle-list vertices
        const size_t vtxOffset = Render_UploadVertices(s_vertices, 4 * sizeof(GenericVertex));
        Render_BindVertexRange(vtxOffset);
        GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    }
    else
    {
        // expand the quad strips into a single triangle list
        static const int indices[6] = { 0, 1, 2, 1, 3, 2 };
        for (int q = 0; q < s_quadCount; q++)
        {
            const GenericVertex *quad = &s_vertices[q * 4];
            for (int i = 0; i < 6; i++)
            {
                s_scratch[q * 6 + i] = quad[indices[i]];
            }
        }

        const size_t vtxOffset =
            Render_UploadVertices(s_scratch, s_quadCount * 6 * sizeof(GenericVertex));
        Render_BindVertexRange(vtxOffset);
        GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, s_quadCount * 6));

        g_frameStats.batchFlushes++;
        g_frameStats.batchedQuads += s_quadCount;
    }

    s_quadCount = 0;
    s_pending = false;
    s_kind = BatchKind::None;
}

#endif // NEW_RENDERER_ENABLED && (USE_GL3 || USE_GLES)
