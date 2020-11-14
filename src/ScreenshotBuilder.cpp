// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "ScreenshotBuilder.h"
#include "GameWindow.h"
#include "Application.h"
#include <time.h>
#include "CrossUO.h"
#include "Managers/ConfigManager.h"
#include "Renderer/RenderAPI.h"

#define STBIWDEF static inline
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <external/stb_image_write.h>

CScreenshotBuilder g_ScreenshotBuilder;

void CScreenshotBuilder::GetScenePixels(int x, int y, int width, int height)
{
    m_Pixels.resize(width * height);
    m_Width = width;
    m_Height = height;

#ifndef NEW_RENDERER_ENABLED
    glReadPixels(
        x,
        g_GameWindow.GetSize().Height - y - height,
        width,
        height,
        GL_RGBA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        &m_Pixels[0]);
#else
    m_WaitingForGPUData = true;
    const auto windowSize = g_GameWindow.GetSize();
    auto window_width = uint32_t(windowSize.Width);
    auto window_height = uint32_t(windowSize.Height);
    RenderAdd_GetFrameBufferPixels(
        g_renderCmdList,
        GetFrameBufferPixelsCmd{ x,
                                 y,
                                 uint32_t(width),
                                 uint32_t(height),
                                 window_width,
                                 window_height,
                                 &m_Pixels[0],
                                 m_Pixels.size() * sizeof(decltype(m_Pixels)::value_type) });
#endif
}

CScreenshotBuilder::CScreenshotBuilder()
    : m_WaitingForGPUData(false)
    , m_Width(0)
    , m_Height(0)
{
}

CScreenshotBuilder::~CScreenshotBuilder()
{
}

void CScreenshotBuilder::SaveScreen()
{
    SaveScreen(0, 0, g_GameWindow.GetSize().Width, g_GameWindow.GetSize().Height);
}

void CScreenshotBuilder::WritePixelsToDisk()
{
    auto path = g_App.ExeFilePath("screenshots");
    fs_path_create(path);

    auto t = time(nullptr);
    auto now = *localtime(&t);
    char buf[100]{};
    sprintf_s(
        buf,
        "%d%d%d_%d%d%d",
        now.tm_year + 1900,
        now.tm_mon,
        now.tm_mday,
        now.tm_hour,
        now.tm_min,
        now.tm_sec);
    astr_t filename = buf;

    int result = 0;
    auto data = (void *)&m_Pixels[0];
    for (uint32_t &i : m_Pixels)
    {
        i |= 0xFF000000;
    }

    stbi_flip_vertically_on_write(true);
    switch (g_ConfigManager.ScreenshotFormat)
    {
        case SF_PNG:
        {
            path = fs_path_join(path, filename + ".png");
            result = stbi_write_png(fs_path_ascii(path), m_Width, m_Height, 4, data, m_Width * 4);
            break;
        }
        case SF_TGA:
        {
            path = fs_path_join(path, filename + ".tga");
            result = stbi_write_tga(fs_path_ascii(path), m_Width, m_Height, 4, data);
            break;
        }
        case SF_JPG:
        {
            path = fs_path_join(path, filename + ".jpg");
            result = stbi_write_jpg(fs_path_ascii(path), m_Width, m_Height, 4, data, 100);
            break;
        }
        default:
        {
            path = fs_path_join(path, filename + ".bmp");
            result = stbi_write_bmp(fs_path_ascii(path), m_Width, m_Height, 4, data);
            break;
        }
    }
    if (result == 0)
    {
        g_Game.CreateTextMessageF(3, 0, "Failed to write screenshot");
        return;
    }

    if (g_GameState >= GS_GAME)
    {
        g_Game.CreateTextMessageF(3, 0, "Screenshot saved to: %s", fs_path_ascii(path));
    }
}

void CScreenshotBuilder::SaveScreen(int x, int y, int width, int height)
{
    assert(width > 0 && height > 0);

    if (m_WaitingForGPUData)
    {
        g_Game.CreateTextMessageF(3, 0, "A screenshot command is already in progress.");
        return;
    }

    GetScenePixels(x, y, width, height);

#ifndef NEW_RENDERER_ENABLED
    WritePixelsToDisk();
#endif
}

void CScreenshotBuilder::GPUDataReady()
{
#ifdef NEW_RENDERER_ENABLED
    if (m_WaitingForGPUData)
    {
        WritePixelsToDisk();
    }
#endif
    m_WaitingForGPUData = false;
}