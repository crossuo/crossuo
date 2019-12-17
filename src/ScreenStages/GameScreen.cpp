// MIT License
// Copyright (C) August 2016 Hotride

#define UO_RENDER_LIST_SORT 1
#define UO_CHECKERBOARD_SEQUENCE_RENDER_LIST 1

#include <algorithm>
#include <SDL_rect.h>
#include <climits> // INT_MAX
#include <common/str.h>
#include "GameScreen.h"
#include "GameBlockedScreen.h"
#include "ScreenshotBuilder.h"
#include <xuocore/uodata.h>
#include "../Config.h"
#include "../Macro.h"
#include "../Target.h"
#include "../Weather.h"
#include "../TargetGump.h"
#include "../CrossUO.h"
#include "../QuestArrow.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../ClickObject.h"
#include "../GameWindow.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/MapManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/GumpManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/PluginManager.h"
#include "../Managers/MacroManager.h"
#include "../Managers/ObjectPropertiesManager.h"
#include "../Managers/ScreenEffectManager.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../GameObjects/MapBlock.h"
#include "../GameObjects/GamePlayer.h"
#include "../GameObjects/LandObject.h"
#include "../GameObjects/CustomHouseMultiObject.h"
#include "../Gumps/GumpMap.h"
#include "../Gumps/GumpDrag.h"
#include "../Gumps/GumpPaperdoll.h"
#include "../Gumps/GumpCustomHouse.h"
#include "../Gumps/GumpConsoleType.h"
#include "../Gumps/GumpTargetSystem.h"
#include "../Gumps/GumpSkills.h"
#include "../Gumps/GumpContainer.h"
#include "../Gumps/GumpPopupMenu.h"
#include "../Network/Packets.h"
#include "../Walker/PathFinder.h"
#include "../TextEngine/GameConsole.h"
#include "../TextEngine/TextData.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGameScreen g_GameScreen;
RENDER_VARIABLES_FOR_GAME_WINDOW g_RenderBounds;

CGameScreen::CGameScreen()
    : CBaseScreen(m_GameScreenGump)
{
    m_RenderList.resize(1000);

    memset(&g_RenderBounds, 0, sizeof(g_RenderBounds));

    memset(&m_ObjectHandlesList[0], 0, sizeof(m_ObjectHandlesList));
    memset(&m_Light[0], 0, sizeof(m_Light));
}

CGameScreen::~CGameScreen()
{
}

void CGameScreen::Init()
{
    CBaseScreen::Init();

    g_GameWindow.SetWindowResizable(true);
    if (m_Maximized)
    {
        g_GameWindow.MaximizeWindow();
    }

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;
}

void CGameScreen::SetMaximized(bool maximized)
{
    m_Maximized = maximized;
}

void CGameScreen::ProcessSmoothAction(uint8_t action)
{
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_GS_LOGOUT)
    {
        g_LogoutAfterClick = true;
    }
}

void CGameScreen::InitToolTip()
{
    CRenderObject *obj = g_SelectedObject.Object;
    CGump *gump = g_SelectedObject.Gump;

    if (obj != nullptr && g_TooltipsEnabled)
    {
        uint32_t serial = 0;

        if (obj->IsGameObject())
        {
            serial = g_SelectedObject.Serial;
        }
        else if (gump != nullptr && obj->IsGUI())
        {
            if (gump->GumpType == GT_TRADE ||
                (gump->GumpType == GT_SHOP && ((CBaseGUI *)obj)->Type == GOT_SHOPITEM))
            {
                serial = g_SelectedObject.Serial;
            }
            else if (!gump->Minimized)
            {
                if (gump->GumpType == GT_CONTAINER &&
                    g_SelectedObject.Serial != CGumpContainer::ID_GC_LOCK_MOVING &&
                    g_SelectedObject.Serial != CGumpContainer::ID_GC_MINIMIZE)
                {
                    serial = g_SelectedObject.Serial;
                }
                else if (
                    gump->GumpType == GT_PAPERDOLL &&
                    g_SelectedObject.Serial >= CGumpPaperdoll::ID_GP_ITEMS && g_World != nullptr)
                {
                    CGameCharacter *character = g_World->FindWorldCharacter(gump->Serial);

                    if (character != nullptr)
                    {
                        CGameObject *item = character->FindLayer(
                            g_SelectedObject.Serial - CGumpPaperdoll::ID_GP_ITEMS);

                        if (item != nullptr)
                        {
                            serial = item->Serial;
                        }
                    }
                }
            }
        }

        if (serial != 0u)
        {
            g_ObjectPropertiesManager.Display(serial);
            return;
        }
    }

    if (g_ConfigManager.UseToolTips && gump != nullptr)
    {
        if (gump == &m_GameScreenGump)
        {
            m_GameScreenGump.InitToolTip();
        }
        else
        {
            g_GumpManager.InitToolTip();
        }
    }
}

void CGameScreen::UpdateMaxDrawZ()
{
    int playerX = g_Player->GetX();
    int playerY = g_Player->GetY();
    int playerZ = g_Player->GetZ();

    if (playerX == g_Player->OldX && playerY == g_Player->OldY && playerZ == g_Player->OldZ)
    {
        return;
    }

    g_Player->OldX = g_Player->GetX();
    g_Player->OldY = g_Player->GetY();
    g_Player->OldZ = g_Player->GetZ();

    g_NoDrawRoof = g_ConfigManager.GetNoDrawRoofs();
    char maxGroundZ = 127;
    g_MaxGroundZ = 127;
    m_MaxDrawZ = 127;

    int bx = playerX / 8;
    int by = playerY / 8;

    int blockIndex = (bx * g_MapBlockSize[g_CurrentMap].Height) + by;
    CMapBlock *mb = g_MapManager.GetBlock(blockIndex);

    if (mb != nullptr)
    {
        int x = playerX % 8;
        int y = playerY % 8;

        int pz14 = playerZ + 14;
        int pz16 = playerZ + 16;

        for (CRenderWorldObject *ro = mb->GetRender(x, y); ro != nullptr; ro = ro->m_NextXY)
        {
            char tileZ = ro->GetZ();

            if (!ro->IsGameObject())
            {
                if (ro->IsLandObject())
                {
                    if (pz16 <= tileZ)
                    {
                        maxGroundZ = pz16;
                        g_MaxGroundZ = pz16;
                        m_MaxDrawZ = g_MaxGroundZ;

                        break;
                    }

                    continue;
                }
                if (!ro->IsStaticObject() && !ro->IsMultiObject())
                {
                    continue;
                }
            }
            else if (((CGameObject *)ro)->NPC)
            {
                continue;
            }

            if (tileZ > pz14 && m_MaxDrawZ > tileZ &&
                ((((CRenderStaticObject *)ro)->GetStaticData()->Flags & 0x20004) == 0u) &&
                (!ro->IsRoof() || ro->IsSurface()))
            {
                m_MaxDrawZ = tileZ;

                g_NoDrawRoof = true;
            }
        }

        int tempZ = m_MaxDrawZ;
        g_MaxGroundZ = m_MaxDrawZ;

        playerX++;
        playerY++;

        bx = playerX / 8;
        by = playerY / 8;

        blockIndex = (bx * g_MapBlockSize[g_CurrentMap].Height) + by;
        CMapBlock *mb11 = g_MapManager.GetBlock(blockIndex);

        if (mb11 != nullptr)
        {
            x = playerX % 8;
            y = playerY % 8;

            for (CRenderWorldObject *ro = mb11->GetRender(x, y); ro != nullptr; ro = ro->m_NextXY)
            {
                if (!ro->IsGameObject())
                {
                    if (!ro->IsStaticObject() && !ro->IsMultiObject())
                    {
                        continue;
                    }
                }
                else if (((CGameObject *)ro)->NPC)
                {
                    continue;
                }

                char tileZ = ro->GetZ();

                if (tileZ > pz14 && m_MaxDrawZ > tileZ &&
                    ((((CRenderStaticObject *)ro)->GetStaticData()->Flags & 0x204) == 0u) &&
                    ro->IsRoof())
                {
                    m_MaxDrawZ = tileZ;
                    g_MapManager.ClearBlockAccess();
                    g_MaxGroundZ = g_MapManager.CalculateNearZ(tileZ, playerX, playerY, tileZ);

                    g_NoDrawRoof = true;
                }
            }

            tempZ = g_MaxGroundZ;
        }

        m_MaxDrawZ = g_MaxGroundZ;

        if (tempZ < pz16)
        {
            m_MaxDrawZ = pz16;
            g_MaxGroundZ = pz16;
        }

        g_MaxGroundZ = maxGroundZ;
    }
}

void CGameScreen::ApplyTransparentFoliageToUnion(uint16_t graphic, int x, int y, int z)
{
    int bx = x / 8;
    int by = y / 8;

    int blockIndex = (bx * g_MapBlockSize[g_CurrentMap].Height) + by;
    CMapBlock *mb = g_MapManager.GetBlock(blockIndex);

    if (mb != nullptr)
    {
        int tx = x % 8;
        int ty = y % 8;

        for (CRenderWorldObject *obj = mb->GetRender(tx, ty); obj != nullptr; obj = obj->m_NextXY)
        {
            uint16_t testGraphic = obj->Graphic;

            if (obj->IsGameObject() && !((CGameObject *)obj)->NPC && ((CGameItem *)obj)->MultiBody)
            {
                testGraphic = ((CGameItem *)obj)->MultiTileGraphic;
            }

            if (testGraphic == graphic && obj->GetZ() == z)
            {
                obj->StaticGroupObjectPtr()->FoliageTransparentIndex = g_FoliageIndex;
            }
        }
    }
}

void CGameScreen::CheckFoliageUnion(uint16_t graphic, int x, int y, int z)
{
    for (int i = 0; i < TREE_COUNT; i++)
    {
        const TREE_UNIONS &info = TREE_INFO[i];

        if (info.GraphicStart <= graphic && graphic <= info.GraphicEnd)
        {
            while (graphic > info.GraphicStart)
            {
                graphic--;
                x--;
                y++;
            }

            for (graphic = info.GraphicStart; graphic <= info.GraphicEnd; graphic++, x++, y--)
            {
                ApplyTransparentFoliageToUnion(graphic, x, y, z);
            }

            break;
        }
    }
}

