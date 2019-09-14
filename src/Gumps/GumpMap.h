// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpMap : public CGump
{
    int m_PlotState = 0;
    uint32_t m_PinTimer = 0;
    CBaseGUI *m_PinOnCursor = nullptr;
    CGUIButton *m_PlotCourse = nullptr;
    CGUIButton *m_StopPlotting = nullptr;
    CGUIButton *m_ClearCourse = nullptr;
    std::vector<CGUIText *> m_Labels;

    int LineUnderMouse(int &x1, int &y1, int x2, int y2);

public:
    int StartX = 0;
    int StartY = 0;
    int EndX = 0;
    int EndY = 0;
    int Width = 0;
    int Height = 0;
    CGUIExternalTexture *m_Texture = nullptr;
    CGUIDataBox *m_DataBox = nullptr;

    CGumpMap(
        uint32_t serial,
        uint16_t graphic,
        int startX,
        int startY,
        int endX,
        int endY,
        int width,
        int height);
    virtual ~CGumpMap();

    int GetPlotState() { return m_PlotState; };
    void SetPlotState(int val);

    virtual void PrepareContent() override;
    virtual void GenerateFrame(bool stop) override;
    virtual CRenderObject *Select() override;

    GUMP_BUTTON_EVENT_H override;

    virtual void OnLeftMouseButtonDown() override;
    virtual void OnLeftMouseButtonUp() override;
};
