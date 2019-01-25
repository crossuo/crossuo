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
};

extern CScreenshotBuilder g_ScreenshotBuilder;
