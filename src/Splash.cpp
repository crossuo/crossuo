// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include "Splash.h"

#include "SDL_wrapper.h"
#include "Renderer/RenderAPI.h"
#include "Wisp/WispWindow.h"

#include <common/logging/logging.h>

#include <external/zlib_amalg.h>

#include <string.h>
#include <stdlib.h>

#include "cover.h" // generated: cover_png[], cover_png_size

#include <SDL_events.h>

// The splash draws through the new renderer's command list, which is the only
// API that exposes texture creation and quad drawing. The original GL2 path
// does not have it, so the splash is compiled out there (see Splash.h: the
// entry points are still defined, they just do nothing).
#if !defined(NEW_RENDERER_ENABLED)
#define XUO_SPLASH_DISABLED 1
#endif

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

// determinate progress bar (web bring-up): two 1x1 solid textures stretched
// into a bar under the splash image
texture_handle_t s_barBack = RENDER_TEXTUREHANDLE_INVALID;
texture_handle_t s_barFill = RENDER_TEXTUREHANDLE_INVALID;
float s_progress01 = 0.0f;
bool s_progressValid = false;

bool Splash_CreateSolidTexture(uint8_t r, uint8_t g, uint8_t b, texture_handle_t *out)
{
    uint8_t pixels[4] = { r, g, b, 0xff };
    *out = Render_CreateTexture2D(1, 1, TextureGPUFormat_RGBA4, pixels, TextureFormat_Unsigned_RGBA8);
    return *out != RENDER_TEXTUREHANDLE_INVALID;
}

uint8_t PaethPredictor(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = (p > a) ? (p - a) : (a - p);
    const int pb = (p > b) ? (p - b) : (b - p);
    const int pc = (p > c) ? (p - c) : (c - p);
    if (pa <= pb && pa <= pc)
    {
        return (uint8_t)a;
    }
    if (pb <= pc)
    {
        return (uint8_t)b;
    }
    return (uint8_t)c;
}

