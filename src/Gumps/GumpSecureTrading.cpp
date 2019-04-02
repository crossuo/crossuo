// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpSecureTrading.h"
#include "../Config.h"
#include "../CrossUO.h"
#include "../Target.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../ClickObject.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../GameObjects/GamePlayer.h"
#include "../Network/Packets.h"

static const int ID_GST_CHECKBOX = 1;

CGumpSecureTrading::CGumpSecureTrading(uint32_t serial, int x, int y, uint32_t id, uint32_t id2)
    : CGump(GT_TRADE, serial, x, y)
    , ID2(id2)
{
    ID = id;
}

void CGumpSecureTrading::CalculateGumpState()
{
    DEBUG_TRACE_FUNCTION;
    CGump::CalculateGumpState();

    if (g_GumpPressed && g_PressedObject.LeftObject != nullptr &&
        g_PressedObject.LeftObject->IsText())
    {
        g_GumpMovingOffset.Reset();
        g_GumpTranslate.X = (float)m_X;
        g_GumpTranslate.Y = (float)m_Y;
    }

    if ((g_GumpTranslate.X != 0.0f) || (g_GumpTranslate.Y != 0.0f))
    {
        WantRedraw = true;
    }
}

void CGumpSecureTrading::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (m_MyCheck != nullptr)
    {
        if (StateMine)
        {
            if (m_MyCheck->Graphic != 0x0869)
            {
                m_MyCheck->Graphic = 0x0869;
                m_MyCheck->GraphicSelected = 0x086A;
                m_MyCheck->GraphicPressed = 0x086A;
                WantRedraw = true;
            }
        }
        else if (m_MyCheck->Graphic != 0x0867)
        {
            m_MyCheck->Graphic = 0x0867;
            m_MyCheck->GraphicSelected = 0x0868;
            m_MyCheck->GraphicPressed = 0x0868;
            WantRedraw = true;
        }
    }

    if (m_OpponentCheck != nullptr)
    {
        if (StateOpponent)
        {
            if (m_OpponentCheck->Graphic != 0x0869)
            {
                m_OpponentCheck->Graphic = 0x0869;
                WantRedraw = true;
            }
        }
        else if (m_OpponentCheck->Graphic != 0x0867)
        {
            m_OpponentCheck->Graphic = 0x0867;
            WantRedraw = true;
        }
    }

    if (m_TextRenderer.CalculatePositions(false))
    {
        WantRedraw = true;
    }
}

