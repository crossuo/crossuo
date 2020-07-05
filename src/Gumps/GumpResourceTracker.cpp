// AGPLv3 License
// Copyright (c) 2020 Jean-Martin Miljours
// Copyright (c) 2020 CrossUO Team

#include <xuocore/uodata.h>
#include "GumpResourceTracker.h"
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

enum
{
    ID_GMB_NONE,
    ID_GTB_TARGET,
};

CGumpResourceTracker::CGumpResourceTracker(short x, short y)
    : CGump(GT_RESOURCETRACKER, 0, x, y)
{
    m_Background = (CGUIResizepic *)Add(new CGUIResizepic(0, 0x0a3c, 0, 0, Width, Height));
    m_Trans = (CGUIChecktrans *)Add(new CGUIChecktrans(5, 5, 505, 65));
    m_gridDataBoxGUI = (CGUIDataBox *)Add(new CGUIDataBox());
    m_itemsDataBoxGui = (CGUIDataBox *)Add(new CGUIDataBox());
    numRow = Height / BOX_SIZE;
    numCol = Width / BOX_SIZE;
    DrawGrid();
    DrawItem();

    m_Resizer = (CGUIResizeButton *)Add(
        new CGUIResizeButton(ID_GWM_RESIZE, 0x0837, 0x0838, 0x0838, Width - 5, Height - 5));
}

void CGumpResourceTracker::PrepareContent()
{
    CPoint2Di offset = g_MouseManager.RealPosition;
    offset.X -= GetX();
    offset.Y -= GetY();
    currCol = offset.X / BOX_SIZE;
    currRow = offset.Y / BOX_SIZE;

    if (currCol > numCol - 1)
    {
        currCol = -1;
    }
    if (currRow > numRow - 1)
    {
        currRow = -1;
    }

    if (g_SelectedObject.Gump == this && g_ObjectInHand.Enabled && offset.X > 0 && offset.Y > 0)
    {
        hasItemInGump = true;
        drawDebug = true;
        WantUpdateContent = true;
    }
    else
    {
        drawDebug = false;
    }
}

void CGumpResourceTracker::UpdateContent()
{
    CGump::UpdateContent();
    WantRedraw = true;
}

void CGumpResourceTracker::Draw()
{
    char dbf[150] = { 0 };
    if (g_DeveloperMode == DM_DEBUGGING && drawDebug)
    {
        sprintf_s(
            dbf, //column row
            "Curent selection :\nColumn=%i Row=%i\nObject in hand=0x%04X",
            currCol,
            currRow,
            g_ObjectInHand.Graphic);
        g_FontManager.DrawA(3, dbf, 0x35, GetX(), GetY() - 70);
    }
    CGump::Draw();
}

void CGumpResourceTracker::DrawText()
{
    for (uint8_t r = 0; r < numRow; r++)
    {
        for (uint8_t c = 0; c < numCol; c++)
        {
            uint16_t graph = items[r][c].graphic;
            if (graph == 0)
            {
                continue;
            }
            int count = CountItemBackPack(graph);
            CGUIText *txt = new CGUIText(0x44, (c * BOX_SIZE) + 5, (r * BOX_SIZE) + BOX_SIZE - 15);
            if (count <= 20 && count > 10)
            {
                txt->Color = 0x99; //yellow
            }
            else if (count <= 10)
            {
                txt->Color = 0x26; //red
            }
            else
            {
                txt->Color = 0x44;
            }
            txt->CreateTextureA(9, str_from(count), BOX_SIZE, TS_CENTER);
            m_itemsDataBoxGui->Add(txt);
        }
    }
}

void CGumpResourceTracker::DrawItem()
{
    for (uint8_t r = 0; r < numRow; r++)
    {
        for (uint8_t c = 0; c < numCol; c++)
        {
            const auto &item = items[r][c];
            if (item.graphic == 0)
            {
                continue;
            }
            CIndexObjectStatic &sio = g_Index.m_Static[item.graphic];
            auto spr = (CSprite *)sio.UserData;
            if (spr != nullptr)
            {
                const int tileOffsetX = (BOX_SIZE - 18 - (spr->ImageWidth)) / 2;
                const int tileOffsetY = (BOX_SIZE - (spr->ImageHeight)) / 2;
                m_itemsDataBoxGui->Add(new CGUIShader(&g_ColorizerShader, true));
                m_itemsDataBoxGui->Add(new CGUITilepic(
                    item.graphic,
                    item.color,
                    (c * BOX_SIZE) + tileOffsetX,
                    (r * BOX_SIZE) + tileOffsetY));
                m_itemsDataBoxGui->Add(new CGUIShader(&g_ColorizerShader, false));
            }
        }
    }
}

