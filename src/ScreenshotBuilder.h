// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CScreenshotBuilder
{
public:
    CScreenshotBuilder();
    virtual ~CScreenshotBuilder();

    void SaveScreen();
    void SaveScreen(int x, int y, int width, int height);

    void GPUDataReady();

private:
    void GetScenePixels(int x, int y, int width, int height);
    void WritePixelsToDisk();

    bool m_WaitingForGPUData;
    uint32_t m_Width;
    uint32_t m_Height;
    std::vector<uint32_t> m_Pixels;
};

extern CScreenshotBuilder g_ScreenshotBuilder;
