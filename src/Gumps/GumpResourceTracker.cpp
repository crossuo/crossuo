// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Jean-Martin Miljours
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include <vector>
#include <xuocore/uodata.h>
#include <algorithm>      // std::min, std::max
#include "GumpResourceTracker.h"
#include "../Spells.h"
#include "../Managers/GumpManager.h"
#include "../GUI/GUIColoredPolygone.h"
#include "../Managers/MouseManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ConfigManager.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GamePlayer.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../CrossUO.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderShader.h" // g_ColorizerShader and friends
#include "xuocore/enumlist.h"

static constexpr int GRID_SIZE = 20;
static constexpr int MAX_WIDTH = GRID_SIZE * TRACKER_BOX_SIZE;
static constexpr int MAX_HEIGHT = GRID_SIZE * TRACKER_BOX_SIZE;
static constexpr int ID_GWM_RESIZE = 2;
static constexpr int MAX_ITEMS = 58; // do not make this bigger or will break gump desktop saving

static std::vector<TrackedItem> s_Items;
static int s_Width = TRACKER_WIDTH;
static int s_Height = TRACKER_HEIGHT;

// change-flash state per tracked item (parallel to s_Items): the count text
// flashes ice-light on gain / fire-light on loss for 5 seconds
static std::vector<int> s_LastCounts;
static std::vector<uint32_t> s_ChangeTicks;
static std::vector<int8_t> s_ChangeDirection; // 1 up, -1 down
static constexpr uint32_t FLASH_DURATION = 5000;
static constexpr uint16_t FLASH_UP_HUE = 1165;   // icelight
static constexpr uint16_t FLASH_DOWN_HUE = 1166; // firelight

struct ItemOffset
{
    Reagent Graphic = (Reagent)0;
    int XOffset = 0;
};

// clang-format off
static ItemOffset itemOffset[] = {
    { Ginseng,      -8 },
    { MandrakeRoot, -5 },
    { PigIron,      -5 },
    { DemonBlood,   -7 },
    { GraveDust,    -10 },
    { Batwing,      -10 },
    { BlackPearl,   -10 },
    { SulfurousAsh, -3 },
    { },
};
// clang-format on

static int getXOffset(uint16_t graphic)
{
    for (const auto &entry : itemOffset)
    {
        if (graphic == entry.Graphic)
            return entry.XOffset;
    }
    return 0;
}

void ResourceTracker_SetStaticSize(int w, int h)
{
    s_Items.clear();
    s_Width = w;
    s_Height = h;
}

void ResourceTracker_AddStaticItem(uint16_t graphic, uint16_t color)
{
    s_Items.push_back({ graphic, color });
    g_GumpManager.UpdateContent(0, 0, GT_RESOURCETRACKER);
}

void ResourceTracker_SaveStaticContents(TextFileWriter &writer)
{
    writer.WriteRaw("[resource-tracker]");
    writer.WriteRaw("Size=" + str_from(s_Width) + "," + str_from(s_Height));
    for (const auto &item : s_Items)
    {
        writer.WriteRaw(
            "TrackerItem=" + str_from((int)item.Graphic) + "," + str_from((int)item.Color));
    }

    s_Width = TRACKER_WIDTH;
    s_Height = TRACKER_HEIGHT;
    s_Items.clear();
}

CGumpResourceTracker::CGumpResourceTracker(int x, int y, int w, int h)
    : CGump(GT_RESOURCETRACKER, 0, x, y)
{
    if (h != 0)
    {
        s_Height = h;
    }
    if (w != 0)
    {
        s_Width = w;
    }
    m_Background = (CGUIResizepic *)Add(new CGUIResizepic(0, 0x0A3C, 0, 0, s_Width, s_Height));
    m_Trans = (CGUIChecktrans *)Add(new CGUIChecktrans(5, 5, s_Width - 5, s_Height - 5));
    m_DataBox = (CGUIDataBox *)Add(new CGUIDataBox());
    Rows = s_Height / TRACKER_BOX_SIZE;
    Cols = s_Width / TRACKER_BOX_SIZE;
    m_Resizer = (CGUIResizeButton *)Add(
        new CGUIResizeButton(ID_GWM_RESIZE, 0x0837, 0x0838, 0x0838, s_Width - 5, s_Height - 5));
}

