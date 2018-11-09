// MIT License
// Copyright (C) August 2016 Hotride

#include "FileSystem.h"
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

CScreenshotBuilder g_ScreenshotBuilder;

CScreenshotBuilder::CScreenshotBuilder()
{
}

CScreenshotBuilder::~CScreenshotBuilder()
{
}

void CScreenshotBuilder::SaveScreen()
{
    DEBUG_TRACE_FUNCTION;
    SaveScreen(0, 0, g_OrionWindow.GetSize().Width, g_OrionWindow.GetSize().Height);
}

void CScreenshotBuilder::SaveScreen(int x, int y, int width, int height)
{
    DEBUG_TRACE_FUNCTION;

    auto path = g_App.ExeFilePath("snapshots");
    fs_path_create(path);

    auto t = time(nullptr);
    auto now = *localtime(&t);
    char buf[100]{};
    sprintf_s(
        buf,
        "/snapshot_d(%d%d%d)_t(%d%d%d)",
        now.tm_year + 1900,
        now.tm_mon,
        now.tm_mday,
        now.tm_hour,
        now.tm_min,
        now.tm_sec);
    path += ToPath(buf);
    vector<uint32_t> pixels = GetScenePixels(x, y, width, height);

#if USE_FREEIMAGE
    FIBITMAP *fBmp =
        FreeImage_ConvertFromRawBits((uint8_t *)&pixels[0], width, height, width * 4, 32, 0, 0, 0);
    FREE_IMAGE_FORMAT format = FIF_BMP;
    switch (g_ConfigManager.ScreenshotFormat)
    {
        case SF_PNG:
        {
            path += ToPath(".png");
            format = FIF_PNG;
            break;
        }
        case SF_TGA:
        {
            path += ToPath(".tiff");
            format = FIF_TIFF;
            break;
        }
        case SF_JPG:
        {
            path += ToPath(".jpeg");
            format = FIF_JPEG;
            break;
        }
        default:
        {
            path += ToPath(".bmp");
            format = FIF_BMP;
            break;
        }
    }
    FreeImage_Save(format, fBmp, CStringFromPath(path));
    FreeImage_Unload(fBmp);
#else
    int result = 0;
    auto data = (void *)&pixels[0];
    stbi_flip_vertically_on_write(true);
    switch (g_ConfigManager.ScreenshotFormat)
    {
        case SF_PNG:
        {
            path += ToPath(".png");
            result = stbi_write_png(CStringFromPath(path), width, height, 4, data, width * 4);
            break;
        }
        case SF_TGA:
        {
            path += ToPath(".tga");
            result = stbi_write_tga(CStringFromPath(path), width, height, 4, data);
            break;
        }
        case SF_JPG:
        {
            path += ToPath(".jpg");
            result = stbi_write_jpg(CStringFromPath(path), width, height, 4, data, 100);
            break;
        }
        default:
        {
            path += ToPath(".bmp");
            result = stbi_write_bmp(CStringFromPath(path), width, height, 4, data);
            break;
        }
    }
    if (result == 0)
    {
        g_Orion.CreateTextMessageF(3, 0, "Failed to write screenshot");
        return;
    }
#endif

    if (g_GameState >= GS_GAME)
    {
        g_Orion.CreateTextMessageF(3, 0, "Screenshot saved to: %s", CStringFromPath(path));
    }
}

vector<uint32_t> CScreenshotBuilder::GetScenePixels(int x, int y, int width, int height)
{
    DEBUG_TRACE_FUNCTION;
    vector<uint32_t> pixels(width * height);

    glReadPixels(
        x,
        g_OrionWindow.GetSize().Height - y - height,
        width,
        height,
#if USE_FREEIMAGE        
        GL_BGRA,
#else
        GL_RGBA,
#endif
        GL_UNSIGNED_INT_8_8_8_8_REV,
        &pixels[0]);

    for (uint32_t &i : pixels)
    {
        i |= 0xFF000000;
    }

    return pixels;
}
