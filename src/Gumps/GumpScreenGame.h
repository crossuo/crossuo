// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpScreenGame : public CGump
{
private:
    static const int ID_GS_RESIZE = 1;

    // canvas size the portrait fit last ran for
    int m_LastCanvasWidth = 0;
    int m_LastCanvasHeight = 0;

    void FitCanvas();
    void ClampToDisplay(int &x, int &y, int &w, int &h);

public:
    CGumpScreenGame();
    virtual ~CGumpScreenGame() override;

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;
    virtual void Draw() override;
    virtual CRenderObject *Select() override;
    virtual void OnLeftMouseButtonDown() override;
    virtual void OnLeftMouseButtonUp() override;
};
