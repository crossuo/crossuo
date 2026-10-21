// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include "Splash.h"

#include "SDL_wrapper.h"
#include "Renderer/RenderAPI.h"
#include "GameWindow.h"

#include <common/logging/logging.h>

// stb_image decodes the embedded cover; its implementation is instantiated
// here (the splash TU is always built, unlike the assistant that only uses
// the declarations)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <external/gfx/stb_image.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <stdlib.h>

#include "cover.h" // generated: cover_png[], cover_png_size

#include <SDL_events.h>

// The splash draws through the renderer's command list, which is the only
// API that exposes texture creation and quad drawing.

// The splash image is embedded at build time and decoded straight from memory.
// Everything here runs before the client's data files are loaded, so it must
// stay free of any dependency on CGame/g_FileManager/g_Index/...

namespace
{

struct SplashImage
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t *pixels = nullptr; // 32bpp, layout required by Render_CreateTexture2D
};

SplashImage s_image;
texture_handle_t s_texture = RENDER_TEXTUREHANDLE_INVALID;
bool s_decoded = false;
bool s_uploaded = false;

// determinate progress bar (native bring-up): two 1x1 solid textures
// stretched into a bar under the splash image; web omits the bar - the
// browser shell page renders its own streaming progress line
#if !defined(XUO_EMSC)
texture_handle_t s_barBack = RENDER_TEXTUREHANDLE_INVALID;
texture_handle_t s_barFill = RENDER_TEXTUREHANDLE_INVALID;
float s_progress01 = 0.0f;
bool s_progressValid = false;
#endif

#if !defined(XUO_EMSC)
bool Splash_CreateSolidTexture(uint8_t r, uint8_t g, uint8_t b, texture_handle_t *out)
{
    uint8_t pixels[4] = { r, g, b, 0xff };
#if defined(USE_GL)
    // desktop GL samples this format as A,R,G,B (see DecodeCoverPng) - rotate
    // one byte, or the bar colors come out channel-swapped (blue/magenta)
    const uint8_t a = pixels[3];
    pixels[3] = pixels[2];
    pixels[2] = pixels[1];
    pixels[1] = pixels[0];
    pixels[0] = a;
#endif
    *out = Render_CreateTexture2D(1, 1, TextureGPUFormat_RGBA4, pixels, TextureFormat_Unsigned_RGBA8);
    return *out != RENDER_TEXTUREHANDLE_INVALID;
}
#endif

// Decodes the embedded PNG into s_image: 32bpp in the byte order
// Render_CreateTexture2D expects for TextureFormat_Unsigned_RGBA8, which
// differs per backend:
//
//  - desktop GL (USE_GL): uploads GL_BGRA + GL_UNSIGNED_INT_8_8_8_8, which
//    samples the bytes as A,R,G,B (empirically verified against this
//    renderer's texture path).
//  - GLES: uploads GL_RGBA + GL_UNSIGNED_BYTE, i.e. plain R,G,B,A.
//
// Note this is NOT the ABGR8888 order the game's own 16/32bpp sprites
// use: those go through the RGB5_A1/Unsigned_A1_BGR5 format instead.
bool DecodeCoverPng()
{
    if (s_decoded)
    {
        return s_image.pixels != nullptr;
    }
    s_decoded = true;

    int width = 0;
    int height = 0;
    int channels = 0;
    // force the 4-component output the texture layout needs (stb expands
    // grayscale/RGB/palette inputs and frees its intermediate on its own)
    uint8_t *pixels =
        stbi_load_from_memory(cover_png, (int)cover_png_size, &width, &height, &channels, 4);
    if (pixels == nullptr)
    {
        Warning(Client, "splash: could not decode cover.png (%s)", stbi_failure_reason());
        return false;
    }

#if defined(USE_GL)
    // stb yields R,G,B,A; the desktop GL layout wants A,R,G,B - a rotation
    // by one byte, done in place
    const size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; ++i)
    {
        uint8_t *px = pixels + i * 4;
        const uint8_t alpha = px[3];
        px[3] = px[2];
        px[2] = px[1];
        px[1] = px[0];
        px[0] = alpha;
    }
#endif

    s_image.width = (uint32_t)width;
    s_image.height = (uint32_t)height;
    s_image.pixels = pixels;
    return true;
}

} // namespace

