// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpShop.h"

enum
{
    ID_GB_NONE,

    ID_GB_BUTTON_ACCEPT,
    ID_GB_BUTTON_CLEAR,
    ID_GB_SHOP_LIST,
    ID_GB_SHOP_RESULT,

    ID_GB_COUNT,
};

CGumpShop::CGumpShop(uint32_t serial, bool isBuyGump, short x, short y)
    : CGump(GT_SHOP, serial, x, y)
    , IsBuyGump(isBuyGump)
{
    DEBUG_TRACE_FUNCTION;
    Visible = !isBuyGump;

    if (isBuyGump)
    {
        Add(new CGUIGumppic(0x0870, 0, 0));
    }
    else
    {
        Add(new CGUIGumppic(0x0872, 0, 0));
    }

    Add(new CGUIShader(&g_ColorizerShader, true));
    m_ItemList[0] =
        (CGUIHTMLGump *)Add(new CGUIHTMLGump(ID_GB_SHOP_LIST, 0, 30, 60, 215, 176, false, true));
    Add(new CGUIShader(&g_ColorizerShader, false));

    if (isBuyGump)
    {
        Add(new CGUIGumppic(0x0871, 170, 214));
    }
    else
    {
        Add(new CGUIGumppic(0x0873, 170, 214));
    }

    m_ItemList[1] =
        (CGUIHTMLGump *)Add(new CGUIHTMLGump(ID_GB_SHOP_RESULT, 0, 200, 280, 215, 92, false, true));

    Add(new CGUIHitBox(ID_GB_BUTTON_ACCEPT, 200, 406, 34, 30, true));
    Add(new CGUIHitBox(ID_GB_BUTTON_CLEAR, 372, 410, 24, 24, true));

    if (isBuyGump)
    {
        m_TotalPriceText = (CGUIText *)Add(new CGUIText(0x0386, 240, 385));
        m_TotalPriceText->CreateTextureA(9, "0");
        m_TotalGoldText = (CGUIText *)Add(new CGUIText(0x0386, 358, 385));
        m_TotalGoldText->CreateTextureA(9, std::to_string(g_Player->Gold));
    }
    else
    {
        m_TotalPriceText = (CGUIText *)Add(new CGUIText(0x0386, 358, 386));
        m_TotalPriceText->CreateTextureA(9, "0");

        m_TotalGoldText = nullptr;
    }

    m_NameText = (CGUIText *)Add(new CGUIText(0x0386, 242, 408));

    for (int i = 0; i < 2; i++)
    {
        CGUIHTMLButton *button = m_ItemList[i]->m_ButtonUp;

        if (button != nullptr)
        {
            button->Graphic = 0x0824;
            button->GraphicSelected = 0x0824;
            button->GraphicPressed = 0x0824;
            button->CheckPolygone = true;

            if (i == 0)
            {
                button->SetY(button->GetY() - 11);
            }
            else
            {
                button->SetY(button->GetY() - 16);
            }
        }

        button = m_ItemList[i]->m_ButtonDown;

        if (button != nullptr)
        {
            button->Graphic = 0x0825;
            button->GraphicSelected = 0x0825;
            button->GraphicPressed = 0x0825;
            button->CheckPolygone = true;

            if (i == 0)
            {
                button->SetY(button->GetY() - 25);
            }
            else
            {
                button->SetX(button->GetX() - 1);
                button->SetY(button->GetY() + 18);
            }
        }

        CGUIHTMLSlider *slider = m_ItemList[i]->m_Slider;

        if (slider != nullptr)
        {
            slider->Graphic = 0x001F;
            slider->GraphicSelected = 0x001F;
            slider->GraphicPressed = 0x001F;
            slider->BackgroundGraphic = 0;

            if (i == 0)
            {
                slider->SetY(slider->GetY() - 11);
                slider->Length -= 14;
            }
            else
            {
                slider->SetY(slider->GetY() - 16);
                slider->Length += 34;
            }
        }
    }
}

CGumpShop::~CGumpShop()
{
}

void CGumpShop::SendList()
{
    DEBUG_TRACE_FUNCTION;
    CGameCharacter *vendor = g_World->FindWorldCharacter(Serial);

    if (vendor == nullptr)
    {
        return;
    }

    if (IsBuyGump)
    {
        CPacketBuyRequest(this).Send();
    }
    else
    {
        CPacketSellRequest(this).Send();
    }
}

void CGumpShop::UpdateTotalPrice()
{
    DEBUG_TRACE_FUNCTION;
    if (m_TotalPriceText != nullptr)
    {
        int totalPrice = 0;

        QFOR(item, m_ItemList[1]->m_Items, CBaseGUI *)
        {
            if (item->Type == GOT_SHOPRESULT)
            {
                CGUIShopResult *shop = (CGUIShopResult *)item;

                totalPrice += shop->Price * shop->m_MinMaxButtons->Value;
            }
        }

        m_TotalPriceText->CreateTextureA(9, std::to_string(totalPrice));
    }
}

