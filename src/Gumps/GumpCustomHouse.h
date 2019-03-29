﻿// MIT License
// Copyright (C) September 2017 Hotride

#pragma once

#include <SDL_rect.h>
#include "Gump.h"
#include "../GameObjects/CustomHouseObjects.h"
#include "../Managers/CustomHousesManager.h"

class CMulti;
class CRenderWorldObject;

enum CUSTOM_HOUSE_GUMP_STATE
{
    CHGS_WALL = 0,
    CHGS_DOOR,
    CHGS_FLOOR,
    CHGS_STAIR,
    CHGS_ROOF,
    CHGS_MISC,
    CHGS_MENU
};

enum CUSTOM_HOUSE_FLOOR_VISION_STATE
{
    CHGVS_NORMAL = 0,
    CHGVS_TRANSPARENT_CONTENT,
    CHGVS_HIDE_CONTENT,
    CHGVS_TRANSPARENT_FLOOR,
    CHGVS_HIDE_FLOOR,
    CHGVS_TRANSLUCENT_FLOOR,
    CHGVS_HIDE_ALL
};

enum CUSTOM_HOUSE_BUILD_TYPE
{
    CHBT_NORMAL = 0,
    CHBT_ROOF,
    CHBT_FLOOR,
    CHBT_STAIR
};

enum CUSTOM_HOUSE_MULTI_OBJECT_FLAGS
{
    CHMOF_GENERIC_INTERNAL = 0x01,
    CHMOF_FLOOR = 0x02,
    CHMOF_STAIR = 0x04,
    CHMOF_ROOF = 0x08,
    CHMOF_FIXTURE = 0x10,
    CHMOF_TRANSPARENT = 0x20,
    CHMOF_IGNORE_IN_RENDER = 0x40,
    CHMOF_VALIDATED_PLACE = 0x80,
    CHMOF_INCORRECT_PLACE = 0x100
};

enum CUSTOM_HOUSE_VALIDATE_CHECK_FLAGS
{
    CHVCF_TOP = 0x01,
    CHVCF_BOTTOM = 0x02,
    CHVCF_N = 0x04,
    CHVCF_E = 0x08,
    CHVCF_S = 0x10,
    CHVCF_W = 0x20,
    CHVCF_DIRECT_SUPPORT = 0x40,
    CHVCF_CANGO_W = 0x80,
    CHVCF_CANGO_N = 0x100
};

class CGumpCustomHouse : public CGump
{
public:
    CUSTOM_HOUSE_GUMP_STATE State = CHGS_WALL;
    int Category = -1;
    int MaxPage = 1;
    uint16_t SelectedGraphic = 0;
    int CurrentFloor = 1;
    int FloorCount = 4;
    int RoofZ = 1;
    int MinHouseZ = -120;
    int Components = 0;
    int Fixtures = 0;
    int MaxComponents = 0;
    int MaxFixtures = 0;
    bool Erasing = false;
    bool SeekTile = false;
    bool ShowWindow = false;
    bool CombinedStair = false;
    CPoint2Di StartPos = CPoint2Di();
    CPoint2Di EndPos = CPoint2Di();

private:
    vector<CCustomHouseObjectWallCategory> m_Walls;
    vector<CCustomHouseObjectFloor> m_Floors;
    vector<CCustomHouseObjectDoor> m_Doors;
    vector<CCustomHouseObjectMiscCategory> m_Miscs;
    vector<CCustomHouseObjectStair> m_Stairs;
    vector<CCustomHouseObjectTeleport> m_Teleports;
    vector<CCustomHouseObjectRoofCategory> m_Roofs;
    vector<CCustomHouseObjectPlaceInfo> m_ObjectsInfo;

    int m_FloorVisionState[4];

    enum ID_GUMP_CUSTOM_HOUSE
    {
        ID_GCH_STATE_WALL = 1,
        ID_GCH_STATE_DOOR,
        ID_GCH_STATE_FLOOR,
        ID_GCH_STATE_STAIR,
        ID_GCH_STATE_ROOF,
        ID_GCH_STATE_MISC,
        ID_GCH_STATE_ERASE,
        ID_GCH_STATE_EYEDROPPER,
        ID_GCH_STATE_MENU,

        ID_GCH_VISIBILITY_STORY_1,
        ID_GCH_VISIBILITY_STORY_2,
        ID_GCH_VISIBILITY_STORY_3,
        ID_GCH_VISIBILITY_STORY_4,

        ID_GCH_GO_FLOOR_1,
        ID_GCH_GO_FLOOR_2,
        ID_GCH_GO_FLOOR_3,
        ID_GCH_GO_FLOOR_4,

        ID_GCH_LIST_LEFT,
        ID_GCH_LIST_RIGHT,

        ID_GCH_MENU_BACKUP,
        ID_GCH_MENU_RESTORE,
        ID_GCH_MENU_SYNCH,
        ID_GCH_MENU_CLEAR,
        ID_GCH_MENU_COMMIT,
        ID_GCH_MENU_REVERT,

        ID_GCH_GO_CATEGORY,
        ID_GCH_WALL_SHOW_WINDOW,
        ID_GCH_ROOF_Z_UP,
        ID_GCH_ROOF_Z_DOWN,

        ID_GCH_AREA_OBJECTS_INFO,
        ID_GCH_AREA_COST_INFO,
        ID_GCH_AREA_ROOF_Z_INFO,

        ID_GCH_ITEM_IN_LIST
    };

    CGUIDataBox *m_DataBox{ nullptr };
    CGUIDataBox *m_DataBoxGUI{ nullptr };

    CGUIGumppic *m_Gumppic{ nullptr };

    CGUIText *m_TextComponents{ nullptr };
    CGUIText *m_TextFixtures{ nullptr };
    CGUIText *m_TextCost{ nullptr };

    void DrawWallSection();
    void DrawDoorSection();
    void DrawFloorSection();
    void DrawStairSection();
    void DrawRoofSection();
    void DrawMiscSection();
    void DrawMenuSection();

    void UpdateMaxPage();

    pair<int, int> ExistsInList(CUSTOM_HOUSE_GUMP_STATE &state, uint16_t graphic);

    bool
    ValidatePlaceStructure(CGameItem *foundationItem, CMulti *multi, int minZ, int maxZ, int flags);

protected:
    virtual void CalculateGumpState() override;

public:
    CGumpCustomHouse(int serial, int x, int y);
    virtual ~CGumpCustomHouse();

    virtual void UpdateContent() override;

    virtual void InitToolTip() override;

    void GenerateFloorPlace();

    void SeekGraphic(uint16_t graphic);

    bool CanBuildHere(
        vector<CBuildObject> &list, CRenderWorldObject *place, CUSTOM_HOUSE_BUILD_TYPE &type);

    bool ValidateItemPlace(const SDL_Rect &rect, uint16_t graphic, int x, int y);

    bool ValidateItemPlace(
        CGameItem *foundationItem,
        CMultiObject *item,
        int minZ,
        int maxZ,
        vector<CPoint2Di> &validatedFloors);

    bool CanEraseHere(CRenderWorldObject *place, CUSTOM_HOUSE_BUILD_TYPE &type);

    void OnTargetWorld(CRenderWorldObject *place);

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;

    virtual void OnLeftMouseButtonUp() override;
};

extern CGumpCustomHouse *g_CustomHouseGump;