void CGameScreen::CalculateRenderList()
{
    m_RenderListCount = 0;

    if (g_Player == nullptr)
    {
        return;
    }

    if (g_Target.IsTargeting() && (g_Target.MultiGraphic != 0u) &&
        g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsWorldObject())
    {
        int grZ = 0;
        int stZ = 0;
        CRenderObject *sel = g_SelectedObject.Object;
        g_MapManager.GetMapZ(sel->GetX(), sel->GetY(), grZ, stZ);

        if (((CRenderWorldObject *)sel)->IsStaticObject() && ((CRenderWorldObject *)sel)->IsWet())
        {
            grZ = ((CRenderWorldObject *)sel)->GetZ();
        }

        g_Target.LoadMulti(sel->GetX() - g_Target.MultiX, sel->GetY() - g_Target.MultiY, grZ);
    }

    m_CanProcessAlpha = (m_ProcessAlphaTimer < g_Ticks);

    if (m_CanProcessAlpha)
    {
        m_ProcessAlphaTimer = g_Ticks + 50;
    }

    g_FoliageIndex++;

    if (g_FoliageIndex >= 100)
    {
        g_FoliageIndex = 1;
    }

    switch (g_ConfigManager.GetDrawAuraState())
    {
        case DAS_IN_WARMODE:
        {
            g_DrawAura =
                g_Player->Warmode && (!g_ConfigManager.DrawAuraWithCtrlPressed || g_CtrlPressed);
            break;
        }
        case DAS_ALWAYS:
        {
            g_DrawAura = (!g_ConfigManager.DrawAuraWithCtrlPressed || g_CtrlPressed);
            break;
        }
        default:
        {
            g_DrawAura = false;
            break;
        }
    }

    m_ObjectHandlesCount = 0;
    bool useObjectHandles =
        (!g_GrayedPixels && g_ConfigManager.ObjectHandles && g_ShiftPressed && g_CtrlPressed);

    QFOR(go, g_World->m_Items, CGameObject *)
    {
        if (go->NPC || go->IsCorpse())
        {
            go->m_FrameInfo = g_AnimationManager.CollectFrameInformation(go);

            if (go->IsPlayer())
            {
                int playerZOffset = (g_Player->GetZ() * 4) - g_Player->OffsetZ;

                DRAW_FRAME_INFORMATION &dfInfo = go->m_FrameInfo;

                g_PlayerRect.X =
                    g_RenderBounds.GameWindowCenterX - dfInfo.OffsetX + g_Player->OffsetX;
                g_PlayerRect.Y = g_RenderBounds.GameWindowCenterY + g_Player->OffsetY -
                                 playerZOffset - dfInfo.OffsetY;
                g_PlayerRect.Width = dfInfo.Width;
                g_PlayerRect.Height = dfInfo.Height;
            }
        }
    }

#if UO_CHECKERBOARD_SEQUENCE_RENDER_LIST == 1
    int minX = g_RenderBounds.RealMinRangeX;
    int minY = g_RenderBounds.RealMinRangeY;

    int maxX = g_RenderBounds.RealMaxRangeX;
    int maxY = g_RenderBounds.RealMaxRangeY;

    int mapBlockHeight = g_MapBlockSize[g_MapManager.GetActualMap()].Height;
    uint32_t maxBlockIndex = g_MapManager.MaxBlockIndex;

    for (int i = 0; i < 2; i++)
    {
        int minValue = minY;
        int maxValue = maxY;

        if (i != 0)
        {
            minValue = minX;
            maxValue = maxX;
        }

        for (int lead = minValue; lead < maxValue; lead++)
        {
            int x = minX;
            int y = (int)lead;

            if (i != 0)
            {
                x = (int)lead;
                y = maxY;
            }

            while (true)
            {
                if (x < minX || x > maxX || y < minY || y > maxY)
                {
                    break;
                }

                int blockX = x / 8;
                int blockY = y / 8;

                uint32_t blockIndex = (blockX * mapBlockHeight) + blockY;

                if (blockIndex < maxBlockIndex)
                {
                    CMapBlock *block = g_MapManager.GetBlock(blockIndex);

                    if (block == nullptr)
                    {
                        block = g_MapManager.AddBlock(blockIndex);
                        block->X = blockX;
                        block->Y = blockY;
                        g_MapManager.LoadBlock(block);
                    }

                    AddTileToRenderList(block->GetRender(x % 8, y % 8), x, y, useObjectHandles);
                }
                else
                {
                    DEBUG(Client, "expected: %i %i\n", blockIndex, g_MapManager.MaxBlockIndex);
                }

                x++;
                y--;
            }
        }
    }
#else
    for (int bx = g_RenderBounds.MinBlockX; bx <= g_RenderBounds.MaxBlockX; bx++)
    {
        for (int by = g_RenderBounds.MinBlockY; by <= g_RenderBounds.MaxBlockY; by++)
        {
            int blockIndex = (bx * g_MapBlockSize[g_CurrentMap].Height) + by;

            CMapBlock *mb = g_MapManager.GetBlock(blockIndex);

            if (mb == nullptr)
            {
                mb = g_MapManager.AddBlock(blockIndex);
                mb->X = bx;
                mb->Y = by;
                g_MapManager.LoadBlock(mb);
            }

            for (int x = 0; x < 8; x++)
            {
                int currentX = bx * 8 + x;

                if (currentX < g_RenderBounds.RealMinRangeX ||
                    currentX > g_RenderBounds.RealMaxRangeX)
                    continue;

                for (int y = 0; y < 8; y++)
                {
                    int currentY = by * 8 + y;

                    if (currentY < g_RenderBounds.RealMinRangeY ||
                        currentY > g_RenderBounds.RealMaxRangeY)
                        continue;

                    AddTileToRenderList(mb->GetRender(x, y), currentX, currentY, useObjectHandles);
                }
            }
        }
    }
#endif

    if (m_CanProcessAlpha)
    {
        for (int i = 0; i < m_RenderListCount; i++)
        {
            CRenderWorldObject *obj = m_RenderList[i].Object;

            if (obj != nullptr && obj->IsStaticGroupObject())
            {
                CRenderStaticObject *rst = obj->StaticGroupObjectPtr();

                if (rst->IsFoliage())
                {
                    if (rst->FoliageTransparentIndex == g_FoliageIndex)
                    {
                        rst->ProcessAlpha(FOLIAGE_ALPHA);
                    }
                    else
                    {
                        rst->ProcessAlpha(0xFF);
                    }
                }
            }
        }
    }

    if (m_ObjectHandlesCount > MAX_OBJECT_HANDLES)
    {
        m_ObjectHandlesCount = MAX_OBJECT_HANDLES;
    }

    QFOR(go, g_World->m_Items, CGameObject *)
    {
        if (go->UseInRender != RenderIndex && (go->NPC || go->IsCorpse()) &&
            go->m_TextControl->m_Items != nullptr)
        {
            go->UpdateDrawCoordinates();
            go->UseInRender = RenderIndex;
        }
    }

#if UO_RENDER_LIST_SORT == 1
    RenderIndex++;

    if (RenderIndex >= 100)
    {
        RenderIndex = 1;
    }
#endif

    UpdateDrawPos = false;
}

void CGameScreen::AddTileToRenderList(
    CRenderWorldObject *obj, int worldX, int worldY, bool useObjectHandles, int maxZ)
{
    uint16_t grayColor = 0;

    if (g_ConfigManager.GrayOutOfRangeObjects)
    {
        if (GetDistance(g_Player, CPoint2Di(worldX, worldY)) > g_ConfigManager.UpdateRange)
        {
            grayColor = 0x038E;
        }
    }

    if (g_CustomHouseGump != nullptr)
    {
        SDL_Rect rect = { g_CustomHouseGump->StartPos.X,
                          g_CustomHouseGump->StartPos.Y,
                          g_CustomHouseGump->EndPos.X,
                          g_CustomHouseGump->EndPos.Y + 1 };
        SDL_Point pos = { worldX, worldY };

        if (SDL_PointInRect(&pos, &rect) == 0u)
        {
            grayColor = 0x038E;
        }
    }

    for (; obj != nullptr; obj = obj->m_NextXY)
    {
#if UO_RENDER_LIST_SORT == 1
        if (obj->CurrentRenderIndex == RenderIndex || obj->NoDrawTile)
        {
            continue;
        }
#endif

        if ((UpdateDrawPos && obj->CurrentRenderIndex != RenderIndex) || obj->Changed)
        {
            obj->UpdateDrawCoordinates();
        }

        obj->UseInRender = 0xFF;
        int drawX = obj->DrawX;
        int drawY = obj->DrawY;

        if (drawX < g_RenderBounds.MinPixelsX || drawX > g_RenderBounds.MaxPixelsX)
        {
            break;
        }

        bool aphaChanged = false;

        int z = obj->GetZ();
#if UO_RENDER_LIST_SORT == 1
        int maxObjectZ = obj->PriorityZ;

        CRenderStaticObject *rso = obj->StaticGroupObjectPtr();

        if (rso != nullptr)
        {
            if (rso->IsGameObject())
            {
                if (((CGameObject *)rso)->NPC)
                {
                    maxObjectZ += DEFAULT_CHARACTER_HEIGHT;
                }
                else
                {
                    maxObjectZ += rso->GetStaticHeight();
                }
            }
            else
            {
                if (!rso->CheckDrawFoliage())
                {
                    continue;
                }
                if (g_NoDrawRoof && rso->IsRoof())
                {
                    if (m_CanProcessAlpha)
                    {
                        aphaChanged = obj->ProcessAlpha(0);
                    }
                    else
                    {
                        aphaChanged = (obj->m_DrawTextureColor[3] != 0);
                    }

                    if (!aphaChanged)
                    {
                        continue;
                    }
                }
                else if (!rso->CheckDrawVegetation())
                {
                    continue;
                }

                maxObjectZ += rso->GetStaticHeight();
            }
        }

        if (maxObjectZ > maxZ)
        {
            break;
        }

        obj->CurrentRenderIndex = RenderIndex;
#endif

        if (obj->IsInternal())
        {
            continue;
        }
        if (!obj->IsLandObject() && z >= m_MaxDrawZ)
        {
            if (!aphaChanged)
            {
                if (m_CanProcessAlpha)
                {
                    aphaChanged = obj->ProcessAlpha(0);
                }
                else
                {
                    aphaChanged = (obj->m_DrawTextureColor[3] != 0);
                }

                if (!aphaChanged)
                {
                    continue;
                }
            }
        }

        int testMinZ = drawY + (z * 4);
        int testMaxZ = drawY;

        CLandObject *land = obj->LandObjectPtr();

        if (land != nullptr && land->IsStretched)
        {
            testMinZ -= (land->MinZ * 4);
        }
        else
        {
            testMinZ = testMaxZ;
        }

        if (testMinZ < g_RenderBounds.MinPixelsY || testMaxZ > g_RenderBounds.MaxPixelsY)
        {
            continue;
        }

        if (obj->IsGameObject())
        {
            CGameObject *go = (CGameObject *)obj;

            if (go->NPC)
            {
                CGameCharacter *character = go->GameCharacterPtr();

                CTextContainer &textContainer = character->m_DamageTextControl;

                if (!textContainer.Empty())
                {
                    const auto dims = g_AnimationManager.GetAnimationDimensions(go);
                    int textDrawX = drawX + character->OffsetX;
                    int textDrawY = drawY + character->OffsetY -
                                    (character->OffsetZ + dims.Height + dims.CenterY);

                    for (CTextData *text = (CTextData *)textContainer.m_Items; text != nullptr;)
                    {
                        CTextData *next = (CTextData *)text->m_Next;

                        if (text->m_TextSprite.Empty())
                        {
                            text = next;
                            continue;
                        }

                        if (text->Timer < g_Ticks)
                        {
                            if (text->Transparent)
                            {
                                textContainer.Delete(text);
                                text = next;
                                continue;
                            }

                            text->Timer = g_Ticks + DAMAGE_TEXT_TRANSPARENT_DELAY;
                            text->Transparent = true;
                            text->Color = 0x00FF;
                        }

                        text->RealDrawX = textDrawX - text->GetX();
                        text->RealDrawY = textDrawY + text->GetY();

                        if (text->Transparent)
                        {
                            if ((uint8_t)text->Color >= DAMAGE_TEXT_ALPHA_STEP)
                            {
                                text->Color -= DAMAGE_TEXT_ALPHA_STEP;
                            }
                            else
                            {
                                text->Color = 0;
                            }
                        }

                        if (text->MoveTimer < g_Ticks)
                        {
                            text->SetY(text->GetY() - DAMAGE_TEXT_STEP);
                            text->MoveTimer = g_Ticks + DAMAGE_TEXT_MOVE_DELAY;
                        }

                        text = next;
                    }
                }
            }

            if ((go->NPC || (!go->Locked() && !((CGameItem *)go)->MultiBody)) && useObjectHandles &&
                !go->ClosedObjectHandle) // && m_ObjectHandlesCount < MAX_OBJECT_HANDLES)
            {
                int index = m_ObjectHandlesCount % MAX_OBJECT_HANDLES;

                m_ObjectHandlesList[index] = go;

                m_ObjectHandlesCount++;
            }

            else if (go->IsHuman() && g_ConfigManager.AlwaysDisplayHumanoidsName)
            {
                int index = m_ObjectHandlesCount % MAX_OBJECT_HANDLES;

                m_ObjectHandlesList[index] = go;

                m_ObjectHandlesCount++;
            }

#if UO_RENDER_LIST_SORT == 1
            if (go->NPC || go->IsCorpse())
            {
                AddOffsetCharacterTileToRenderList(go, useObjectHandles);
            }
#endif
        }
        else if (
            obj->IsFoliage() &&
            obj->StaticGroupObjectPtr()->FoliageTransparentIndex != g_FoliageIndex)
        {
            char index = 0;

            bool foliageCanBeChecked =
                (g_RenderBounds.PlayerX <= worldX && g_RenderBounds.PlayerY <= worldY);

            if (!foliageCanBeChecked)
            {
                foliageCanBeChecked =
                    (g_RenderBounds.PlayerY <= worldY && g_RenderBounds.PlayerX <= worldX + 1);

                if (!foliageCanBeChecked)
                {
                    foliageCanBeChecked =
                        (g_RenderBounds.PlayerX <= worldX && g_RenderBounds.PlayerY <= worldY + 1);
                }
            }

            if (foliageCanBeChecked)
            {
                auto spr = g_Game.ExecuteStaticArt(obj->Graphic);
                if (spr != nullptr)
                {
                    CImageBounds fib(
                        drawX - spr->Width / 2 + spr->ImageOffsetX,
                        drawY - spr->Height + spr->ImageOffsetY,
                        spr->ImageWidth,
                        spr->ImageHeight);
                    if (fib.InRect(g_PlayerRect))
                    {
                        index = g_FoliageIndex;
                        CheckFoliageUnion(obj->Graphic, obj->GetX(), obj->GetY(), z);
                    }
                }
            }
            obj->StaticGroupObjectPtr()->FoliageTransparentIndex = index;
        }

        if (m_CanProcessAlpha && !aphaChanged)
        {
            if (obj->IsTranslucent())
            {
                obj->ProcessAlpha(TRANSLUCENT_ALPHA);
            }
            else if (!obj->IsFoliage() && obj->m_DrawTextureColor[3] != 0xFF)
            {
                obj->ProcessAlpha(0xFF);
            }
        }

        if (m_RenderListCount >= (int)m_RenderList.size())
        {
            size_t newSize = m_RenderList.size() + 1000;

            m_RenderList.resize(newSize);

            if (m_RenderList.size() != newSize)
            {
                Warning(Client, "out of memory for render list (need size: %zi)", newSize);
                m_RenderList.resize(newSize - 1000);
                if (m_RenderList.size() != newSize - 1000)
                {
                    Error(
                        Client,
                        "out of memory for render list - SECOND TRY (need size: %zi)",
                        newSize - 1000);
                    m_RenderListCount = 0;
                    return;
                }
            }
        }

        TRACE(
            Client,
            "Item[0x%04X]: x=%i y=%i (dx=%i, dy=%i)\n",
            obj->Graphic,
            drawX,
            drawY,
            obj->DrawX,
            obj->DrawY);

        m_RenderList[m_RenderListCount].Object = obj;
        m_RenderList[m_RenderListCount].GrayColor = grayColor;
        obj->UseInRender = RenderIndex;

        if ((grayColor == 0u) && g_CustomHouseGump != nullptr && g_Target.IsTargeting() &&
            obj == g_SelectedObject.Object)
        {
            int zOffset = 0;

            if (g_CustomHouseGump->CurrentFloor == 1)
            {
                zOffset = -7;
            }

            if (obj->GetZ() >= g_Player->GetZ() + zOffset && obj->GetZ() < g_Player->GetZ() + 20)
            {
                if (g_CustomHouseGump->Erasing)
                {
                    CUSTOM_HOUSE_BUILD_TYPE type;

                    if (g_CustomHouseGump->CanEraseHere(obj, type))
                    {
                        m_RenderList[m_RenderListCount].GrayColor = 0x0021;
                    }
                }
                else
                {
                    m_RenderList[m_RenderListCount].GrayColor = 0x0035;
                }
            }
        }

        m_RenderListCount++;
    }
}

