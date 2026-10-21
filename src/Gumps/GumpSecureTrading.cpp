// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

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
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"
#include "../Renderer/RenderShader.h" // g_ColorizerShader and friends
#include "../GUI/GUIHitBox.h"
#include "../GUI/GUITextEntry.h"
#include <cstdint>
#if defined(XUO_ASSISTANT)
#include <managers/trademanager.h> // xuoassist::g_TradeManager
#endif

static const int ID_GST_CHECKBOX = 1;
static const int ID_GST_ENTRY_GOLD = 2;
static const int ID_GST_ENTRY_PLATINUM = 3;

CGumpSecureTrading::CGumpSecureTrading(uint32_t serial, int x, int y, uint32_t id, uint32_t id2)
    : CGump(GT_TRADE, serial, x, y)
    , ID2(id2)
{
    ID = id;
}

void CGumpSecureTrading::CalculateGumpState()
{
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

    // Pick up ledger changes from the 0x6F gold-update sub-commands
    const uint32_t coinValue[COIN_COUNT] = { Gold, Platinum, HisGold, HisPlatinum };
    for (int i = 0; i < COIN_COUNT; i++)
    {
        if (m_CoinLabels[i] != nullptr && m_CoinValues[i] != coinValue[i])
        {
            m_CoinValues[i] = coinValue[i];
            m_CoinLabels[i]->CreateTextureA(9, str_from(uint64_t(coinValue[i])));
            WantRedraw = true;
        }
    }
}

void CGumpSecureTrading::UpdateContent()
{
    CGameObject *selobj = g_World->FindWorldObject(Serial);
    if (selobj == nullptr)
    {
        // The object to which the gump is attached disappeared
        return;
    }

    TRACE(
        Client,
        "Update GUMP Content, has items? %016lx (ID1: %08x, ID2: %08x)",
        (intptr_t)m_Items,
        ID,
        ID2);
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

            // Gold/platinum ledger readouts for both sides of the trade
            const int coinX[COIN_COUNT] = { 43, 180, 180, 180 };
            const int coinY[COIN_COUNT] = { 67, 67, 190, 210 };
            const uint32_t coinValue[COIN_COUNT] = { Gold, Platinum, HisGold, HisPlatinum };
            for (int i = 0; i < COIN_COUNT; i++)
            {
                m_CoinLabels[i] = (CGUIText *)Add(new CGUIText(0x0481, coinX[i], coinY[i]));
                m_CoinValues[i] = coinValue[i];
                m_CoinLabels[i]->CreateTextureA(9, str_from(uint64_t(coinValue[i])));
            }

            // Offer entry boxes under the opponent ledger: typing an amount
            // offers it from the player's own holdings
            const int entryIds[2] = { ID_GST_ENTRY_GOLD, ID_GST_ENTRY_PLATINUM };
            const int entryY[2] = { 190, 210 };
            for (int i = 0; i < 2; i++)
            {
                Add(new CGUIHitBox(entryIds[i], 43, entryY[i], 100, 20));
                m_CoinEntries[i] = (CGUITextEntry *)Add(new CGUITextEntry(
                    entryIds[i], 0x0481, 0x0035, 0x0035, 43, entryY[i], 100, false, 9));
                m_CoinEntries[i]->m_Entry.SetTextA("0");
            }

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
        for (CGameItem *item = (CGameItem *)container->m_Items; item != nullptr; item = (CGameItem *)item->m_Next)
        {
            TRACE(Client, " - Item %08x Name: %s", item->Serial, item->GetName().c_str());
            bool doubleDraw = false;
            const uint16_t graphic = item->GetDrawGraphic(doubleDraw);
            auto *dataObject =
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
        for (CGameItem *item = (CGameItem *)container->m_Items; item != nullptr; item = (CGameItem *)item->m_Next)
        {
            TRACE(Client, " - Item %08x Name: %s", item->Serial, item->GetName().c_str());
            bool doubleDraw = false;
            const uint16_t graphic = item->GetDrawGraphic(doubleDraw);
            auto *dataObject =
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
    ScopedPerfMarker(__FUNCTION__);

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
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList,
        SetModelViewTranslationCmd{ { g_GumpTranslate.X, g_GumpTranslate.Y, 0.0f } });
    g_FontColorizerShader.Enable();
    m_TextRenderer.Draw();
    g_FontColorizerShader.Disable();
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList,
        SetModelViewTranslationCmd{ { -g_GumpTranslate.X, -g_GumpTranslate.Y, 0.0f } });
}