bool Splash_Init()
{
    if (s_uploaded)
    {
        return true;
    }

    if (!DecodeCoverPng())
    {
        return false;
    }

    s_texture = Render_CreateTexture2D(
        s_image.width,
        s_image.height,
        TextureGPUFormat_RGBA4,
        s_image.pixels,
        TextureFormat_Unsigned_RGBA8);
    if (s_texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        Warning(Client, "splash: could not create the cover texture");
        return false;
    }

    // The decoded copy is only needed for the upload.
    free(s_image.pixels);
    s_image.pixels = nullptr;

#if !defined(XUO_EMSC)
    if (!Splash_CreateSolidTexture(26, 18, 12, &s_barBack) ||
        !Splash_CreateSolidTexture(206, 166, 104, &s_barFill))
    {
        Warning(Client, "splash: could not create the progress bar textures");
    }
#endif

    s_uploaded = true;
    return true;
}

bool Splash_IsReady()
{
    return s_uploaded;
}

void Splash_SetProgress(float done01)
{
#if !defined(XUO_EMSC)
    if (done01 < 0.0f)
        done01 = 0.0f;
    if (done01 > 1.0f)
        done01 = 1.0f;
    s_progress01 = done01;
    s_progressValid = true;
#else
    (void)done01;
#endif
}

void Splash_Shutdown()
{
    if (s_texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        Render_DestroyTexture(s_texture);
        s_texture = RENDER_TEXTUREHANDLE_INVALID;
    }
    s_uploaded = false;
}

void Splash_Render()
{
    if (!s_uploaded || s_texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        return;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(g_GameWindow.m_window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0)
    {
        return;
    }

    // Aspect-fit the image into the window, centered (integer math only, this
    // runs before anything else is initialized and must not allocate).
    const int imageWidth = (int)s_image.width;
    const int imageHeight = (int)s_image.height;
    int width = windowWidth;
    int height = windowWidth * imageHeight / imageWidth;
    if (height > windowHeight)
    {
        height = windowHeight;
        width = windowHeight * imageWidth / imageHeight;
    }
    const int x = (windowWidth - width) / 2;
    const int y = (windowHeight - height) / 2;

    Render_ResetCmdList(g_renderCmdList, Render_DefaultState());
    RenderAdd_FlushState(g_renderCmdList);
    RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{});
    RenderAdd_SetViewParams(
        g_renderCmdList,
        SetViewParamsCmd{ 0, 0, windowWidth, windowHeight, windowWidth, windowHeight, -150, 150 });
    RenderAdd_DrawQuad(
        g_renderCmdList, DrawQuadCmd{ s_texture, x, y, (uint32_t)width, (uint32_t)height });
#if !defined(XUO_EMSC)
    if (s_progressValid)
    {
        // progress bar under the splash image (native bring-up); web omits it
        // - the browser shell page renders its own streaming progress line
        const int barWidth = (windowWidth * 3) / 5;
        const int barHeight = 10;
        const int barX = (windowWidth - barWidth) / 2;
        const int barY = windowHeight - (barHeight * 3);
        RenderAdd_DrawQuad(
            g_renderCmdList,
            DrawQuadCmd{ s_barBack, barX, barY, (uint32_t)barWidth, (uint32_t)barHeight });
        const int fill = (int)((int64_t)barWidth * s_progress01);
        if (fill > 0)
        {
            RenderAdd_DrawQuad(
                g_renderCmdList, DrawQuadCmd{ s_barFill, barX, barY, (uint32_t)fill, (uint32_t)barHeight });
        }
    }
#endif
#if defined(USE_GL3) || defined(USE_GLES)
    RenderDraw_Execute(g_renderCmdList); // no-op in immediate mode (GL1)
#endif
    Render_SwapBuffers();
}

void Splash_RenderAndPumpEvents()
{
    Splash_Render();

    // The loading code ahead is a long uninterrupted stretch of work: without
    // this the window would be marked unresponsive by the window manager and
    // the splash would look frozen.
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (g_GameWindow.OnWindowProc(event))
        {
            // a quit request was queued: let the main loop handle it
            SDL_PushEvent(&event);
            break;
        }
    }
}