void CGameScreen::AddOffsetCharacterTileToRenderList(CGameObject *obj, bool useObjectHandles)
{
    int characterX = obj->GetX();
    int characterY = obj->GetY();

    CGameCharacter *character = obj->GameCharacterPtr();

    // Make all characters draw in right order.
    // TODO: Some cases are still broken. It moves tiles to different layers etc. But it should not be directly player related.
    bool bAdjust =
        character &&
        ((character->Direction & 7) == 6 || (character->Direction & 7) == 2 ||
         (!character->m_Steps.empty() && ((character->m_Steps.back().Direction & 7) == 6 ||
                                          (character->m_Steps.back().Direction & 7) == 2)));

    std::vector<std::pair<int, int>> coordinates;

    coordinates.push_back(std::pair<int, int>(characterX + 1, characterY - 1));
    coordinates.push_back(std::pair<int, int>(characterX + 1, characterY - 2));
    coordinates.push_back(std::pair<int, int>(characterX + 2, characterY - 2));
    coordinates.push_back(std::pair<int, int>(characterX - 1, characterY + 2));
    coordinates.push_back(std::pair<int, int>(characterX, characterY + 1));
    coordinates.push_back(std::pair<int, int>(characterX + 1, characterY));
    coordinates.push_back(std::pair<int, int>(characterX + 2, characterY - 1));
    coordinates.push_back(std::pair<int, int>(characterX + 1, characterY + 1));

    const auto size = (int)coordinates.size();

    int maxZ = obj->PriorityZ;

    int mapBlockHeight = g_MapBlockSize[g_CurrentMap].Height;
    uint32_t maxBlockIndex = g_MapManager.MaxBlockIndex;

    for (int i = 0; i < size; i++)
    {
        int x = coordinates[i].first;

        if (x < g_RenderBounds.RealMinRangeX || x > g_RenderBounds.RealMaxRangeX)
        {
            continue;
        }

        int y = coordinates[i].second;

        if (y < g_RenderBounds.RealMinRangeY || y > g_RenderBounds.RealMaxRangeY)
        {
            continue;
        }

        int blockX = x / 8;
        int blockY = y / 8;

        uint32_t blockIndex = (blockX * mapBlockHeight) + blockY;

        if (blockIndex < maxBlockIndex)
        {
            CMapBlock *block = g_MapManager.GetBlock(blockIndex);

            if (block == nullptr)
            {
                block = g_MapManager.AddBlock(blockIndex);
                block->X = blockX;
                block->Y = blockY;
                g_MapManager.LoadBlock(block);
            }

            int currentMaxZ = maxZ;

            // Draw whole character in front of walls.
            if (i <= 1 && bAdjust)
                currentMaxZ += 30;

            AddTileToRenderList(
                block->GetRender(x % 8, y % 8), x, y, useObjectHandles, currentMaxZ);
        }
    }
}