void CGumpResourceTracker::PrepareContent()
{
    CPoint2Di offset = g_MouseManager.RealPosition;
    offset.X -= GetX();
    offset.Y -= GetY();
    CurrCol = offset.X / TRACKER_BOX_SIZE;
    CurrRow = offset.Y / TRACKER_BOX_SIZE;

    if (CurrCol > Cols - 1)
    {
        CurrCol = -1;
    }
    if (CurrRow > Rows - 1)
    {
        CurrRow = -1;
    }

    if (g_SelectedObject.Gump == this && g_ObjectInHand.Enabled && offset.X > 0 && offset.Y > 0)
    {
        HasItemInGump = true;
        DrawDebug = true;
        WantUpdateContent = true;
    }
    else
    {
        DrawDebug = false;
    }

    // while reordering the overlays follow the mouse: rebuild so the
    // picked/target highlights track the hovered cell
    if (m_ReorderIndex != -1)
    {
        WantUpdateContent = true;
    }
}

void CGumpResourceTracker::UpdateContent()
{
    m_DataBox->Clear();
    UpdateItems();
    UpdateCounters();
    UpdateReorderOverlays();
}

// reorder visual feedback: the picked cell glows yellow, the hovered cell
// glows cyan when it is a valid drop target
void CGumpResourceTracker::UpdateReorderOverlays()
{
    if (m_ReorderIndex == -1)
    {
        return;
    }

    const int pickedCol = m_ReorderIndex % Cols;
    const int pickedRow = m_ReorderIndex / Cols;
    m_DataBox->Add(new CGUIColoredPolygone(
        0,
        0,
        5 + pickedCol * TRACKER_BOX_SIZE,
        5 + pickedRow * TRACKER_BOX_SIZE,
        TRACKER_BOX_SIZE - 5,
        TRACKER_BOX_SIZE - 5,
        0x5000FFFF)); // yellow

    const int target = CurrRow * Cols + CurrCol;
    if (target != m_ReorderIndex && CurrCol != -1 && CurrRow != -1 && target >= 0 &&
        target < (int)s_Items.size())
    {
        const int targetCol = target % Cols;
        const int targetRow = target / Cols;
        m_DataBox->Add(new CGUIColoredPolygone(
            0,
            0,
            5 + targetCol * TRACKER_BOX_SIZE,
            5 + targetRow * TRACKER_BOX_SIZE,
            TRACKER_BOX_SIZE - 5,
            TRACKER_BOX_SIZE - 5,
            0x5000FF00)); // cyan
    }
}

void CGumpResourceTracker::Draw()
{
    ScopedPerfMarker(__FUNCTION__);
    char dbf[150] = { 0 };
    if (g_DeveloperMode == DM_DEBUGGING && DrawDebug)
    {
        sprintf(
            dbf, //column row
            "Curent selection :\nColumn=%i Row=%i\nObject in hand=0x%04X",
            CurrCol,
            CurrRow,
            g_ObjectInHand.Graphic);
        g_FontManager.DrawA(3, dbf, 0x35, GetX(), GetY() - 70);
    }
    CGump::Draw();

    // reorder feedback: the picked counter's tile follows the mouse and a
    // hint explains the pending placement
    if (m_ReorderIndex != -1 && m_ReorderIndex < (int)s_Items.size())
    {
        const auto &item = s_Items[m_ReorderIndex];
        bool doubleDraw = false;
        const auto stackedGraphic = get_stack_graphic(item.Graphic, 999, doubleDraw);
        g_Game.DrawStaticArt(
            stackedGraphic, item.Color, g_MouseManager.Position.X, g_MouseManager.Position.Y);
        g_FontManager.DrawA(
            3, "Moving counter - click a slot to place it", 0x35, GetX(), GetY() - 20);
    }
}

void CGumpResourceTracker::UpdateCounters()
{
    // sync the flash tracking vectors with the item list
    s_LastCounts.resize(s_Items.size(), -1);
    s_ChangeTicks.resize(s_Items.size(), 0);
    s_ChangeDirection.resize(s_Items.size(), 0);

    bool flashActive = false;
    for (uint8_t row = 0; row < Rows; row++)
    {
        for (uint8_t col = 0; col < Cols; col++)
        {
            const auto index = row * Cols + col;
            if (index >= s_Items.size())
                continue;

            const auto item = s_Items[index];
            const auto count = CountItemBackPack(item.Graphic);

            uint16_t color = 0x44; // green
            if (count <= 20 && count > 10)
            {
                color = 0x99; // yellow
            }
            else if (count <= 10)
            {
                color = 0x26; // red
            }

            // change flash: ice-light on gain / fire-light on loss for 5s
            if (index < (int)s_LastCounts.size() && s_LastCounts[index] != -1 &&
                s_LastCounts[index] != count && s_ChangeTicks[index] == 0)
            {
                s_ChangeTicks[index] = g_Ticks;
                s_ChangeDirection[index] = (count > s_LastCounts[index]) ? 1 : -1;
            }
            s_LastCounts[index] = count;

            if (s_ChangeTicks[index] != 0)
            {
                if (g_Ticks - s_ChangeTicks[index] < FLASH_DURATION)
                {
                    color = (s_ChangeDirection[index] > 0) ? FLASH_UP_HUE : FLASH_DOWN_HUE;
                    flashActive = true;
                }
                else
                {
                    s_ChangeTicks[index] = 0;
                    s_ChangeDirection[index] = 0;
                }
            }

            CGUIText *txt = new CGUIText(
                color,
                (col * TRACKER_BOX_SIZE) + 5,
                (row * TRACKER_BOX_SIZE) + TRACKER_BOX_SIZE - 15);
            txt->CreateTextureA(9, FormatCount(count), TRACKER_BOX_SIZE, TS_CENTER);
            m_DataBox->Add(txt);
            WantRedraw = true;
        }
    }

    // keep rebuilding while a flash is running so it can expire cleanly
    if (flashActive)
    {
        WantUpdateContent = true;
    }
}