uint32_t ReadBE32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// Decodes the embedded PNG into s_image. Supports the subset the build pipeline
// produces (8-bit, non-interlaced) for color types 0/2/4/6, which covers RGB
// covers and lets the resource be swapped for an RGBA or grayscale one later.
bool DecodeCoverPng()
{
    if (s_decoded)
    {
        return s_image.pixels != nullptr;
    }
    s_decoded = true;

    static const uint8_t pngSignature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    if (cover_png_size < 8 || memcmp(cover_png, pngSignature, sizeof(pngSignature)) != 0)
    {
        Warning(Client, "splash: embedded cover.png is not a PNG");
        return false;
    }

    const uint8_t *const end = cover_png + cover_png_size;
    const uint8_t *p = cover_png + sizeof(pngSignature);

    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bitDepth = 0;
    uint8_t colorType = 0;
    bool interlaced = false;

    // The compressed image data is gathered into one contiguous buffer (the
    // chunks are usually split every 8k, decoding them separately would need a
    // streaming inflate for no benefit).
    uint8_t *compressed = nullptr;
    size_t compressedSize = 0;
    size_t compressedCapacity = 0;

    while (p + 8 <= end)
    {
        const uint32_t chunkLength = ReadBE32(p);
        const char *chunkType = (const char *)(p + 4);
        const uint8_t *chunkData = p + 8;
        if (chunkData + chunkLength + 4 > end)
        {
            break;
        }

        if (memcmp(chunkType, "IHDR", 4) == 0 && chunkLength == 13)
        {
            width = ReadBE32(chunkData);
            height = ReadBE32(chunkData + 4);
            bitDepth = chunkData[8];
            colorType = chunkData[9];
            interlaced = chunkData[12] != 0;
        }
        else if (memcmp(chunkType, "IDAT", 4) == 0 && chunkLength > 0)
        {
            if (compressedSize + chunkLength > compressedCapacity)
            {
                const size_t newCapacity = (compressedCapacity == 0) ? chunkLength : compressedCapacity * 2;
                uint8_t *newBuffer = (uint8_t *)realloc(compressed, newCapacity);
                if (newBuffer == nullptr)
                {
                    free(compressed);
                    Warning(Client, "splash: out of memory reading cover.png");
                    return false;
                }
                compressed = newBuffer;
                compressedCapacity = newCapacity;
            }
            memcpy(compressed + compressedSize, chunkData, chunkLength);
            compressedSize += chunkLength;
        }
        else if (memcmp(chunkType, "IEND", 4) == 0)
        {
            break;
        }

        p = chunkData + chunkLength + 4; // skip data + CRC
    }

    bool ok = false;
    uint8_t *rgb = nullptr;
    do
    {
        if (width == 0 || height == 0 || compressed == nullptr)
        {
            Warning(Client, "splash: cover.png has no image data");
            break;
        }
        if (bitDepth != 8 || interlaced)
        {
            Warning(Client, "splash: cover.png must be 8-bit and non-interlaced");
            break;
        }

        uint32_t channels = 0;
        switch (colorType)
        {
            case 0: // grayscale
                channels = 1;
                break;
            case 2: // truecolor
                channels = 3;
                break;
            case 4: // grayscale + alpha
                channels = 2;
                break;
            case 6: // truecolor + alpha
                channels = 4;
                break;
            default:
                break;
        }
        if (channels == 0)
        {
            Warning(Client, "splash: cover.png color type %d is not supported", (int)colorType);
            break;
        }

        const size_t stride = (size_t)width * channels;
        const size_t filteredSize = (stride + 1) * height;
        uint8_t *filtered = (uint8_t *)malloc(filteredSize);
        if (filtered == nullptr)
        {
            Warning(Client, "splash: out of memory decoding cover.png");
            break;
        }

        uLongf decodedSize = (uLongf)filteredSize;
        const int zret = uncompress(filtered, &decodedSize, compressed, (uLong)compressedSize);
        if (zret != Z_OK || decodedSize != filteredSize)
        {
            Warning(Client, "splash: cover.png inflate failed (%d)", zret);
            free(filtered);
            break;
        }

        rgb = (uint8_t *)malloc(stride * height);
        if (rgb == nullptr)
        {
            Warning(Client, "splash: out of memory decoding cover.png");
            free(filtered);
            break;
        }

        // Undo the per-scanline PNG filters (in place, scanline order).
        const uint8_t *src = filtered;
        uint8_t *dst = rgb;
        for (uint32_t y = 0; y < height; ++y)
        {
            const uint8_t filter = *src++;
            const uint8_t *prev = (y == 0) ? nullptr : (dst - stride);
            for (size_t x = 0; x < stride; ++x)
            {
                const uint8_t raw = src[x];
                const uint8_t left = (x >= channels) ? dst[x - channels] : 0;
                const uint8_t up = (prev != nullptr) ? prev[x] : 0;
                const uint8_t upLeft = (prev != nullptr && x >= channels) ? prev[x - channels] : 0;
                switch (filter)
                {
                    case 0:
                        dst[x] = raw;
                        break;
                    case 1:
                        dst[x] = (uint8_t)(raw + left);
                        break;
                    case 2:
                        dst[x] = (uint8_t)(raw + up);
                        break;
                    case 3:
                        dst[x] = (uint8_t)(raw + ((left + up) >> 1));
                        break;
                    case 4:
                        dst[x] = (uint8_t)(raw + PaethPredictor(left, up, upLeft));
                        break;
                    default:
                        dst[x] = raw;
                        break;
                }
            }
            src += stride;
            dst += stride;
        }
        free(filtered);

        // Expand to the 32bpp layout Render_CreateTexture2D expects for
        // TextureFormat_Unsigned_RGBA8, which differs per backend:
        //
        //  - desktop GL (USE_GL): uploads GL_BGRA + GL_UNSIGNED_INT_8_8_8_8,
        //    which samples the bytes as A,R,G,B (empirically verified against
        //    this renderer's texture path).
        //  - GLES: uploads GL_RGBA + GL_UNSIGNED_BYTE, i.e. plain R,G,B,A.
        //
        // Note this is NOT the ABGR8888 order the game's own 16/32bpp sprites
        // use: those go through the RGB5_A1/Unsigned_A1_BGR5 format instead.
#if defined(USE_GL)
        // index of each output byte holding R, G, B
        const int red = 1;
        const int green = 2;
        const int blue = 3;
        const int alpha = 0;
#else
        const int red = 0;
        const int green = 1;
        const int blue = 2;
        const int alpha = 3;
#endif
        uint8_t *pixels32 = (uint8_t *)malloc((size_t)width * height * 4);
        if (pixels32 == nullptr)
        {
            Warning(Client, "splash: out of memory decoding cover.png");
            break;
        }
        for (size_t i = 0, count = (size_t)width * height; i < count; ++i)
        {
            const uint8_t *in = rgb + i * channels;
            uint8_t *out = pixels32 + i * 4;
            switch (colorType)
            {
                case 0: // grayscale
                    out[red] = out[green] = out[blue] = in[0];
                    out[alpha] = 0xff;
                    break;
                case 2: // truecolor
                    out[red] = in[0];
                    out[green] = in[1];
                    out[blue] = in[2];
                    out[alpha] = 0xff;
                    break;
                case 4: // grayscale + alpha
                    out[red] = out[green] = out[blue] = in[0];
                    out[alpha] = in[1];
                    break;
                default: // truecolor + alpha
                    out[red] = in[0];
                    out[green] = in[1];
                    out[blue] = in[2];
                    out[alpha] = in[3];
                    break;
            }
        }

        s_image.width = width;
        s_image.height = height;
        s_image.pixels = pixels32;
        ok = true;
    } while (false);

    free(rgb);
    free(compressed);
    return ok;
}

} // namespace