void CGumpSecureTrading::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    CGameObject *selobj = g_World->FindWorldObject(Serial);
    if (selobj == nullptr)
    {
        // The object to which the gump is attached disappeared
        return;
    }

    TRACE(Client, "Update GUMP Content, has items? %016lx (ID1: %08x, ID2: %08x)", (intptr_t)m_Items, ID, ID2);
    if (m_Items == nullptr)
    {
        if (g_Config.ClientVersion >= VERSION(7, 0, 45, 65))
        {
            Add(new CGUIGumppic(0x088A, 0, 0)); // New trade window gump
            if (StateMine)
            {
                m_MyCheck = (CGUIButton *)Add(
                    new CGUIButton(ID_GST_CHECKBOX, 0x0869, 0x086A, 0x086A, 37, 29));
            }
            else
            {
                m_MyCheck = (CGUIButton *)Add(
                    new CGUIButton(ID_GST_CHECKBOX, 0x0867, 0x0868, 0x0868, 37, 29));
            }

            CGUIText *text = (CGUIText *)Add(new CGUIText(0x0481, 73, 32));
            text->CreateTextureA(3, g_Player->GetName());
            if (StateOpponent)
            {
                m_OpponentCheck = (CGUIGumppic *)Add(new CGUIGumppic(0x0869, 258, 240));
            }
            else
            {
                m_OpponentCheck = (CGUIGumppic *)Add(new CGUIGumppic(0x0867, 258, 240));
            }
            const int fontWidth = 250 - g_FontManager.GetWidthA(3, Text);
            text = (CGUIText *)Add(new CGUIText(0x0481, fontWidth, 244));
            text->CreateTextureA(3, Text);
            mdbx = 40;
            mdby = 110;
            odbx = 192;
            odby = 110;
        }
        else //older client version old trade
        {
            Add(new CGUIGumppic(0x0866, 0, 0)); //Trade Gump
            if (StateMine)
            {
                m_MyCheck = (CGUIButton *)Add(
                    new CGUIButton(ID_GST_CHECKBOX, 0x0869, 0x086A, 0x086A, 52, 29));
            }
            else
            {
                m_MyCheck = (CGUIButton *)Add(
                    new CGUIButton(ID_GST_CHECKBOX, 0x0867, 0x0868, 0x0868, 52, 29));
            }

            CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, 84, 40));
            text->CreateTextureA(1, g_Player->GetName());
            if (StateOpponent)
            {
                m_OpponentCheck = (CGUIGumppic *)Add(new CGUIGumppic(0x0869, 266, 160));
            }
            else
            {
                m_OpponentCheck = (CGUIGumppic *)Add(new CGUIGumppic(0x0867, 266, 160));
            }
            const int fontWidth = 260 - g_FontManager.GetWidthA(1, Text);
            text = (CGUIText *)Add(new CGUIText(0x0386, fontWidth, 170));
            text->CreateTextureA(1, Text);
        }

        if (g_Config.ClientVersion < CV_500A)
        {
            Add(new CGUIColoredPolygone(0, 0, 45, 90, 110, 60, 0xFF000001));
            Add(new CGUIColoredPolygone(0, 0, 192, 70, 110, 60, 0xFF000001));
        }

        Add(new CGUIShader(&g_ColorizerShader, true));
        Add(new CGUIScissor(true, 0, 0, mdbx, mdby, 110, 80));
        m_MyDataBox = (CGUIDataBox *)Add(new CGUIDataBox());
        Add(new CGUIScissor(false));
        Add(new CGUIScissor(true, 0, 0, odbx, odby, 110, 80));
        m_OpponentDataBox = (CGUIDataBox *)Add(new CGUIDataBox());
        Add(new CGUIScissor(false));
        Add(new CGUIShader(&g_ColorizerShader, false));
    }
    else
    {
        m_MyDataBox->Clear();
        m_OpponentDataBox->Clear();
    }

    // Draw sending item (if available)
    CGameObject *container = g_World->FindWorldObject(ID);
    TRACE(Client, "Container1 %016lx ID1: %08x", (intptr_t)container, ID);
    if (container != nullptr && container->m_Items != nullptr)
    {
        QFOR(item, container->m_Items, CGameItem *)
        {
            TRACE(Client, " - Item %08x Name: %s", item->Serial, item->GetName().c_str());
            bool doubleDraw = false;
            const uint16_t graphic = item->GetDrawGraphic(doubleDraw);
            auto dataObject =
                (CGUITilepicHightlighted *)m_MyDataBox->Add(new CGUITilepicHightlighted(
                    item->Serial,
                    graphic,
                    item->Color & 0x3FFF,
                    0x0035,
                    mdbx + item->GetX(),
                    mdby + item->GetY(),
                    doubleDraw));
            dataObject->PartialHue = IsPartialHue(g_Game.GetStaticFlags(graphic));
            if (dataObject->GetY() >= 150)
            {
                dataObject->SetY(120);
            }
            if (dataObject->GetX() >= 155)
            {
                dataObject->SetX(125);
            }
        }
    }

    // Draw receiving item (if available)
    container = g_World->FindWorldObject(ID2);
    TRACE(Client, "Container2 %016lx ID2: %08x", (intptr_t)container, ID2);
    if (container != nullptr && container->m_Items != nullptr)
    {
        QFOR(item, container->m_Items, CGameItem *)
        {
            TRACE(Client, " - Item %08x Name: %s", item->Serial, item->GetName().c_str());
            bool doubleDraw = false;
            const uint16_t graphic = item->GetDrawGraphic(doubleDraw);
            auto dataObject =
                (CGUITilepicHightlighted *)m_OpponentDataBox->Add(new CGUITilepicHightlighted(
                    item->Serial,
                    graphic,
                    item->Color & 0x3FFF,
                    0x0035,
                    odbx + item->GetX(),
                    odby + item->GetY(),
                    doubleDraw));
            dataObject->PartialHue = IsPartialHue(g_Game.GetStaticFlags(graphic));
            if (dataObject->GetY() >= 150)
            {
                dataObject->SetY(120);
            }
            if (dataObject->GetX() >= 302)
            {
                dataObject->SetX(272);
            }
        }
    }
}

