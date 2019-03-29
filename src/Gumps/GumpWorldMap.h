// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpWorldMap : public CGump
{
public:
    int Width = MIN_WORLD_MAP_WIDTH;
    int Height = MIN_WORLD_MAP_HEIGHT;

protected:
    int m_Scale = 2;

public:
    int GetScale() { return m_Scale; };
    void SetScale(int val);

protected:
    int m_Map = 0;

public:
    int GetMap() { return m_Map; };
    void SetMap(int val);

protected:
    bool m_LinkWithPlayer = true;

public:
    bool GetLinkWithPlayer() { return m_LinkWithPlayer; };
    void SetLinkWithPlayer(bool val);
    int OffsetX = 0;
    int OffsetY = 0;
    bool Called = false;
    int CurrentOffsetX = 0;
    int CurrentOffsetY = 0;

private:
    static constexpr int ID_GWM_MINIMIZE = 1;
    static constexpr int ID_GWM_RESIZE = 2;
    static constexpr int ID_GWM_MAP = 3;
    static constexpr int ID_GWM_LINK_WITH_PLAYER = 4;
    static constexpr int ID_GWM_MAP_LIST = 10;
    static constexpr int ID_GWM_SCALE_LIST = 20;

    static constexpr int MIN_WORLD_MAP_HEIGHT = 300;
    static constexpr int MIN_WORLD_MAP_WIDTH = 400;

    int m_StartResizeWidth = 0;
    int m_StartResizeHeight = 0;

    bool m_MapMoving = false;

    void FixOffsets(int &offsetX, int &offsetY, int &width, int &height);
    void GetScaledDimensions(int &width, int &height, int &playerX, int &playerY);
    void GetCurrentCenter(int &x, int &y, int &mouseX, int &mouseY);
    void ScaleOffsets(int newScale, int mouseX, int mouseY);
    void LoadMap(int map);

    int GetCurrentMap();

    CGUIButton *m_Minimizer = nullptr;
    CGUIResizepic *m_Background = nullptr;
    CGUIResizeButton *m_Resizer = nullptr;
    CGUIText *Text = nullptr;
    CGUICheckbox *m_Checkbox = nullptr;
    CGUIScissor *m_Scissor = nullptr;
    CGUIWorldMapTexture *m_MapData = nullptr;
    CGUIComboBox *m_ComboboxScale = nullptr;
    CGUIComboBox *m_ComboboxMap = nullptr;

protected:
    virtual void CalculateGumpState() override;

public:
    CGumpWorldMap(short x, short y);
    virtual ~CGumpWorldMap();

    void UpdateSize();

    virtual bool CanBeDisplayed() override { return Called; }

    virtual void GenerateFrame(bool stop) override;

    virtual void PrepareContent() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_CHECKBOX_EVENT_H override;
    GUMP_COMBOBOX_SELECTION_EVENT_H override;
    GUMP_RESIZE_START_EVENT_H override;
    GUMP_RESIZE_EVENT_H override;
    GUMP_RESIZE_END_EVENT_H override;

    virtual void OnLeftMouseButtonDown() override;
    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
    virtual void OnMidMouseButtonScroll(bool up) override;
};