bool Splash_Init()
{
    if (s_uploaded)
    {
        return true;
    }

#if defined(XUO_SPLASH_DISABLED)
    return false;
#else
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

    if (!Splash_CreateSolidTexture(24, 24, 28, &s_barBack) ||
        !Splash_CreateSolidTexture(32, 160, 255, &s_barFill))
    {
        Warning(Client, "splash: could not create the progress bar textures");
    }

    s_uploaded = true;
    return true;
#endif // #if defined(XUO_SPLASH_DISABLED)
}

bool Splash_IsReady()
{
    return s_uploaded;
}

void Splash_SetProgress(float done01)
{
    if (done01 < 0.0f)
        done01 = 0.0f;
    if (done01 > 1.0f)
        done01 = 1.0f;
    s_progress01 = done01;
    s_progressValid = true;
}

void Splash_Shutdown()
{
#if !defined(XUO_SPLASH_DISABLED)
    if (s_texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        Render_DestroyTexture(s_texture);
        s_texture = RENDER_TEXTUREHANDLE_INVALID;
    }
    s_uploaded = false;
#endif // #if !defined(XUO_SPLASH_DISABLED)
}

void Splash_Render()
{
#if defined(XUO_SPLASH_DISABLED)
    return;
#else
    if (!s_uploaded || s_texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        return;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(Wisp::g_WispWindow->m_window, &windowWidth, &windowHeight);
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
    if (s_progressValid)
    {
        // progress bar under the splash image (web bring-up)
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
    RenderDraw_Execute(g_renderCmdList);
    Render_SwapBuffers();
#endif // #if defined(XUO_SPLASH_DISABLED)
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
        if (Wisp::g_WispWindow->OnWindowProc(event))
        {
            // a quit request was queued: let the main loop handle it
            SDL_PushEvent(&event);
            break;
        }
    }
}