void CGumpSecureTrading::Draw()
{
    DEBUG_TRACE_FUNCTION;
    CGameObject *selobj = g_World->FindWorldObject(Serial);
    if (selobj == nullptr)
    {
        // The object to which the gump is attached disappeared
        return;
    }
    if (g_GumpPressed)
    {
        WantRedraw = true;
    }
    CGump::Draw();
    glTranslatef(g_GumpTranslate.X, g_GumpTranslate.Y, 0.0f);
    g_FontColorizerShader.Use();
    m_TextRenderer.Draw();
    UnuseShader();
    glTranslatef(-g_GumpTranslate.X, -g_GumpTranslate.Y, 0.0f);
}

CRenderObject *CGumpSecureTrading::Select()
{
    DEBUG_TRACE_FUNCTION;
    CGameObject *selobj = g_World->FindWorldObject(Serial);
    if (selobj == nullptr)
    {
        // The object to which the gump is attached disappeared
        return nullptr;
    }

    CRenderObject *selected = CGump::Select();
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position =
        CPoint2Di(oldPos.X - (int)g_GumpTranslate.X, oldPos.Y - (int)g_GumpTranslate.Y);

    m_TextRenderer.Select(this);
    g_MouseManager.Position = oldPos;
    return selected;
}

void CGumpSecureTrading::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    // Update checkbox status
    if (serial == ID_GST_CHECKBOX)
    {
        StateMine = !StateMine;
        SendTradingResponse(2);
    }
    else
    {
        if (!g_ClickObject.Enabled)
        {
            CGameObject *clickTarget = g_World->FindWorldObject(serial);
            if (clickTarget == nullptr)
            {
                return;
            }

            g_ClickObject.Init(clickTarget);
            g_ClickObject.Timer = g_Ticks + DCLICK_DELAY;
            g_ClickObject.X = g_MouseManager.Position.X - m_X;
            g_ClickObject.Y = g_MouseManager.Position.Y - m_Y;
        }
    }
}

void CGumpSecureTrading::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    CGump::OnLeftMouseButtonUp();
    if (g_ObjectInHand.Enabled)
    {
        int x = m_X;
        int y = m_Y;
        if (g_Game.PolygonePixelsInXY(x + 45, y + 70, 110, 80))
        {
            //if (GetTopObjDistance(g_Player, g_World->FindWorldObject(ID2)) <= DRAG_ITEMS_DISTANCE)
            {
                x = g_MouseManager.Position.X - x - 45;
                y = g_MouseManager.Position.Y - y - 70;

                bool doubleDraw = false;
                uint16_t graphic = g_ObjectInHand.GetDrawGraphic(doubleDraw);
                auto spr = g_Game.ExecuteStaticArt(graphic);
                if (spr != nullptr)
                {
                    x -= (spr->Width / 2);
                    y -= (spr->Height / 2);
                    if (x + spr->Width > 110)
                    {
                        x = 110 - spr->Width;
                    }
                    if (y + spr->Height > 80)
                    {
                        y = 80 - spr->Height;
                    }
                }

                if (x < 0)
                {
                    x = 0;
                }
                if (y < 0)
                {
                    y = 0;
                }

                g_Game.DropItem(ID, x, y, 0);
                g_MouseManager.CancelDoubleClick = true;
            }
            //else
            //	g_Game. PlaySoundEffect(0x0051);
        }
    }
    else if (
        g_Target.IsTargeting() && g_SelectedObject.Serial >= 0x40000000 &&
        g_World->FindWorldObject(g_SelectedObject.Serial) != nullptr)
    {
        g_Target.SendTargetObject(g_SelectedObject.Serial);
        g_MouseManager.CancelDoubleClick = true;
    }
}

void CGumpSecureTrading::SendTradingResponse(int code)
{
    DEBUG_TRACE_FUNCTION;
    // Reply the trade window
    CPacketTradeResponse(this, code).Send();
    if (code == SECURE_TRADE_CLOSE)
    {
        RemoveMark = true;
    }
}