void CGumpResourceTracker::UpdateItems()
{
    for (uint8_t row = 0; row < Rows; row++)
    {
        for (uint8_t col = 0; col < Cols; col++)
        {
            const auto index = row * Cols + col;
            if (index >= s_Items.size())
                continue;

            const auto item = s_Items[index];
            bool doubleDraw = false;
            const auto stackedGraphic = get_stack_graphic(item.Graphic, 999, doubleDraw);
            const auto &sio = g_Index.m_Static[stackedGraphic];
            auto *spr = (CSprite *)sio.UserData;
            if (spr != nullptr)
            {
                const auto doubleOffset = doubleDraw ? -5 : 0;
                auto tileOffsetX = ((TRACKER_BOX_SIZE / 2) - (spr->ImageWidth / 2)) - 5;
                const auto tileOffsetY = (TRACKER_BOX_SIZE - (spr->ImageHeight)) / 2;
                tileOffsetX += getXOffset(item.Graphic);
                const auto posX = (col * TRACKER_BOX_SIZE) + tileOffsetX + doubleOffset;
                const auto posY = (row * TRACKER_BOX_SIZE) + tileOffsetY + doubleOffset;
                m_DataBox->Add(new CGUIShader(&g_ColorizerShader, true));
                m_DataBox->Add(new CGUITilepic(stackedGraphic, item.Color, posX, posY, doubleDraw));
                m_DataBox->Add(new CGUIShader(&g_ColorizerShader, false));
                WantRedraw = true;
            }
        }
    }
}

void CGumpResourceTracker::UpdateGrid()
{
    const auto color = 0x900;
    for (uint8_t col = 0; col < Cols; col++)
    {
        const auto c = (col + 1) * TRACKER_BOX_SIZE;
        m_DataBox->Add(new CGUILine(c, 5, c, TRACKER_BOX_SIZE * Rows, color));
        for (uint8_t row = 0; row < Rows; row++)
        {
            const auto r = (row + 1) * TRACKER_BOX_SIZE;
            m_DataBox->Add(new CGUILine(0, r, TRACKER_BOX_SIZE * Cols, r, color));
        }
    }
}

astr_t CGumpResourceTracker::FormatCount(int count) const
{
    if (g_ConfigManager.ResourceTrackerAbbreviatedAmount)
    {
        // abbreviated amounts: 1500 -> 1k, 12000 -> 12k, 2500000 -> 2m
        char buf[32] = { 0 };
        if (abs(count) >= 1000000)
        {
            snprintf(buf, sizeof(buf), "%dm", count / 1000000);
        }
        else if (abs(count) >= 1000)
        {
            snprintf(buf, sizeof(buf), "%dk", count / 1000);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%d", count);
        }
        return astr_t(buf);
    }
    return str_from(count);
}

int CGumpResourceTracker::CountItemBackPack(uint16_t graphic)
{
    int result = 0;
    CGameItem *bk = (CGameItem *)g_Player->FindLayer(OL_BACKPACK);
    if (bk == nullptr)
    {
        return result;
    }
    for (int i = 0; i < bk->GetItemsCount(); i++)
    {
        CGameItem *item = (CGameItem *)bk->Get(i);
        if (item->IsContainer())
        {
            for (int j = 0; j < item->GetItemsCount(); j++)
            {
                CGameItem *subitem = (CGameItem *)item->Get(j);
                if (subitem->Graphic == graphic)
                {
                    result += subitem->Count;
                }
            }
        }
        else if (item->Graphic == graphic)
        {
            result += item->Count;
        }
    }
    return result;
}

