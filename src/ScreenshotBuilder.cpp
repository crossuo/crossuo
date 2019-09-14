// MIT License
// Copyright (C) August 2016 Hotride

#include "ScreenshotBuilder.h"
#include "GameWindow.h"
#include "Application.h"
#include <time.h>
#include "CrossUO.h"
#include "Managers/ConfigManager.h"

#define STBIWDEF static inline
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <external/stb_image_write.h>

CScreenshotBuilder g_ScreenshotBuilder;

static std::vector<uint32_t> GetScenePixels(int x, int y, int width, int height)
{
    DEBUG_TRACE_FUNCTION;
    std::vector<uint32_t> pixels(width * height);

    glReadPixels(
        x,
        g_GameWindow.GetSize().Height - y - height,
        width,
        height,
        GL_RGBA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        &pixels[0]);

    for (uint32_t &i : pixels)
    {
        i |= 0xFF000000;
    }

    return pixels;
}

CScreenshotBuilder::CScreenshotBuilder()
{
}

CScreenshotBuilder::~CScreenshotBuilder()
{
}

void CScreenshotBuilder::SaveScreen()
{
    DEBUG_TRACE_FUNCTION;
    SaveScreen(0, 0, g_GameWindow.GetSize().Width, g_GameWindow.GetSize().Height);
}

void CScreenshotBuilder::SaveScreen(int x, int y, int width, int height)
{
    DEBUG_TRACE_FUNCTION;

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
    std::string filename = buf;

    auto pixels = GetScenePixels(x, y, width, height);
    int result = 0;
    auto data = (void *)&pixels[0];
    stbi_flip_vertically_on_write(true);
    switch (g_ConfigManager.ScreenshotFormat)
    {
        case SF_PNG:
        {
            path = fs_join_path(path, filename + ".png");
            result = stbi_write_png(fs_path_ascii(path), width, height, 4, data, width * 4);
            break;
        }
        case SF_TGA:
        {
            path = fs_join_path(path, filename + ".tga");
            result = stbi_write_tga(fs_path_ascii(path), width, height, 4, data);
            break;
        }
        case SF_JPG:
        {
            path = fs_join_path(path, filename + ".jpg");
            result = stbi_write_jpg(fs_path_ascii(path), width, height, 4, data, 100);
            break;
        }
        default:
        {
            path = fs_join_path(path, filename + ".bmp");
            result = stbi_write_bmp(fs_path_ascii(path), width, height, 4, data);
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