void CGumpShop::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (NoProcess && g_Player != nullptr)
    {
        string name = g_Player->GetName();
        int len = (int)name.length();

        if (m_ContinueCounter > len)
        {
            m_ContinueCounter = len;
        }

        name.resize(m_ContinueCounter);

        static int counterCount = 0;

        if (m_ContinueCounter == len)
        {
            NoProcess = false;
            SendList();
        }
        else if (counterCount == 0)
        {
            if (m_ContinueCounter < len)
            {
                m_ContinueCounter++;
            }
        }

        counterCount++;

        if (counterCount >= 3)
        {
            counterCount = 0;
        }

        m_NameText->CreateTextureA(5, name);

        WantRedraw = true;
    }
}

void CGumpShop::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GB_BUTTON_ACCEPT) //Accept
    {
        NoProcess = true;
        m_ContinueCounter = 0;
    }
    else if (serial == ID_GB_BUTTON_CLEAR)
    { //Clear
        m_ContinueCounter = 0;
    }
}

void CGumpShop::GUMP_SCROLL_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    CGUIMinMaxButtons *minmax = (CGUIMinMaxButtons *)g_PressedObject.LeftObject;

    if (minmax == nullptr)
    {
        return;
    }

    bool deleteItem = false;

    QFOR(item, m_ItemList[0]->m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_SHOPITEM && item->Serial == minmax->Serial)
        {
            ((CGUIShopItem *)item)->CreateCountText(minmax->Value);

            deleteItem = (minmax->Value < 1);

            break;
        }
    }

    if (deleteItem)
    {
        uint32_t itemSerial = minmax->Serial;
        int y = 0;

        for (CBaseGUI *item = (CBaseGUI *)m_ItemList[1]->m_Items; item != nullptr;)
        {
            CBaseGUI *next = (CBaseGUI *)item->m_Next;

            if (item->Type == GOT_SHOPRESULT)
            {
                if (item->Serial == itemSerial)
                {
                    m_ItemList[1]->Delete(item);
                }
                else
                {
                    item->SetY(y);
                    y += item->GetSize().Height;
                }
            }

            item = next;
        }
    }

    UpdateTotalPrice();
}

void CGumpShop::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    CGump::OnLeftMouseButtonUp();

    if (g_Target.IsTargeting() && !g_ObjectInHand.Enabled &&
        g_World->FindWorldObject(g_SelectedObject.Serial) != nullptr)
    {
        g_Target.SendTargetObject(g_SelectedObject.Serial);
        g_MouseManager.CancelDoubleClick = true;
    }
}

bool CGumpShop::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI())
    {
        if (((CBaseGUI *)g_PressedObject.LeftObject)->Type == GOT_SHOPITEM)
        {
            CGUIShopItem *shopItem = (CGUIShopItem *)g_PressedObject.LeftObject;
            CGUIShopResult *shopResult = nullptr;

            int posY = 0;

            QFOR(item, m_ItemList[1]->m_Items, CBaseGUI *)
            {
                if (item->Type == GOT_SHOPRESULT)
                {
                    if (item->Serial == shopItem->Serial)
                    {
                        shopResult = (CGUIShopResult *)item;
                        break;
                    }

                    posY += ((CGUIShopResult *)item)->GetSize().Height;
                }
            }

            if (shopResult != nullptr)
            {
                CGUIMinMaxButtons *minmax = shopResult->m_MinMaxButtons;

                if (minmax->Value < minmax->MaxValue)
                {
                    if (g_ShiftPressed)
                    {
                        minmax->Value = minmax->MaxValue;
                    }
                    else
                    {
                        minmax->Value += 1;
                    }

                    minmax->UpdateText();
                    shopItem->CreateCountText(minmax->Value);
                    WantRedraw = true;
                    result = true;
                    UpdateTotalPrice();
                }
            }
            else
            {
                shopResult =
                    (CGUIShopResult *)m_ItemList[1]->Add(new CGUIShopResult(shopItem, 0, posY));

                CGUIMinMaxButtons *minmax = shopResult->m_MinMaxButtons;

                if (g_ShiftPressed)
                {
                    minmax->Value = minmax->MaxValue;
                    minmax->UpdateText();
                }

                shopItem->CreateCountText(minmax->Value);
                WantRedraw = true;
                result = true;
                UpdateTotalPrice();
                m_ItemList[1]->CalculateDataSize();
            }
        }
    }

    return result;
}