void CGumpResourceTracker::DrawGrid()
{
    for (uint8_t c = 0; c < numCol; c++)
    {
        m_gridDataBoxGUI->Add(
            new CGUILine((c + 1) * BOX_SIZE, 5, (c + 1) * BOX_SIZE, BOX_SIZE * numRow, 0x900));
        for (uint8_t r = 0; r < numRow; r++)
        {
            m_gridDataBoxGUI->Add(
                new CGUILine(0, (r + 1) * BOX_SIZE, BOX_SIZE * numCol, (r + 1) * BOX_SIZE, 0x900));
        }
    }
}

void CGumpResourceTracker::DeleteGrid()
{
    m_gridDataBoxGUI->Clear();
}

int CGumpResourceTracker::CountItemBackPack(uint32_t graph)
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
                if (subitem->Graphic == graph)
                {
                    result += subitem->Count;
                }
            }
        }
        else if (item->Graphic == graph)
        {
            result += item->Count;
        }
    }
    return result;
}

void CGumpResourceTracker::UpdateSize()
{
    Width = m_StartResizeWidth + g_MouseManager.LeftDroppedOffset().X;
    Height = m_StartResizeHeight + g_MouseManager.LeftDroppedOffset().Y;

    if (Height < BOX_SIZE)
    {
        Height = BOX_SIZE;
    }

    if (Width < BOX_SIZE)
    {
        Width = BOX_SIZE;
    }

    if (Height >= MAX_HEIGHT)
    {
        Height = MAX_HEIGHT;
    }

    if (Width >= MAX_WIDTH)
    {
        Width = MAX_WIDTH;
    }

    numRow = Height / BOX_SIZE;
    numCol = Width / BOX_SIZE;
    m_itemsDataBoxGui->Clear();
    DeleteGrid();
    DrawGrid();
    DrawItem();
    DrawText();
    m_Background->Width = Width + 5;
    m_Background->Height = Height + 5;
    m_Trans->Width = Width - 10;
    m_Trans->Height = Height - 10;
    m_Resizer->SetX(Width);
    m_Resizer->SetY(Height);
    WantRedraw = true;
    WantUpdateContent = true;
}

void CGumpResourceTracker::OnLeftMouseButtonUp()
{
    CGump::OnLeftMouseButtonUp();
    if (!hasItemInGump)
    {
        return;
    }
    CRenderWorldObject *container = g_World->FindWorldObject(g_ObjectInHand.Container);

    if (container == nullptr)
    {
        return;
    }
    items[currRow][currCol].graphic = g_ObjectInHand.Graphic;
    items[currRow][currCol].color = g_ObjectInHand.Color;
    g_Game.DropItem(container->Serial, 0xFFFF, 0xFFFF, 0);
    DrawText();
    DrawItem();
    hasItemInGump = false;
}

void CGumpResourceTracker::OnLeftMouseButtonDown()
{
    CGump::OnLeftMouseButtonDown();
}

bool CGumpResourceTracker::OnLeftMouseButtonDoubleClick()
{
    bool result = false;

    if (g_PressedObject.LeftObject == nullptr && !g_PressedObject.LeftObject->IsGUI())
    {
        return result;
    }
    if (((CBaseGUI *)g_PressedObject.LeftObject)->Type == GOT_TILEPIC)
    {
        CGameItem *item = (CGameItem *)g_Player->FindLayer(OL_BACKPACK);
        CGameItem *find = nullptr;
        if (item != nullptr)
        {
            find = item->FindItem(g_PressedObject.LeftObject->Graphic);
        }
        if (find != nullptr)
        {
            g_Game.DoubleClick(find->Serial);
            FrameCreated = false;
            result = true;
        }
    }

    return result;
}

void CGumpResourceTracker::GUMP_BUTTON_EVENT_C
{
    switch (serial)
    {
        case ID_GTB_TARGET:
        {
            break;
        }
    }
}

void CGumpResourceTracker::GUMP_RESIZE_START_EVENT_C
{
    m_StartResizeWidth = Width;
    m_StartResizeHeight = Height;
}

void CGumpResourceTracker::GUMP_RESIZE_EVENT_C
{
    CPoint2Di offset = g_MouseManager.LeftDroppedOffset();
    Width = m_StartResizeWidth + offset.X;
    Height = m_StartResizeHeight + offset.Y;
    UpdateSize();
}

void CGumpResourceTracker::GUMP_RESIZE_END_EVENT_C
{
    m_StartResizeWidth = 0;
    m_StartResizeHeight = 0;
}