void CGameScreen::CalculateGameWindowBounds()
{
    g_GrayedPixels = g_Player->Dead();

    if (g_GrayedPixels && g_Season != ST_DESOLATION)
    {
        g_Game.ChangeSeason(ST_DESOLATION, DEATH_MUSIC_INDEX);
    }

    g_RenderBounds.PlayerX = g_Player->GetX();
    g_RenderBounds.PlayerY = g_Player->GetY();
    g_RenderBounds.PlayerZ = g_Player->GetZ();

    int oldDrawOffsetX = g_RenderBounds.WindowDrawOffsetX;

    g_RenderBounds.GameWindowPosX = g_ConfigManager.GameWindowX;
    g_RenderBounds.GameWindowPosY = g_ConfigManager.GameWindowY;

    g_RenderBounds.GameWindowWidth = g_ConfigManager.GameWindowWidth;
    g_RenderBounds.GameWindowHeight = g_ConfigManager.GameWindowHeight;

    m_GameScreenGump.UpdateContent();

    //int playerZOffset = (g_Player->GetZ() * 4) - g_Player->OffsetZ;

    g_RenderBounds.GameWindowCenterX =
        g_RenderBounds.GameWindowPosX + (g_RenderBounds.GameWindowWidth / 2);
    g_RenderBounds.GameWindowCenterY =
        (g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight / 2) +
        (g_Player->GetZ() * 4);

    /*int earthquakeMagnitude = RandomInt(11);

	if (earthquakeMagnitude)
	{
		g_RenderBounds.GameWindowCenterX += RandomInt(earthquakeMagnitude * 3);
		g_RenderBounds.GameWindowCenterY += RandomInt(earthquakeMagnitude * 3);
	}*/

    g_RenderBounds.GameWindowCenterX -= g_Player->OffsetX;
    g_RenderBounds.GameWindowCenterY -= (g_Player->OffsetY - g_Player->OffsetZ);

    g_RenderBounds.WindowDrawOffsetX =
        ((g_Player->GetX() - g_Player->GetY()) * 22) - g_RenderBounds.GameWindowCenterX;
    g_RenderBounds.WindowDrawOffsetY =
        ((g_Player->GetX() + g_Player->GetY()) * 22) - g_RenderBounds.GameWindowCenterY;

    if (g_ConfigManager.GetUseScaling())
    {
        const auto left = (float)g_RenderBounds.GameWindowPosX;
        const auto right = (float)(g_RenderBounds.GameWindowWidth + left);
        const auto top = (float)g_RenderBounds.GameWindowPosY;
        const auto bottom = (float)(g_RenderBounds.GameWindowHeight + top);

        const auto newRight = right * g_GlobalScale;
        const auto newBottom = bottom * g_GlobalScale;

        g_RenderBounds.GameWindowScaledOffsetX = (int)((left * g_GlobalScale) - (newRight - right));
        g_RenderBounds.GameWindowScaledOffsetY =
            (int)((top * g_GlobalScale) - (newBottom - bottom));

        g_RenderBounds.GameWindowScaledWidth =
            (int)(newRight - g_RenderBounds.GameWindowScaledOffsetX);
        g_RenderBounds.GameWindowScaledHeight =
            (int)(newBottom - g_RenderBounds.GameWindowScaledOffsetY);
    }
    else
    {
        g_GlobalScale = 1.0;

        g_RenderBounds.GameWindowScaledOffsetX = 0;
        g_RenderBounds.GameWindowScaledOffsetY = 0;

        g_RenderBounds.GameWindowScaledWidth = 0;
        g_RenderBounds.GameWindowScaledHeight = 0;
    }

    int rangeX = (int)(((g_RenderBounds.GameWindowWidth / 44) + 1) * g_GlobalScale);
    int rangeY = (int)(((g_RenderBounds.GameWindowHeight / 44) + 1) * g_GlobalScale);

    if (rangeX < rangeY)
    {
        rangeX = rangeY;
    }
    else
    {
        rangeY = rangeX;
    }

    g_RenderBounds.RealMinRangeX = g_Player->GetX() - rangeX;

    if (g_RenderBounds.RealMinRangeX < 0)
    {
        g_RenderBounds.RealMinRangeX = 0;
    }

    g_RenderBounds.RealMaxRangeX = g_Player->GetX() + rangeX;

    if (g_RenderBounds.RealMaxRangeX >= g_MapSize[g_CurrentMap].Width)
    {
        g_RenderBounds.RealMaxRangeX = g_MapSize[g_CurrentMap].Width;
    }

    g_RenderBounds.RealMinRangeY = g_Player->GetY() - rangeY;

    if (g_RenderBounds.RealMinRangeY < 0)
    {
        g_RenderBounds.RealMinRangeY = 0;
    }

    g_RenderBounds.RealMaxRangeY = g_Player->GetY() + rangeY;

    if (g_RenderBounds.RealMaxRangeY >= g_MapSize[g_CurrentMap].Height)
    {
        g_RenderBounds.RealMaxRangeY = g_MapSize[g_CurrentMap].Height;
    }

    g_RenderBounds.MinBlockX = (g_RenderBounds.RealMinRangeX / 8) - 1;
    g_RenderBounds.MinBlockY = (g_RenderBounds.RealMinRangeY / 8) - 1;
    g_RenderBounds.MaxBlockX = (g_RenderBounds.RealMaxRangeX / 8) + 1;
    g_RenderBounds.MaxBlockY = (g_RenderBounds.RealMaxRangeY / 8) + 1;

    if (g_RenderBounds.MinBlockX < 0)
    {
        g_RenderBounds.MinBlockX = 0;
    }

    if (g_RenderBounds.MinBlockY < 0)
    {
        g_RenderBounds.MinBlockY = 0;
    }

    if (g_RenderBounds.MaxBlockX >= g_MapBlockSize[g_CurrentMap].Width)
    {
        g_RenderBounds.MaxBlockX = g_MapBlockSize[g_CurrentMap].Width - 1;
    }

    if (g_RenderBounds.MaxBlockY >= g_MapBlockSize[g_CurrentMap].Height)
    {
        g_RenderBounds.MaxBlockY = g_MapBlockSize[g_CurrentMap].Height - 1;
    }

    int drawOffset = (int)(g_GlobalScale * 40.0);

    const auto maxX = g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth + drawOffset;
    const auto maxY = g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight + drawOffset;
    const auto newMaxX = maxX * g_GlobalScale;
    const auto newMaxY = maxY * g_GlobalScale;

    g_RenderBounds.MinPixelsX =
        (int)(((g_RenderBounds.GameWindowPosX - drawOffset) * g_GlobalScale) - (newMaxX - maxX));
    g_RenderBounds.MaxPixelsX = (int)newMaxX;

    g_RenderBounds.MinPixelsY =
        (int)(((g_RenderBounds.GameWindowPosY - drawOffset) * g_GlobalScale) - (newMaxY - maxY)); // -playerZOffset;
    g_RenderBounds.MaxPixelsY = (int)newMaxY; // + playerZOffset;

    if (UpdateDrawPos || oldDrawOffsetX != g_RenderBounds.WindowDrawOffsetX ||
        oldDrawOffsetX != g_RenderBounds.WindowDrawOffsetY)
    {
        UpdateDrawPos = true;
        RenderListInitalized = false;
    }

    UpdateMaxDrawZ();

    UseLight = (g_PersonalLightLevel < g_LightLevel);

    if (UseLight)
    {
        int testWidth = g_RenderBounds.GameWindowWidth;
        int testHeight = g_RenderBounds.GameWindowHeight;

        if (g_ConfigManager.GetUseScaling())
        {
            testWidth = g_RenderBounds.GameWindowScaledWidth;
            testHeight = g_RenderBounds.GameWindowScaledHeight;
        }

        if (!g_LightBuffer.Ready(testWidth, testHeight))
        {
            g_LightBuffer.Init(testWidth, testHeight);
        }
    }
}

void CGameScreen::AddLight(CRenderWorldObject *rwo, CRenderWorldObject *lightObject, int x, int y)
{
    if (m_LightCount < MAX_LIGHT_SOURCES)
    {
        bool canBeAdded = true;

        int testX = rwo->GetX() + 1;
        int testY = rwo->GetY() + 1;

        int bx = testX / 8;
        int by = testY / 8;

        int blockIndex = (bx * g_MapBlockSize[g_CurrentMap].Height) + by;
        CMapBlock *mb = g_MapManager.GetBlock(blockIndex);

        if (mb != nullptr)
        {
            bx = testX % 8;
            by = testY % 8;

            char z5 = rwo->GetZ() + 5;

            for (CRenderWorldObject *obj = mb->GetRender(bx, by); obj != nullptr;
                 obj = obj->m_NextXY)
            {
                if (!obj->IsStaticGroupObject() ||
                    (obj->IsGameObject() && ((CGameObject *)obj)->NPC) || obj->NoDrawTile ||
                    obj->IsTransparent())
                {
                    continue;
                }

                if (obj->GetZ() < m_MaxDrawZ && obj->GetZ() >= z5)
                {
                    canBeAdded = false;
                    break;
                }
            }
        }

        if (canBeAdded)
        {
            LIGHT_DATA &light = m_Light[m_LightCount];

            uint16_t graphic = lightObject->Graphic;

            if ((graphic >= 0x3E02 && graphic <= 0x3E0B) ||
                (graphic >= 0x3914 && graphic <= 0x3929))
            {
                light.ID = 2;
            }
            else if (rwo == lightObject && rwo->IsGameObject())
            {
                light.ID = ((CGameItem *)lightObject)->LightID;
            }
            else
            {
                light.ID = (uint8_t)lightObject->GetLightID();
            }

            if (light.ID >= MAX_LIGHTS_DATA_INDEX_COUNT)
            {
                return;
            }

            if (g_ConfigManager.ColoredLighting)
            {
                light.Color = g_Game.GetLightColor(graphic);
            }
            else
            {
                light.Color = 0;
            }

            light.DrawX = x;
            light.DrawY = y;

            m_LightCount++;
        }
    }
}

void CGameScreen::DrawGameWindow(bool render)
{
    const int playerZPlus5 = g_RenderBounds.PlayerZ + 5;
    if (render)
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4f(g_DrawColor, g_DrawColor, g_DrawColor, 1.0f);
#else
        RenderAdd_SetColor(
            g_renderCmdList, SetColorCmd{ { g_DrawColor, g_DrawColor, g_DrawColor, 1.0f } });
#endif
        if (g_ConfigManager.UseCircleTrans)
        {
            if (g_CircleOfTransparency.Create(g_ConfigManager.CircleTransRadius))
            {
                int drawX = g_RenderBounds.GameWindowCenterX + g_Player->OffsetX;
                int drawY = g_RenderBounds.GameWindowCenterY + g_Player->OffsetY -
                            (g_RenderBounds.PlayerZ * 4 + g_Player->OffsetZ);
                g_CircleOfTransparency.Draw(drawX, drawY);
            }
        }

        m_HitsStack.clear();
        for (int i = 0; i < m_RenderListCount; i++)
        {
            RENDER_OBJECT_DATA &rod = m_RenderList[i];
            CRenderWorldObject *obj = rod.Object;
            if (obj != nullptr)
            {
                g_OutOfRangeColor = rod.GrayColor;
                g_UseCircleTrans =
                    (g_ConfigManager.UseCircleTrans && obj->TranparentTest(playerZPlus5));

                int x = obj->DrawX;
                int y = obj->DrawY;
                obj->Draw(x, y);
                if ((g_ConfigManager.GetDrawStatusState() != 0u) && obj->IsGameObject() &&
                    ((CGameObject *)obj)->NPC && !((CGameCharacter *)obj)->Dead())
                {
                    CGameCharacter *gc = (CGameCharacter *)obj;
                    if (g_ConfigManager.DrawStatusForHumanoids && !gc->IsHuman())
                    {
                        continue;
                    }

                    uint16_t color = g_ConfigManager.GetColorByNotoriety(gc->Notoriety);
                    uint16_t healthColor = 90;
                    int width = gc->MaxHits;
                    if (width > 0)
                    {
                        width = (gc->Hits * 100) / width;
                        if (width > 100)
                        {
                            width = 100;
                        }

                        if (width < 1)
                        {
                            width = 0;
                        }
                    }
                    else
                    {
                        continue;
                    }

                    if (g_ConfigManager.DrawStatusConditionState == DCSCS_ALWAYS ||
                        (g_ConfigManager.DrawStatusConditionState == DCSCS_NOT_MAX &&
                         gc->Hits != gc->MaxHits) ||
                        (g_ConfigManager.DrawStatusConditionState == DCSCS_LOWER &&
                         width < g_ConfigManager.DrawStatusConditionValue))
                    {
                        x += gc->OffsetX;
                        y += gc->OffsetY - gc->OffsetZ;

                        if (g_ConfigManager.GetDrawStatusState() == DCSS_ABOVE)
                        {
                            const auto dims = g_AnimationManager.GetAnimationDimensions(gc, 0);
                            y -= (dims.Height + dims.CenterY) + 24;
                            gc->UpdateHitsTexture(width);
                            x -= (gc->m_HitsTexture.Width / 2) - 3;
                        }
                        else
                        {
                            x -= 20;
                            if (g_ConfigManager.GetDrawStatusState() == DCSS_UNDER)
                            {
                                if (g_TargetGump.TargetedCharacter == obj)
                                {
                                    continue;
                                }
                                if (g_AttackTargetGump.TargetedCharacter == obj)
                                {
                                    continue;
                                }
                                if (g_NewTargetSystem.TargetedCharacter == obj)
                                {
                                    continue;
                                }
                                width = (34 * width) / 100;
                                if (gc->Poisoned())
                                {
                                    healthColor = 63; //Character status line (green)
                                }
                                else if (gc->YellowHits())
                                {
                                    healthColor = 53; //Character status line (green)
                                }
                            }
                        }

                        OBJECT_HITS_INFO hitsInfo = { x,     y,           color,
                                                      width, healthColor, &gc->m_HitsTexture };
                        m_HitsStack.push_back(hitsInfo);
                    }
                }
            }
        }

#ifndef NEW_RENDERER_ENABLED
        glDisable(GL_DEPTH_TEST);
#else
        RenderAdd_DisableDepth(g_renderCmdList);
#endif
        UnuseShader();
        for (int i = 0; i < m_ObjectHandlesCount; i++)
        {
            m_ObjectHandlesList[i]->DrawObjectHandlesTexture();
        }

        g_PluginManager.WorldDraw();
    }
    else
    {
        const bool useCircleTrans =
            (g_ConfigManager.UseCircleTrans &&
             g_CircleOfTransparency.m_Sprite.Select(
                 g_CircleOfTransparency.X, g_CircleOfTransparency.Y));

        for (int i = 0; i < m_RenderListCount; i++)
        {
            CRenderWorldObject *obj = m_RenderList[i].Object;
            if (obj != nullptr)
            {
                g_UseCircleTrans = (useCircleTrans && obj->TranparentTest(playerZPlus5));
                obj->Select(obj->DrawX, obj->DrawY);
            }
        }

        for (int i = 0; i < m_ObjectHandlesCount; i++)
        {
            m_ObjectHandlesList[i]->SelectObjectHandlesTexture();
        }
    }
}

