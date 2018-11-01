// MIT License
// Copyright (C) August 2016 Hotride

#include "stdafx.h"
#include "FileSystem.h"
#include <time.h>

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
    sprintf_s(buf, "/snapshot_d(%d%d%d)_t(%d%d%d)", now.tm_year + 1900, now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);  
    path += ToPath(buf);

    vector<uint32_t> pixels = GetScenePixels(x, y, width, height);

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
        case SF_TIFF:
        {
            path += ToPath(".tiff");
            format = FIF_TIFF;
            break;
        }
        case SF_JPEG:
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

    if (g_GameState >= GS_GAME)
        g_Orion.CreateTextMessageF(3, 0, "Screenshot saved to: %s", CStringFromPath(path));
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
        GL_BGRA,
        GL_UNSIGNED_INT_8_8_8_8_REV,
        &pixels[0]);

    for (uint32_t &i : pixels)
        i |= 0xFF000000;

    return pixels;
}
