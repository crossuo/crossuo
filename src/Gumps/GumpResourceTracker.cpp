// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Jean-Martin Miljours
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include <xuocore/uodata.h>
#include <common/utils.h> // countof
#include <algorithm>      // std::min, std::max
#include "GumpResourceTracker.h"
#include "../Spells.h"
#include "../Managers/GumpManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/FontsManager.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GamePlayer.h"
#include "../Target.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../CrossUO.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

static constexpr int GRID_SIZE = 20;
static constexpr int MAX_WIDTH = GRID_SIZE * TRACKER_BOX_SIZE;
static constexpr int MAX_HEIGHT = GRID_SIZE * TRACKER_BOX_SIZE;
static constexpr int ID_GWM_RESIZE = 2;
static constexpr int MAX_ITEMS = 58; // do not make this bigger or will break gump desktop saving

static std::vector<TrackedItem> s_Items;
static int s_Width = TRACKER_WIDTH;
static int s_Height = TRACKER_HEIGHT;
static const int VERSION = 1;

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

void ResourceTracker_LoadStaticContents(CMappedFile &reader)
{
    s_Items.clear();
    s_Width = reader.ReadUInt16LE();
    s_Height = reader.ReadUInt16LE();
    const auto version = reader.ReadUInt8();
    if (version != VERSION)
    {
        return;
    }
    const uint8_t itemCount = reader.ReadUInt8();
    for (int itemIdx = 0; itemIdx < itemCount; itemIdx++)
    {
        const auto graphic = reader.ReadUInt16LE();
        const auto color = reader.ReadUInt16LE();
        s_Items.push_back({ graphic, color });
    }
    g_GumpManager.UpdateContent(0, 0, GT_RESOURCETRACKER);
}

void ResourceTracker_SaveStaticContents(Wisp::CBinaryFileWriter &writer)
{
    writer.WriteUInt16LE(s_Width);
    writer.WriteUInt16LE(s_Height);
    writer.WriteUInt8(VERSION);
    writer.WriteUInt8(uint8_t(s_Items.size()));
    for (int itemIdx = 0; itemIdx < s_Items.size(); itemIdx++)
    {
        const auto item = s_Items[itemIdx];
        writer.WriteUInt16LE(item.Graphic);
        writer.WriteUInt16LE(item.Color);
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
}

void CGumpResourceTracker::UpdateContent()
{
    m_DataBox->Clear();
    UpdateItems();
    UpdateCounters();
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
}

void CGumpResourceTracker::UpdateCounters()
{
    for (uint8_t row = 0; row < Rows; row++)
    {
        for (uint8_t col = 0; col < Cols; col++)
        {
            const auto index = row * Cols + col;
            if (index >= s_Items.size())
                continue;

            const auto item = s_Items[index];
            const auto count = CountItemBackPack(item.Graphic);
            CGUIText *txt = new CGUIText(
                0x44,
                (col * TRACKER_BOX_SIZE) + 5,
                (row * TRACKER_BOX_SIZE) + TRACKER_BOX_SIZE - 15);
            if (count <= 20 && count > 10)
            {
                txt->Color = 0x99; // yellow
            }
            else if (count <= 10)
            {
                txt->Color = 0x26; // red
            }
            else
            {
                txt->Color = 0x44; // green
            }
            txt->CreateTextureA(9, str_from(count), TRACKER_BOX_SIZE, TS_CENTER);
            m_DataBox->Add(txt);
            WantRedraw = true;
        }
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
            auto spr = (CSprite *)sio.UserData;
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
    if (!HasItemInGump || s_Items.size() >= MAX_ITEMS)
    {
        return;
    }

    const auto container = g_World->FindWorldObject(g_ObjectInHand.Container);
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
    const auto cond = [&item](auto x) {
        return x.Graphic == item.Graphic && x.Color == item.Color;
    };
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

bool CGumpResourceTracker::OnLeftMouseButtonDoubleClick()
{
    if (g_PressedObject.LeftObject == nullptr && !g_PressedObject.LeftObject->IsGUI())
    {
        return false;
    }

    const auto index = CurrRow * Cols + CurrCol;
    if (index >= s_Items.size())
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