void CGameScreen::DrawGameWindowLight()
{
#ifndef NEW_RENDERER_ENABLED
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
#endif

    if (!UseLight)
    {
        return;
    }

    g_LightColorizerShader.Use();

    if (/*g_LightBuffer.Ready() &&*/ g_LightBuffer.Use())
    {
        float newLightColor = ((32 - g_LightLevel + g_PersonalLightLevel) / 32.0f);

        if (!g_ConfigManager.DarkNights)
        {
            newLightColor += 0.2f;
        }

#ifndef NEW_RENDERER_ENABLED
        glClearColor(newLightColor, newLightColor, newLightColor, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
#else
        RenderAdd_SetClearColor(
            g_renderCmdList,
            SetClearColorCmd{ { newLightColor, newLightColor, newLightColor, 1.f } });
        RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{ ClearRT::ClearRT_Color });
        RenderAdd_SetClearColor(g_renderCmdList, SetClearColorCmd{ g_ColorBlack });

        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_One, BlendFactor::BlendFactor_One });
#endif

        int offsetX = 0;
        int offsetY = 0;

        if (g_ConfigManager.GetUseScaling())
        {
            offsetX = g_RenderBounds.GameWindowPosX - g_RenderBounds.GameWindowScaledOffsetX;
            offsetY = g_RenderBounds.GameWindowPosY - g_RenderBounds.GameWindowScaledOffsetY;
        }

        float translateOffsetX = (float)offsetX;
        float translateOffsetY = (float)offsetY;

#ifndef NEW_RENDERER_ENABLED
        glTranslatef(translateOffsetX, translateOffsetY, 0.0f);
#else
        RenderAdd_SetModelViewTranslation(
            g_renderCmdList,
            SetModelViewTranslationCmd{ { translateOffsetX, translateOffsetY, 0.f } });
#endif

        for (int i = 0; i < m_LightCount; i++)
        {
            g_Game.DrawLight(m_Light[i]);
        }

#ifndef NEW_RENDERER_ENABLED
        glTranslatef(-translateOffsetX, -translateOffsetY, 0.0f);
#else
        RenderAdd_SetModelViewTranslation(
            g_renderCmdList,
            SetModelViewTranslationCmd{ { -translateOffsetX, -translateOffsetY, 0.f } });
#endif

        UnuseShader();

        g_LightBuffer.Release();

#ifndef NEW_RENDERER_ENABLED
        g_GL.RestorePort();

        g_GL.ViewPortScaled(
            g_RenderBounds.GameWindowPosX,
            g_RenderBounds.GameWindowPosY,
            g_RenderBounds.GameWindowWidth,
            g_RenderBounds.GameWindowHeight);

        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
#else
        const auto windowSize = g_GameWindow.GetSize();
        auto viewParams = SetViewParamsCmd{ g_RenderBounds.GameWindowPosX,
                                            g_RenderBounds.GameWindowPosY,
                                            g_RenderBounds.GameWindowWidth,
                                            g_RenderBounds.GameWindowHeight,
                                            windowSize.Width,
                                            windowSize.Height,
                                            -150,
                                            150,
                                            float(g_GlobalScale) };

        RenderAdd_SetViewParams(g_renderCmdList, viewParams);

        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_Zero, BlendFactor::BlendFactor_SrcColor });
#endif

        if (g_ConfigManager.GetUseScaling())
        {
            g_LightBuffer.Draw(
                g_RenderBounds.GameWindowScaledOffsetX, g_RenderBounds.GameWindowScaledOffsetY);
        }
        else
        {
            g_LightBuffer.Draw(g_RenderBounds.GameWindowPosX, g_RenderBounds.GameWindowPosY);
        }

#ifndef NEW_RENDERER_ENABLED
        glDisable(GL_BLEND);
#else
        RenderAdd_DisableBlend(g_renderCmdList);
#endif
    }

    UnuseShader();
}

void CGameScreen::DrawGameWindowText(bool render)
{
    if (render)
    {
        g_FontColorizerShader.Use();
        g_WorldTextRenderer.WorldDraw();
        UnuseShader();

        if ((g_ConfigManager.GetDrawStatusState() != 0u) &&
            (static_cast<unsigned int>(!m_HitsStack.empty()) != 0u))
        {
            if (g_ConfigManager.GetDrawStatusState() == DCSS_ABOVE)
            {
                for (std::vector<OBJECT_HITS_INFO>::iterator it = m_HitsStack.begin();
                     it != m_HitsStack.end();
                     ++it)
                {
                    CTextSprite *texture = it->HitsTexture;
                    texture->Draw(it->X, it->Y);
                }
            }
            else
            {
                g_ColorizerShader.Use();
                for (int i = 0; i < 2; i++)
                {
                    for (std::vector<OBJECT_HITS_INFO>::iterator it = m_HitsStack.begin();
                         it != m_HitsStack.end();
                         ++it)
                    {
                        if (i == 0)
                        {
                            g_Game.DrawGump(0x1068, it->Color, it->X, it->Y);
                        }
                        else if (it->Width != 0)
                        {
                            g_Game.DrawGump(0x1069, it->HealthColor, it->X, it->Y, it->Width, 0);
                        }
                    }
                }
                UnuseShader();
            }
        }

        QFOR(obj, g_World->m_Items, CGameObject *)
        {
            if (obj->NPC)
            {
                CTextContainer &textContainer = obj->GameCharacterPtr()->m_DamageTextControl;
                if (textContainer.Empty())
                {
                    continue;
                }

                QFOR(text, textContainer.m_Items, CTextData *)
                {
                    if (!text->m_TextSprite.Empty())
                    {
                        if (text->Transparent)
                        {
#ifndef NEW_RENDERER_ENABLED
                            glEnable(GL_BLEND);
                            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                            glColor4ub(0xFF, 0xFF, 0xFF, (uint8_t)text->Color);
#else
                            RenderAdd_SetBlend(
                                g_renderCmdList,
                                BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                                               BlendFactor::BlendFactor_OneMinusSrcAlpha });
                            RenderAdd_SetColor(
                                g_renderCmdList,
                                SetColorCmd{ { 1.f, 1.f, 1.f, (uint8_t)text->Color / 255.f } });
#endif

                            text->m_TextSprite.Draw(text->RealDrawX, text->RealDrawY);

#ifndef NEW_RENDERER_ENABLED
                            glDisable(GL_BLEND);
#else
                            RenderAdd_DisableBlend(g_renderCmdList);
#endif
                        }
                        else
                        {
                            text->m_TextSprite.Draw(text->RealDrawX, text->RealDrawY);
                        }
                    }
                }
            }
        }
#ifndef NEW_RENDERER_ENABLED
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
#endif
    }
    else
    {
        g_WorldTextRenderer.Select(nullptr);
    }
}

void CGameScreen::PrepareContent()
{
    g_WorldTextRenderer.CalculateWorldPositions(false);

    m_GameScreenGump.PrepareContent();

    g_GumpManager.PrepareContent();

    if (g_SelectedObject.Gump != nullptr &&
        (g_SelectedObject.Gump->GumpType == GT_STATUSBAR ||
         g_SelectedObject.Gump->GumpType == GT_POPUP_MENU) &&
        g_SelectedObject.Gump->Serial != g_PlayerSerial)
    {
        g_StatusbarUnderMouse = g_SelectedObject.Gump->Serial;
    }
    else
    {
        g_StatusbarUnderMouse = 0;
    }

    //if (g_SelectedObject.Object() != nullptr && g_SelectedObject.Object()->IsGameObject() && g_PressedObject.LeftObject == g_SelectedObject.Object())
    if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGameObject() &&
        g_MouseManager.LastLeftButtonClickTimer < g_Ticks)
    {
        CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

        if (CanBeDraggedByOffset(offset) ||
            (g_MouseManager.LastLeftButtonClickTimer + g_MouseManager.DoubleClickDelay < g_Ticks))
        {
            CGameItem *selobj = g_World->FindWorldItem(g_PressedObject.LeftSerial);

            if (selobj != nullptr && !g_ObjectInHand.Enabled && !selobj->Locked() &&
                GetDistance(g_Player, selobj) <= DRAG_ITEMS_DISTANCE)
            {
                if (selobj->Serial >= 0x40000000 && !g_GrayedPixels) //Item selection
                {
                    if (selobj->IsStackable() && selobj->Count > 1 && !g_ShiftPressed)
                    {
                        CGumpDrag *newgump = new CGumpDrag(
                            g_PressedObject.LeftSerial,
                            g_MouseManager.Position.X - 80,
                            g_MouseManager.Position.Y - 34);

                        g_GumpManager.AddGump(newgump);
                        g_GameWindow.EmulateOnLeftMouseButtonDown();
                        selobj->Dragged = true;
                    }
                    else if (!g_Target.IsTargeting())
                    {
                        g_Game.PickupItem(selobj);
                        g_PressedObject.ClearLeft(); //g_LastObjectLeftMouseDown = 0;
                    }
                }
            }
            else if (!g_ObjectInHand.Enabled)
            {
                CGameCharacter *selchar = g_World->FindWorldCharacter(g_PressedObject.LeftSerial);

                if (selchar != nullptr) //Character selection
                {
                    g_Game.OpenStatus(selchar->Serial);
                    g_GeneratedMouseDown = true;
                    g_GameWindow.EmulateOnLeftMouseButtonDown();
                    PLUGIN_EVENT(UOMSG_STATUS_REQUEST, selchar->Serial);
                }
            }
        }
    }
}

void CGameScreen::PreRender()
{
    if (!RenderListInitalized)
    {
        CalculateRenderList();
    }

    if (g_DeathScreenTimer < g_Ticks)
    {
        if ((g_DeathScreenTimer != 0u) && g_ScreenEffectManager.UseSunrise())
        {
            SmoothScreenAction = 0;
        }

        g_DeathScreenTimer = 0;
    }
    else if (!g_Player->Dead())
    {
        g_DeathScreenTimer = 0;
    }
}