void CGumpResourceTracker::UpdateSize()
{
    s_Width = StartResizeWidth + g_MouseManager.LeftDroppedOffset().X;
    s_Height = StartResizeHeight + g_MouseManager.LeftDroppedOffset().Y;
    s_Height = std::max(TRACKER_BOX_SIZE, std::min(MAX_HEIGHT, s_Height));
    s_Width = std::max(TRACKER_BOX_SIZE, std::min(MAX_WIDTH, s_Width));
    Rows = s_Height / TRACKER_BOX_SIZE;
    Cols = s_Width / TRACKER_BOX_SIZE;
    m_Background->Width = s_Width + 5;
    m_Background->Height = s_Height + 5;
    m_Trans->Width = s_Width - 10;
    m_Trans->Height = s_Height - 10;
    m_Resizer->SetX(s_Width);
    m_Resizer->SetY(s_Height);
    WantRedraw = true;
    WantUpdateContent = true;
}

void CGumpResourceTracker::OnLeftMouseButtonUp()
{
    CGump::OnLeftMouseButtonUp();

    // reorder pass: click a filled cell to pick its counter up, then click
    // another cell to move it there (click the same cell to cancel)
    if (!g_ObjectInHand.Enabled && m_ReorderIndex != -1)
    {
        const int target = CurrRow * Cols + CurrCol;
        if (target != m_ReorderIndex && CurrCol != -1 && CurrRow != -1 && target >= 0 &&
            target < (int)s_Items.size())
        {
            const TrackedItem moved = s_Items[m_ReorderIndex];
            s_Items.erase(s_Items.begin() + m_ReorderIndex);
            s_Items.insert(s_Items.begin() + std::min<size_t>(target, s_Items.size()), moved);
            WantUpdateContent = true;
        }
        m_ReorderIndex = -1;
        // release the reorder overlays: without this kick the last built
        // frame keeps showing the yellow highlight after the drop
        WantUpdateContent = true;
        return;
    }

    if (!HasItemInGump || s_Items.size() >= MAX_ITEMS)
    {
        // no item in hand: start a reorder when the pressed cell is filled
        const int index = CurrRow * Cols + CurrCol;
        if (index >= 0 && index < (int)s_Items.size() && g_PressedObject.LeftGump == this)
        {
            m_ReorderIndex = index;
        }
        else if (m_ReorderIndex != -1)
        {
            // clicked while holding but not on a grid cell: cancel the pickup
            m_ReorderIndex = -1;
            WantUpdateContent = true;
        }
        return;
    }

    auto *const container = g_World->FindWorldObject(g_ObjectInHand.Container);
    if (container == nullptr)
    {
        return;
    }
    const auto color = g_ObjectInHand.Color;
    const auto graphic = g_ObjectInHand.Graphic;
    g_Game.DropItem(container->Serial, 0xFFFF, 0xFFFF, 0);

    const auto index = CurrRow * Cols + CurrCol;
    if (!IsStackable(graphic))
    {
        return;
    }

    const TrackedItem item = { graphic, color };
    const auto cond = [&item](auto x)
    { return x.Graphic == item.Graphic && x.Color == item.Color; };
    const auto it = std::find_if(s_Items.begin(), s_Items.end(), cond);
    if (it != s_Items.end())
    {
        return;
    }

    if (index >= s_Items.size())
    {
        s_Items.push_back(item);
    }
    else
    {
        s_Items.insert(s_Items.begin() + index, item);
    }

    WantUpdateContent = true;
    HasItemInGump = false;
}

// right click on a tracked counter removes it; returns whether the click was
// consumed (false lets the gump manager close the tracker)
bool CGumpResourceTracker::RightClickErase()
{
    const int index = CurrRow * Cols + CurrCol;
    if (index < 0 || index >= (int)s_Items.size())
    {
        return false;
    }

    s_Items.erase(s_Items.begin() + index);
    WantUpdateContent = true;
    return true;
}

void CGumpResourceTracker::GUMP_RESIZE_START_EVENT_C
{
    StartResizeWidth = s_Width;
    StartResizeHeight = s_Height;
}

void CGumpResourceTracker::GUMP_RESIZE_EVENT_C
{
    CPoint2Di offset = g_MouseManager.LeftDroppedOffset();
    s_Width = StartResizeWidth + offset.X;
    s_Height = StartResizeHeight + offset.Y;
    UpdateSize();
}

void CGumpResourceTracker::GUMP_RESIZE_END_EVENT_C
{
    StartResizeWidth = 0;
    StartResizeHeight = 0;
}
