// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Platform.h"
#include "BaseScreen.h"
#include "../Gumps/GumpScreenGame.h"

class CGameObject;

struct RENDER_VARIABLES_FOR_GAME_WINDOW
{
    int GameWindowPosX;
    int GameWindowPosY;

    int GameWindowWidth;
    int GameWindowHeight;

    int GameWindowScaledWidth;
    int GameWindowScaledHeight;

    int GameWindowScaledOffsetX;
    int GameWindowScaledOffsetY;

    int GameWindowCenterX;
    int GameWindowCenterY;

    int WindowDrawOffsetX;
    int WindowDrawOffsetY;

    int RealMinRangeX;
    int RealMaxRangeX;
    int RealMinRangeY;
    int RealMaxRangeY;

    int MinBlockX;
    int MinBlockY;
    int MaxBlockX;
    int MaxBlockY;

    int MinPixelsX;
    int MaxPixelsX;
    int MinPixelsY;
    int MaxPixelsY;

    int PlayerX;
    int PlayerY;
    int PlayerZ;
};

struct LIGHT_DATA
{
    uint8_t ID;
    uint16_t Color;
    int DrawX;
    int DrawY;
};

struct RENDER_OBJECT_DATA
{
    class CRenderWorldObject *Object;
    uint16_t GrayColor;
};

struct OBJECT_HITS_INFO
{
    int X;
    int Y;
    uint16_t Color;
    int Width;
    uint16_t HealthColor;
    CGLTextTexture *HitsTexture;
};

class CGameScreen : public CBaseScreen
{
public:
    bool UseLight = false;
    bool RenderListInitalized = false;
    bool UpdateDrawPos = false;
    int RenderIndex = 1;

private:
    CGameObject *m_ObjectHandlesList[MAX_OBJECT_HANDLES];
    LIGHT_DATA m_Light[MAX_LIGHT_SOURCES];
    vector<RENDER_OBJECT_DATA> m_RenderList;

    int m_ObjectHandlesCount = 0;
    int m_RenderListCount = 0;
    int m_LightCount = 0;
    int m_MaxDrawZ = 0;
    uint32_t m_ProcessAlphaTimer = 0;
    bool m_CanProcessAlpha = false;

    void PreRender();
    void DrawGameWindow(bool render);
    void DrawGameWindowLight();
    void DrawGameWindowText(bool render);
    void ApplyTransparentFoliageToUnion(uint16_t graphic, int x, int y, int z);
    void CheckFoliageUnion(uint16_t graphic, int x, int y, int z);

    void AddTileToRenderList(
        class CRenderWorldObject *obj,
        int worldX,
        int worldY,
        bool useObjectHandles,
        int maxZ = 150);

    void AddOffsetCharacterTileToRenderList(class CGameObject *obj, bool useObjectHandles);

    class CGumpScreenGame m_GameScreenGump;

    vector<OBJECT_HITS_INFO> m_HitsStack;

public:
    CGameScreen();
    virtual ~CGameScreen();

    static const uint8_t ID_SMOOTH_GS_LOGOUT = 1;

    void UpdateMaxDrawZ();
    virtual void ProcessSmoothAction(uint8_t action = 0xFF);
    void CalculateGameWindowBounds();
    void CalculateRenderList();
    virtual void Init();
    virtual void InitToolTip();
    void
    AddLight(class CRenderWorldObject *rwo, class CRenderWorldObject *lightObject, int x, int y);
    virtual void PrepareContent();
    virtual void Render() override;
    virtual void SelectObject() override;
    virtual void OnLeftMouseButtonDown();
    virtual void OnLeftMouseButtonUp();
    virtual bool OnLeftMouseButtonDoubleClick();
    virtual void OnRightMouseButtonDown();
    virtual void OnRightMouseButtonUp();
    virtual bool OnRightMouseButtonDoubleClick();
    virtual void OnMidMouseButtonDown() {}
    virtual void OnMidMouseButtonUp() {}
    virtual bool OnMidMouseButtonDoubleClick() { return false; }
    virtual void OnMidMouseButtonScroll(bool up);
    virtual void OnDragging();
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
    virtual void OnKeyUp(const KeyEvent &ev) override;
};

extern CGameScreen g_GameScreen;
extern RENDER_VARIABLES_FOR_GAME_WINDOW g_RenderBounds;