void CGameScreen::Render()
{
    PreRender();

    static uint32_t lastRender = 0;
    static int currentFPS = 0;
    static int FPScount = 0;
    static int FPSMax = 0;
    static int FPSMin = INT_MAX;

    if (lastRender < g_Ticks)
    {
        DEBUG(Client, "FPS=%i (Min=%d, Max=%d)", FPScount, FPSMin, FPSMax);
        FPScount = currentFPS;
        FPSMax = currentFPS > FPSMax ? currentFPS : FPSMax;
        FPSMin = currentFPS < FPSMin && currentFPS != 0 ? currentFPS : FPSMin;
        currentFPS = 1;
        lastRender = g_Ticks + 1000;
    }
    else
    {
        currentFPS++;
    }

#ifdef NEW_RENDERER_ENABLED
    Render_ResetCmdList(&m_RenderCmdList, Render_DefaultState());
    RenderAdd_FlushState(&m_RenderCmdList);
#endif

    // TODO renderer BeginDraw exists only to set Drawing to true, as it's used by
    // Gump-something; investigate it and remove/move it elsewhere so
    // we can get rid of glEngine
    g_GL.BeginDraw();
#ifdef NEW_RENDERER_ENABLED
    RenderAdd_ClearRT(&m_RenderCmdList, ClearRTCmd{});
#endif
    if (DrawSmoothMonitor() != 0)
    {
        return;
    }

    uint32_t deathScreenTimer = g_DeathScreenTimer;
    if ((deathScreenTimer != 0u) && g_ScreenEffectManager.Mode != SEM_NONE)
    {
        deathScreenTimer = 0;
    }

    CSelectedObject tempSelected;
    CPressedObject tempPressed;
    if (g_GameState == GS_GAME_BLOCKED)
    {
        tempSelected.Init(g_SelectedObject);
        g_SelectedObject.Clear();
        tempPressed.Init(g_PressedObject);
        g_PressedObject.ClearAll();
    }

    g_RenderedObjectsCountInGameWindow = 0;
    g_TargetGump.Color = 0;
    g_TargetGump.TargetedCharacter = nullptr;
    g_AttackTargetGump.Color = 0;
    g_AttackTargetGump.TargetedCharacter = nullptr;
    g_NewTargetSystem.ColorGump = 0;
    g_NewTargetSystem.TargetedCharacter = nullptr;

    m_LightCount = 0;

#ifndef NEW_RENDERER_ENABLED
    g_GL.ViewPortScaled(
        g_RenderBounds.GameWindowPosX,
        g_RenderBounds.GameWindowPosY,
        g_RenderBounds.GameWindowWidth,
        g_RenderBounds.GameWindowHeight);
#else
    const auto windowSize = g_GameWindow.GetSize();
    auto viewParams = SetViewParamsCmd{ g_RenderBounds.GameWindowPosX,
                                        g_RenderBounds.GameWindowPosY,
                                        g_RenderBounds.GameWindowWidth,
                                        g_RenderBounds.GameWindowHeight,
                                        windowSize.Width,
                                        windowSize.Height,
                                        -150,
                                        150,
                                        float(g_GlobalScale) };
    RenderAdd_SetViewParams(g_renderCmdList, viewParams);
#endif

    g_DrawColor = 1.0f;

    if (g_GrayedPixels)
    {
        g_DeathShader.Use();
    }
    else
    {
        g_ColorizerShader.Use();
    }

    DrawGameWindow(true);
    UnuseShader();
    if (deathScreenTimer == 0u)
    {
        if (!g_GrayedPixels)
        {
            DrawGameWindowLight();
            g_ColorizerShader.Use();
            g_NewTargetSystem.Draw();
            g_TargetGump.Draw();
            g_AttackTargetGump.Draw();
            UnuseShader();
            g_Weather.Draw(g_RenderBounds.GameWindowPosX, g_RenderBounds.GameWindowPosY);
        }

        DrawGameWindowText(true);
        DrawSmoothMonitorEffect();
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        g_GL.DrawPolygone(
            g_RenderBounds.GameWindowPosX,
            g_RenderBounds.GameWindowPosY,
            g_RenderBounds.GameWindowWidth,
            g_RenderBounds.GameWindowHeight);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
        RenderAdd_DrawUntexturedQuad(
            g_renderCmdList,
            DrawUntexturedQuadCmd{ g_RenderBounds.GameWindowPosX,
                                   g_RenderBounds.GameWindowPosY,
                                   uint32_t(g_RenderBounds.GameWindowWidth),
                                   uint32_t(g_RenderBounds.GameWindowHeight),
                                   g_ColorBlack });
#endif

        g_FontManager.DrawA(
            3,
            "You are dead.",
            0,
            g_RenderBounds.GameWindowPosX + (g_RenderBounds.GameWindowWidth / 2) - 50,
            g_RenderBounds.GameWindowPosY + (g_RenderBounds.GameWindowHeight / 2) - 20);
    }

    g_OutOfRangeColor = 0;
    g_GrayedPixels = false;
    UnuseShader();
    if (deathScreenTimer == 0u)
    {
        g_SystemChat.DrawSystemChat(
            g_RenderBounds.GameWindowPosX,
            g_RenderBounds.GameWindowPosY,
            g_RenderBounds.GameWindowHeight);
        g_QuestArrow.Draw();
    }

#ifndef NEW_RENDERER_ENABLED
    g_GL.RestorePort();
#else
    auto viewParamsCmd = SetViewParamsCmd{
        0, 0, windowSize.Width, windowSize.Height, windowSize.Width, windowSize.Height, -150, 150
    };
    RenderAdd_SetViewParams(g_renderCmdList, viewParamsCmd);
#endif
    m_GameScreenGump.Draw();

    if (g_DeveloperMode == DM_SHOW_FPS_ONLY)
    {
        char dbf[100] = { 0 };

        sprintf_s(
            dbf,
            "FPS=%i (%ims) (Min=%d, Max=%d) scale=%.1f\n%s",
            FPScount,
            g_FrameDelay[WINDOW_ACTIVE],
            FPSMin,
            FPSMax,
            g_GlobalScale,
            g_PingString.c_str());

        g_FontManager.DrawA(
            3,
            dbf,
            0x35,
            g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth + 10,
            g_RenderBounds.GameWindowPosY);
    }
    else if (g_DeveloperMode == DM_DEBUGGING)
    {
        char dbf[150] = { 0 };

        sprintf_s(
            dbf,
            "FPS=%i (%ims) (Min=%d, Max=%d) %sDir=%i Z=%i (MDZ=%i) scale=%.1f",
            FPScount,
            g_FrameDelay[WINDOW_ACTIVE],
            FPSMin,
            FPSMax,
            g_PingString.c_str(),
            g_Player->Direction,
            g_RenderBounds.PlayerZ,
            m_MaxDrawZ,
            g_GlobalScale);

        g_FontManager.DrawA(3, dbf, 0x35, 20, 30);

        sprintf_s(
            dbf,
            "Rendered %i object counts:\nLand=%i Statics=%i Game=%i Multi=%i Lights=%i",
            g_RenderedObjectsCountInGameWindow,
            g_LandObjectsCount,
            g_StaticsObjectsCount,
            g_GameObjectsCount,
            g_MultiObjectsCount,
            m_LightCount);

        g_FontManager.DrawA(3, dbf, 0x35, 20, 54);

        if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsWorldObject())
        {
            CRenderWorldObject *selRwo = (CRenderWorldObject *)g_SelectedObject.Object;
            CLandObject *land = selRwo->LandObjectPtr();
            char soName[20] = "UnknownObject";

            switch (selRwo->RenderType)
            {
                case ROT_LAND_OBJECT:
                {
                    if (!land->IsStretched)
                    {
                        sprintf_s(soName, "Land");
                    }
                    else
                    {
                        sprintf_s(soName, "LandTex (mz=%i)", land->MinZ);
                    }

                    break;
                }
                case ROT_STATIC_OBJECT:
                {
                    sprintf_s(soName, "Static");
                    break;
                }
                case ROT_GAME_OBJECT:
                {
                    sprintf_s(soName, "GameObject");
                    break;
                }
                case ROT_MULTI_OBJECT:
                {
                    sprintf_s(soName, "Multi");

                    if (((CMultiObject *)selRwo)->IsCustomHouseMulti())
                    {
                        sprintf_s(soName, "Multi CH %04X", ((CMultiObject *)selRwo)->State);
                    }
                    else
                    {
                        sprintf_s(soName, "Multi");
                    }
                    break;
                }
                default:
                    break;
            }

            int tz = selRwo->GetZ();

            //Если это тайл текстуры
            if (land != nullptr && land->IsStretched)
            {
                tz = (char)land->Serial;
            }

            uint32_t tiledataFlags = (uint32_t)(
                selRwo->IsStaticGroupObject() ?
                    ((CRenderStaticObject *)selRwo)->GetStaticData()->Flags :
                    0);

            sprintf_s(
                dbf,
                "Selected:\n%s: G=0x%04X C:0x%04X TF=0x%08X X=%i Y=%i Z=%i (%i) PriZ=%i",
                soName,
                selRwo->Graphic,
                selRwo->Color,
                tiledataFlags,
                selRwo->GetX(),
                selRwo->GetY(),
                selRwo->GetZ(),
                tz,
                selRwo->PriorityZ);

            const std::string flagNames[] = {
                "Background", "Weapon",     "Transparent", "Translucent", "Wall",     "Damaging",
                "Impassable", "Wet",        "Unknown",     "Surface",     "Bridge",   "Stackable",
                "Window",     "NoShoot",    "PrefixA",     "PrefixAn",    "Internal", "Foliage",
                "PartialHue", "Unknown1",   "Map",         "Container",   "Wearable", "LightSource",
                "Animated",   "NoDiagonal", "Unknown2",    "Armor",       "Roof",     "Door",
                "StairBack",  "StairRight"
            };

            std::string flagsData{};
            for (int f = 0; f < 32; f++)
            {
                if ((tiledataFlags & (1 << f)) != 0u)
                {
                    flagsData += std::string("\n") + flagNames[f];
                }
            }

            flagsData = std::string(dbf) + flagsData;

            g_FontManager.DrawA(3, flagsData, 0x0035, 20, 102);
        }
    }

    g_GumpManager.Draw(false);
    g_GameConsole.DrawW(
        (uint8_t)g_ConfigManager.SpeechFont,
        g_ConfigManager.SpeechColor,
        g_RenderBounds.GameWindowPosX,
        g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight - 18,
        TS_LEFT,
        UOFONT_BLACK_BORDER | UOFONT_FIXED);

    g_PluginManager.SceneDraw();
    if (g_GameState == GS_GAME_BLOCKED)
    {
        g_SelectedObject.Init(tempSelected);
        g_PressedObject.Init(tempPressed);
        g_GameBlockedScreen.SelectObject();
        g_GameBlockedScreen.Render();
    }
    else
    {
        InitToolTip();
        g_MouseManager.Draw(g_MouseManager.GetGameCursor()); //Game Gump mouse cursor
    }

    RenderDraw_Execute(&m_RenderCmdList);

    g_GL.EndDraw();

#ifdef NEW_RENDERER_ENABLED
    Render_SwapBuffers();
    g_ScreenshotBuilder.GPUDataReady();
#endif
}