CRenderObject *CGumpSecureTrading::Select()
{
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
                auto *spr = g_Game.ExecuteStaticArt(graphic);
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
    // Reply the trade window
    CPacketTradeResponse(this, code).Send();
#if defined(XUO_ASSISTANT)
    // the check state is
    // the field the builder writes for SECURE_TRADE_CHANGE
    xuoassist::g_TradeManager.OnTradeSent(code, ID, StateMine != 0);
#endif
    if (code == SECURE_TRADE_CLOSE)
    {
        RemoveMark = true;
    }
}

// 1e9 gold per platinum: the server converts between the currencies and
// validates the combined offer against the combined balance
static const uint64_t PLATINUM_VALUE = 1000000000ull;

void CGumpSecureTrading::UpdateCoinOffer(int index)
{
    if (index < 0 || index >= 2 || g_EntryPointer == nullptr ||
        g_EntryPointer != &m_CoinEntries[index]->m_Entry)
    {
        return;
    }

    const astr_t text = g_EntryPointer->GetTextA();
    uint32_t value = 0;
    if (text.empty())
    {
        g_EntryPointer->SetTextA("0");
    }
    else
    {
        // digits only; ignore anything else that may have slipped in
        for (char c : text)
        {
            if (c < '0' || c > '9')
            {
                g_EntryPointer->SetTextA("0");
                value = 0;
                break;
            }
            value = value * 10 + uint32_t(c - '0');
        }
    }

    bool changed = false;
    const uint64_t totalWealth = uint64_t(Gold) + uint64_t(Platinum) * PLATINUM_VALUE;
    if (index == 0)
    {
        const uint64_t platOffered = uint64_t(m_OfferPlatinum) * PLATINUM_VALUE;
        const uint64_t remaining = totalWealth > platOffered ? totalWealth - platOffered : 0;
        const uint32_t maxGold = remaining > 0xFFFFFFFFull ? 0xFFFFFFFFu : uint32_t(remaining);
        if (value > maxGold)
        {
            value = maxGold;
            changed = true;
        }
        if (m_OfferGold != value)
        {
            changed = true;
        }
        m_OfferGold = value;
    }
    else
    {
        const uint64_t remaining = totalWealth > m_OfferGold ? totalWealth - m_OfferGold : 0;
        const uint32_t maxPlatinum = uint32_t(remaining / PLATINUM_VALUE);
        if (value > maxPlatinum)
        {
            value = maxPlatinum;
            changed = true;
        }
        if (m_OfferPlatinum != value)
        {
            changed = true;
        }
        m_OfferPlatinum = value;
    }

    if (changed)
    {
        g_EntryPointer->SetTextA(str_from(uint64_t(value)));
    }

    CPacketTradeUpdateGold(ID, m_OfferGold, m_OfferPlatinum).Send();
}

void CGumpSecureTrading::OnTextInput(const TextEvent &ev)
{
    const auto ch = EvChar(ev);
    if (g_EntryPointer != nullptr && ch >= '0' && ch <= '9' &&
        (g_EntryPointer == &m_CoinEntries[0]->m_Entry ||
         g_EntryPointer == &m_CoinEntries[1]->m_Entry))
    {
        g_EntryPointer->Insert(ch);
        const int index = (g_EntryPointer == &m_CoinEntries[0]->m_Entry) ? 0 : 1;
        UpdateCoinOffer(index);
        WantRedraw = true;
    }
}

void CGumpSecureTrading::OnKeyDown(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    switch (key)
    {
        case KEY_RETURN:
        case KEY_RETURN2:
        case KEY_BACK:
        case KEY_DELETE:
        {
            if (g_EntryPointer != nullptr &&
                (g_EntryPointer == &m_CoinEntries[0]->m_Entry ||
                 g_EntryPointer == &m_CoinEntries[1]->m_Entry))
            {
                g_EntryPointer->OnKey(this, key);
                const int index = (g_EntryPointer == &m_CoinEntries[0]->m_Entry) ? 0 : 1;
                UpdateCoinOffer(index);
                WantRedraw = true;
            }
            break;
        }
        case KEY_HOME:
        case KEY_END:
        case KEY_LEFT:
        case KEY_RIGHT:
        {
            g_EntryPointer->OnKey(this, key);
            WantRedraw = true;
            break;
        }
        default:
            break;
    }
}