void CGameScreen::SelectObject()
{
    const auto oldScale = g_GlobalScale;
    g_GlobalScale = 1.0;
    g_SelectedObject.Clear();
    g_StatusbarUnderMouse = 0;
    g_GumpManager.Select(false);
    if (g_SelectedObject.Gump != nullptr)
    {
        if (g_SelectedObject.Object->IsText())
        {
            ((CRenderTextObject *)g_SelectedObject.Object)->ToTop();
        }
    }
    else if (m_GameScreenGump.Select() == nullptr)
    {
        if (g_DeathScreenTimer == 0u)
        {
            DrawGameWindowText(false);
            if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsText())
            {
                g_WorldTextRenderer.ToTop((CRenderTextObject *)g_SelectedObject.Object);
            }
        }

        if (g_SelectedObject.Object == nullptr) //Если ничего не выбралось - пройдемся по объектам
        {
            //Если курсор мыши в игровом окне - просканируем его
            if (g_MouseManager.Position.X < g_RenderBounds.GameWindowPosX ||
                g_MouseManager.Position.Y < g_RenderBounds.GameWindowPosY ||
                g_MouseManager.Position.X >
                    (g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth) ||
                g_MouseManager.Position.Y >
                    (g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight))
            {
                // do nothing
            }
            else
            {
                g_GlobalScale = oldScale;
                CPoint2Di oldMouse = g_MouseManager.Position;

                //g_MouseManager.Position = CPoint2Di((int)((oldMouse.X - (g_RenderBounds.GameWindowScaledOffsetX / g_GlobalScale)) * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetX, (int)((oldMouse.Y - (g_RenderBounds.GameWindowScaledOffsetY / g_GlobalScale)) * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetY);

                //g_MouseManager.Position = CPoint2Di((int)((oldMouse.X * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetX / g_GlobalScale), (int)((oldMouse.Y * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetY / g_GlobalScale));

                //int mouseX = (int)((oldMouse.X * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetX / g_GlobalScale);
                // int mouseY = (int)((oldMouse.Y * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetY / g_GlobalScale);

                /*g_MouseManager.Position = CPoint2Di
                (
                    //(int)((oldMouse.X * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetX)
                    mouseX
                    ,
                    //(int)((oldMouse.Y * g_GlobalScale) + g_RenderBounds.GameWindowScaledOffsetY)
                    mouseY
                );*/

                /*const auto left = (float)g_RenderBounds.GameWindowPosX;
                const auto right = (float)(g_RenderBounds.GameWindowWidth + left);
                const auto top = (float)g_RenderBounds.GameWindowPosY;
                const auto bottom = (float)(g_RenderBounds.GameWindowHeight + top);

                const auto newRight = right * g_GlobalScale;
                const auto newBottom = bottom * g_GlobalScale;

                g_RenderBounds.GameWindowScaledOffsetX = (int)((left * g_GlobalScale) - (newRight - right));
                g_RenderBounds.GameWindowScaledOffsetY = (int)((top * g_GlobalScale) - (newBottom - bottom));

                g_RenderBounds.GameWindowScaledWidth = (int)(newRight - g_RenderBounds.GameWindowScaledOffsetX);
                g_RenderBounds.GameWindowScaledHeight = (int)(newBottom - g_RenderBounds.GameWindowScaledOffsetY);*/

                DrawGameWindow(false);
                g_MouseManager.Position = oldMouse;
            }
        }
    }

    g_GlobalScale = oldScale;
    if (g_SelectedObject.Object != g_LastSelectedObject.Object)
    {
        if (g_LastSelectedObject.Object != nullptr)
        {
            g_LastSelectedObject.Object->OnMouseExit();
        }

        if (g_SelectedObject.Object != nullptr)
        {
            g_SelectedObject.Object->OnMouseEnter();
        }
    }

    g_LastSelectedObject.Init(g_SelectedObject);
}

void CGameScreen::OnLeftMouseButtonDown()
{
    CGumpSkills *skillGump = (CGumpSkills *)g_GumpManager.GetGump(0, 0, GT_SKILLS);
    if (skillGump != nullptr)
    {
        skillGump->UpdateGroupText();
    }

    if (g_SelectedObject.Gump == &m_GameScreenGump)
    {
        m_GameScreenGump.OnLeftMouseButtonDown();
    }
    else if (g_SelectedObject.Gump != nullptr)
    {
        //g_SelectGumpObjects = true;
        g_GumpManager.OnLeftMouseButtonDown(false);
        //g_SelectGumpObjects = false;
    }

    if (g_PopupMenu != nullptr && g_SelectedObject.Gump != g_PopupMenu)
    {
        g_GumpManager.RemoveGump(g_PopupMenu);
    }
}

void CGameScreen::OnLeftMouseButtonUp()
{
    if (g_PressedObject.LeftGump == &m_GameScreenGump)
    {
        m_GameScreenGump.OnLeftMouseButtonUp();
        return;
    }
    if (g_MouseManager.LeftButtonPressed &&
        (g_PressedObject.LeftGump != nullptr || g_ObjectInHand.Enabled))
    {
        if (g_GumpManager.OnLeftMouseButtonUp(false))
        {
            return;
        }
    }

    int gameWindowPosX = g_ConfigManager.GameWindowX - 4;
    int gameWindowPosY = g_ConfigManager.GameWindowY - 4;
    if (g_MouseManager.Position.X < gameWindowPosX || g_MouseManager.Position.Y < gameWindowPosY ||
        g_MouseManager.Position.X > (gameWindowPosX + g_ConfigManager.GameWindowWidth) ||
        g_MouseManager.Position.Y > (gameWindowPosY + g_ConfigManager.GameWindowHeight))
    {
        return;
    }

    if (g_SelectedObject.Object != nullptr)
    {
        CRenderWorldObject *rwo = nullptr;
        if (g_SelectedObject.Object->IsWorldObject())
        {
            rwo = (CRenderWorldObject *)g_SelectedObject.Object;
        }

        if (g_CustomHouseGump != nullptr && g_Target.IsTargeting())
        {
            g_CustomHouseGump->OnTargetWorld(rwo);
            g_MouseManager.LastLeftButtonClickTimer = 0;
            return;
        }

        if (g_Target.IsTargeting() && !g_ObjectInHand.Enabled)
        {
            if (g_SelectedObject.Object->IsText())
            {
                CTextData *td = (CTextData *)g_SelectedObject.Object;
                if (td->Type == TT_OBJECT)
                {
                    CGameObject *obj = g_World->FindWorldObject(td->Serial);
                    if (obj != nullptr && (obj->NPC || obj->IsCorpse()))
                    {
                        g_Target.SendTargetObject(td->Serial);
                    }
                }
                g_MouseManager.LastLeftButtonClickTimer = 0;
                return;
            }
            if (rwo != nullptr)
            {
                if (rwo->IsGameObject())
                {
                    g_Target.SendTargetObject(rwo->Serial);
                }
                else if (rwo->IsLandObject())
                {
                    g_Target.SendTargetTile(
                        0 /*g_SelectedObject->Index*/, rwo->GetX(), rwo->GetY(), rwo->GetZ());
                }
                else if (rwo->IsStaticObject() || rwo->IsMultiObject())
                {
                    MulStaticTile2 *st = nullptr;
                    if (g_Config.ClientVersion >= CV_7090 && rwo->IsSurface())
                    {
                        st = ((CRenderStaticObject *)rwo)->GetStaticData();
                    }

                    short targetZ = rwo->GetZ();
                    if (st != nullptr)
                    {
                        targetZ += st->Height;
                    }

                    g_Target.SendTargetTile(rwo->Graphic, rwo->GetX(), rwo->GetY(), (char)targetZ);
                }
                g_MouseManager.LastLeftButtonClickTimer = 0;
                return;
            }
        }

        if (rwo != nullptr)
        {
            uint32_t drop_container = 0xFFFFFFFF;
            bool can_drop = false;
            uint16_t dropX = 0;
            uint16_t dropY = 0;
            char dropZ = 0;
            if (rwo->IsGameObject() && g_ObjectInHand.Enabled)
            {
                CGameObject *target = (CGameObject *)rwo;
                can_drop = (GetDistance(g_Player, target) <= DRAG_ITEMS_DISTANCE);
                if (can_drop && target != nullptr)
                {
                    if (target->IsContainer() || target->NPC)
                    {
                        dropX = 0xFFFF;
                        dropY = 0xFFFF;
                        dropZ = 0;
                        drop_container = target->Serial;
                    }
                    else if (
                        target->IsSurface() ||
                        (target->IsStackable() && target->Graphic == g_ObjectInHand.Graphic))
                    {
                        if (!target->IsSurface())
                        {
                            drop_container = target->Serial;
                        }
                        dropX = target->GetX();
                        dropY = target->GetY();
                        dropZ = target->GetZ();
                    }
                }
                else
                {
                    g_Game.PlaySoundEffect(0x0051);
                }
            }
            else if (
                (rwo->IsLandObject() || rwo->IsStaticObject() || rwo->IsMultiObject()) &&
                g_ObjectInHand.Enabled)
            {
                can_drop =
                    (GetDistance(g_Player, CPoint2Di(rwo->GetX(), rwo->GetY())) <=
                     DRAG_ITEMS_DISTANCE);
                if (can_drop)
                {
                    dropX = rwo->GetX();
                    dropY = rwo->GetY();
                    dropZ = rwo->GetZ();
                }
                else
                {
                    g_Game.PlaySoundEffect(0x0051);
                }
            }

            if (can_drop /*&& ObjectInHand != nullptr*/)
            {
                if (drop_container == 0xFFFFFFFF && (dropX == 0u) && (dropY == 0u))
                {
                    can_drop = false;
                }

                if (can_drop)
                {
                    g_Game.DropItem(drop_container, dropX, dropY, dropZ);
                }
            }
            else if (!g_ObjectInHand.Enabled)
            {
                if (rwo->IsGameObject())
                {
                    if (!g_ClickObject.Enabled)
                    {
                        g_ClickObject.Init(rwo);
                        g_ClickObject.Timer = g_Ticks + g_MouseManager.DoubleClickDelay;
                    }
                }
                else
                {
                    if (rwo->IsStaticObject() || rwo->IsMultiObject())
                    {
                        CTextData *td =
                            (CTextData *)rwo->StaticGroupObjectPtr()->m_TextControl->m_Items;
                        if (td == nullptr || td->Timer < g_Ticks)
                        {
                            uint16_t id = rwo->Graphic;
                            auto str = g_ClilocManager.Cliloc(g_Language)
                                           ->GetW(1020000 + id, true, g_Data.m_Static[id].Name);
                            if (str.length() != 0u)
                            {
                                // const uint8_t font = g_ConfigManager.ToolTipsTextFont; ?
                                if (g_Config.ClientVersion >= CV_6000)
                                {
                                    g_Game.CreateUnicodeTextMessage(
                                        TT_CLIENT, 0, 1, 0x03B2, str, rwo);
                                }
                                else
                                {
                                    g_Game.CreateTextMessage(
                                        TT_CLIENT, 0, 3, 0x03B2, str_from(str), rwo);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (g_EntryPointer != &g_GameConsole && g_EntryPointer != nullptr)
    {
        CGump *gumpEntry = g_GumpManager.GetTextEntryOwner();
        if (g_ConfigManager.GetConsoleNeedEnter())
        {
            g_EntryPointer = nullptr;
        }
        else
        {
            g_EntryPointer = &g_GameConsole;
        }

        if (gumpEntry != nullptr)
        {
            gumpEntry->FrameCreated = false;
        }
    }
}

bool CGameScreen::OnLeftMouseButtonDoubleClick()
{
    bool result = false;
    uint32_t charUnderMouse = 0;
    if ((g_SelectedObject.Gump != nullptr) && g_GumpManager.OnLeftMouseButtonDoubleClick(false))
    {
        result = true;
    }
    else if (g_SelectedObject.Object != nullptr)
    {
        if (g_SelectedObject.Object->IsGameObject())
        {
            CGameObject *obj = (CGameObject *)g_SelectedObject.Object;
            if (!obj->NPC)
            {
                g_Game.DoubleClick(g_SelectedObject.Serial);
            }
            else
            {
                charUnderMouse = g_SelectedObject.Serial;
            }
            result = true;
        }
        else if (g_SelectedObject.Object->IsText())
        {
            CTextData *td = (CTextData *)g_SelectedObject.Object;
            if (td->Type == TT_OBJECT)
            {
                uint32_t serial = td->Serial;
                CGameObject *obj = g_World->FindWorldObject(serial);
                if (obj != nullptr && (obj->NPC || obj->IsCorpse()))
                {
                    if (obj->NPC)
                    {
                        charUnderMouse = serial;
                    }
                    else
                    {
                        g_Game.DoubleClick(serial);
                    }
                    result = true;
                }
            }
        }
    }

    if (charUnderMouse != 0)
    {
        if (!g_ConfigManager.DisableNewTargetSystem &&
            (charUnderMouse != g_PlayerSerial || g_Player->FindLayer(OL_MOUNT) == nullptr))
        {
            g_GumpManager.CloseGump(g_NewTargetSystem.Serial, 0, GT_TARGET_SYSTEM);
            g_NewTargetSystem.Serial = charUnderMouse;
            if (g_GumpManager.UpdateContent(charUnderMouse, 0, GT_TARGET_SYSTEM) == nullptr)
            {
                CPacketStatusRequest(charUnderMouse).Send();
                g_GumpManager.AddGump(new CGumpTargetSystem(
                    charUnderMouse, g_NewTargetSystem.GumpX, g_NewTargetSystem.GumpY));
            }
        }

        if (g_Player->Warmode && charUnderMouse != g_PlayerSerial)
        {
            g_Game.Attack(charUnderMouse);
        }
        else
        {
            g_Game.DoubleClick(charUnderMouse);
        }
    }
    return result;
}

void CGameScreen::OnRightMouseButtonDown()
{
    if (g_PressedObject.RightGump != nullptr)
    {
        g_GumpManager.OnRightMouseButtonDown(false);
    }

    if (g_PopupMenu != nullptr && g_SelectedObject.Gump != g_PopupMenu)
    {
        g_GumpManager.RemoveGump(g_PopupMenu);
    }
}

void CGameScreen::OnRightMouseButtonUp()
{
    if (g_PressedObject.RightGump != nullptr)
    {
        g_GumpManager.OnRightMouseButtonUp(false);
    }
    else if (
        g_PressedObject.RightObject != nullptr && g_PressedObject.RightObject->IsGameObject() &&
        g_SelectedGameObjectHandle == g_PressedObject.RightSerial)
    {
        ((CGameObject *)g_PressedObject.RightObject)->ClosedObjectHandle = true;
    }

    if ((g_ShiftPressed && !g_CtrlPressed && !g_AltPressed) &&
        g_ConfigManager.HoldShiftForEnablePathfind && g_ConfigManager.EnablePathfind &&
        g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsWorldObject() &&
        !g_PathFinder.AutoWalking)
    {
        CRenderWorldObject *rwo = (CRenderWorldObject *)g_SelectedObject.Object;
        if (rwo->IsLandObject() || rwo->IsSurface())
        {
            if (g_PathFinder.WalkTo(rwo->GetX(), rwo->GetY(), rwo->GetZ(), 0))
            {
                g_Game.CreateTextMessage(TT_OBJECT, g_PlayerSerial, 3, 0, "Pathfinding!");
            }
        }
    }
}

bool CGameScreen::OnRightMouseButtonDoubleClick()
{
    if (g_ConfigManager.EnablePathfind && g_SelectedObject.Object != nullptr &&
        g_SelectedObject.Object->IsWorldObject() && !g_PathFinder.AutoWalking)
    {
        CRenderWorldObject *rwo = (CRenderWorldObject *)g_SelectedObject.Object;
        if (rwo->IsLandObject() || rwo->IsSurface())
        {
            if (g_PathFinder.WalkTo(rwo->GetX(), rwo->GetY(), rwo->GetZ(), 0))
            {
                g_Game.CreateTextMessage(TT_OBJECT, g_PlayerSerial, 3, 0, "Pathfinding!");
                return true;
            }
        }
    }

    return false;
}

void CGameScreen::OnMidMouseButtonScroll(bool up)
{
    if (g_SelectedObject.Gump != nullptr)
    {
        g_GumpManager.OnMidMouseButtonScroll(up, false);
    }
    else if (g_ConfigManager.GetUseScaling())
    {
        int gameWindowPosX = g_ConfigManager.GameWindowX - 4;
        int gameWindowPosY = g_ConfigManager.GameWindowY - 4;

        if (g_MouseManager.Position.X < gameWindowPosX ||
            g_MouseManager.Position.Y < gameWindowPosY ||
            g_MouseManager.Position.X > (gameWindowPosX + g_ConfigManager.GameWindowWidth) ||
            g_MouseManager.Position.Y > (gameWindowPosY + g_ConfigManager.GameWindowHeight))
        {
            return;
        }

        if (up)
        {
            g_GlobalScale += 0.1f;
        }
        else
        {
            g_GlobalScale -= 0.1f;
        }

        if (g_GlobalScale < 0.7f)
        {
            g_GlobalScale = 0.7f;
        }
        else if (g_GlobalScale > 2.3f)
        {
            g_GlobalScale = 2.3f;
        }
    }
}

void CGameScreen::OnDragging()
{
    if (g_PressedObject.LeftGump != nullptr)
    {
        g_GumpManager.OnDragging(false);
    }
}

void CGameScreen::OnTextInput(const TextEvent &ev)
{
    if (g_EntryPointer == nullptr)
    {
        return; //Ignore no print keys
    }

    const auto ch = EvChar(ev);
    if (g_EntryPointer != &g_GameConsole && ch != 0x11 && ch != 0x17)
    {
        if (g_GumpManager.OnTextInput(ev, false))
        {
            return;
        }
    }

    const auto mod = SDL_GetModState();
    const bool altGR = (mod & KMOD_RALT) != 0;
    const bool altPressed = (mod & KMOD_ALT) != 0;
    const bool ctrlPressed = (mod & KMOD_CTRL) != 0;

    if ((altGR || (!altPressed && !ctrlPressed)) &&
        (int)g_EntryPointer->Length() < std::max(g_EntryPointer->MaxLength, 60))
    {
        g_EntryPointer->Insert(ch);
    }
}

void CGameScreen::OnKeyDown(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    if (key == KEY_TAB && ev.repeat)
    {
        return;
    }

    if (g_GumpManager.OnKeyDown(ev, false))
    {
        return;
    }

    if (g_EntryPointer == &g_GameConsole)
    {
        g_EntryPointer->OnKey(nullptr, key);
    }

    switch (key)
    {
        case KEY_CLEAR:
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            if (g_EntryPointer != nullptr)
            {
                if (g_EntryPointer == &g_GameConsole)
                {
                    if (g_ConsolePrompt != 0u)
                    {
                        g_Game.ConsolePromptSend();
                    }
                    else if (g_EntryPointer->Length() != 0u)
                    {
                        g_GameConsole.SaveConsoleMessage();
                        g_GameConsole.Send();
                    }
                    g_GameConsole.Clear();
                }

                if (g_ConfigManager.GetConsoleNeedEnter())
                {
                    g_EntryPointer = nullptr;
                }
                else
                {
                    g_EntryPointer = &g_GameConsole;
                }
            }
            else
            {
                g_EntryPointer = &g_GameConsole;
            }

            if (g_GumpConsoleType != nullptr)
            {
                g_GumpConsoleType->SetConsolePrefix();
            }
            break;
        }
        case KEY_PAGEUP:
        {
            //walk N (0)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 0);
            }
            break;
        }
        case KEY_PAGEDOWN:
        {
            //walk E (2)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 2);
            }
            break;
        }
        case KEY_HOME:
        {
            //walk W (6)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 6);
            }
            break;
        }
        case KEY_END:
        {
            //walk S (4)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 4);
            }
            break;
        }
        case KEY_UP:
        {
            //Walk NW (7)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 7);
            }
            break;
        }
        case KEY_LEFT:
        {
            if (g_GameConsole.Length() > 0)
            {
                return;
            }
            //Walk SW (5)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 5);
            }
            break;
        }
        case KEY_DOWN:
        {
            //Walk SE (3)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 3);
            }
            break;
        }
        case KEY_RIGHT:
        {
            if (g_GameConsole.Length() > 0)
            {
                return;
            }
            //Walk NE (1)
            if (!g_PathFinder.AutoWalking)
            {
                g_PathFinder.Walk(g_ConfigManager.AlwaysRun, 1);
            }
            break;
        }
        case KEY_ESCAPE:
        {
            if (g_Target.IsTargeting())
            {
                g_Target.SendCancelTarget();
            }
            else if (
                (g_NewTargetSystem.Serial != 0u) &&
                (!g_ConfigManager.CancelNewTargetSystemOnShiftEsc || g_ShiftPressed))
            {
                g_NewTargetSystem.Serial = 0;
            }
            else if (g_PathFinder.AutoWalking && g_PathFinder.PathFindidngCanBeCancelled)
            {
                g_PathFinder.StopAutoWalk();
            }

            if (g_ConsolePrompt != 0u)
            {
                g_Game.ConsolePromptCancel();
            }
            break;
        }
        default:
            break;
    }

    if (key == KEY_TAB)
    {
        if (g_ConfigManager.HoldTabForCombat)
        {
            g_Game.ChangeWarmode(1);
        }
        else
        {
            g_Game.ChangeWarmode();
        }
    }

    const auto mod = SDL_GetModState();
    const bool altPressed = (mod & KMOD_ALT) != 0;
    const bool ctrlPressed = (mod & KMOD_CTRL) != 0;
    const bool shiftPressed = (mod & KMOD_SHIFT) != 0;

    // Disable macros to avoid mixing with a chat input.
    // If you activate the chat, you want to write a message not run a macro.
    if (g_ConfigManager.GetConsoleNeedEnter() && g_EntryPointer == &g_GameConsole)
    {
        return;
    }
    else if (key == SDLK_q && ctrlPressed)
    {
        g_GameConsole.ChangeConsoleMessage(false);
    }

    auto macro = g_MacroManager.FindMacro(key, altPressed, ctrlPressed, shiftPressed);
    const bool canExecuteMacro =
        !(g_ConfigManager.DisableMacroInChat && g_EntryPointer == &g_GameConsole &&
          g_GameConsole.InChat());
    if (macro != nullptr && canExecuteMacro)
    {
        g_MacroManager.ChangePointer((CMacroObject *)macro->m_Items);
        g_MacroManager.WaitingBandageTarget = false;
        g_MacroManager.WaitForTargetTimer = 0;
        g_MacroManager.Execute();
    }
}

void CGameScreen::OnKeyUp(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    if (key == KEY_TAB && g_GameState == GS_GAME)
    {
        if (g_ConfigManager.HoldTabForCombat)
        {
            g_Game.ChangeWarmode(0);
        }
    }
}
