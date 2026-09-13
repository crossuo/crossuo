// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The shop buy/sell engine - the port of assist/tabagentsshop.cpp (hook bodies
// and ProcessShopList are line-faithful; the Qt widget globals became agent
// state) and assist/shopprogressform.cpp (the countdown becomes frame ticks).

#define LOGGER_MODULE Assistant

#include "shopagent.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>
#include <common/str.h>
#include <stdlib.h>

#include "assistantevents.h"
#include "assist.h"
#include "packets.h"
#include "../config/assistantconfig.h"
#include "../model/commonitems/finditem.h"
#include "../model/datareader.h"
#include "../model/gameobjects/gamecharacter.h"
#include "../model/gameobjects/gameitem.h"
#include "../model/gameobjects/gameworld.h"
#include "Managers/ClilocManager.h" // ::g_ClilocManager (client)

namespace xuoassist {

CShopAgent g_ShopAgent;

namespace {

// the per-file to_lower_copy the Qt port keeps in assist/displaymanager
string to_lower_copy(string text)
{
    for (char &ch : text)
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            ch += 'a' - 'A';
        }
    }
    return text;
}

// the old toInt semantics: the WHOLE string must parse as an integer
bool string_to_int(const string &text, int &value)
{
    if (text.length() == 0)
    {
        return false;
    }
    char *end = nullptr;
    const long parsed = strtol(text.c_str(), &end, 10);
    if (*end != '\0')
    {
        return false;
    }
    value = (int)parsed;
    return true;
}

string cliloc_name(const string &name, bool &intParsed)
{
    int clilocNum = 0;
    intParsed = string_to_int(name, clilocNum);

    if (intParsed)
    {
        // was g_ClilocManager.Get(clilocNum, true) (the assistant's own
        // manager): the client's cliloc parser with empty args
        return str_from(g_ClilocManager.ParseArgumentsToClilocString(clilocNum, true, wstr_from("")));
    }

    return name;
}

} // namespace


void CShopAgent::Tick()
{
    // was ShopProgressForm::on_Timer (50 ms timer -> g_Ticks frame ticks)
    if (!m_ProgressActive)
    {
        return;
    }

    if ((int)(g_Ticks - m_ProgressStart) < m_ProgressDelay)
    {
        return;
    }

    g_Assistant.ClientPrintDebugLevel1("Shopping completed!");

    m_ProgressActive = false;

    // was: m_Packet->SendServer()
    if (m_ProgressIsBuy)
    {
        CPacketBuyRequest(m_ProgressVendorSerial, m_ProgressList).SendServer();
    }
    else
    {
        CPacketSellRequest(m_ProgressVendorSerial, m_ProgressList).SendServer();
    }
}

bool CShopAgent::IsActiveShopping()
{
    // was TabAgentsShop::IsActiveShopping: waiting for a vendor reply or the
    // queued request countdown still running
    return (
        (m_State != SLWS_NORMAL && m_WaitTimeout >= g_Ticks) || m_ProgressActive);
}

void CShopAgent::Buy(const string &listName)
{
    // was on_pb_BuySelectedList_clicked: "<vendor name> buy" speech
    string text = m_VendorName;

    if (text.length() != 0)
    {
        text += " ";
    }

    text += "buy";

    SendShopRequest(SLWS_BUY, listName, text);
}

void CShopAgent::Sell(const string &listName)
{
    // was on_pb_SellSelectedList_clicked: "<vendor name> sell" speech
    string text = m_VendorName;

    if (text.length() != 0)
    {
        text += " ";
    }

    text += "sell";

    SendShopRequest(SLWS_SELL, listName, text);
}

void CShopAgent::SendShopRequest(int state, const string &listName, const string &text)
{
    // was TabAgentsShop::SendShopRequest (the buttons always passed shopDelay
    // 0: the per-stack delay comes from the config when the request is built)
    if (g_World == nullptr || !EnabledFeatureShopAgent())
    {
        return;
    }

    if (m_State != SLWS_NORMAL && m_WaitTimeout >= g_Ticks)
    {
        g_Assistant.ClientPrintDebugLevel1("Preveous waiting for shopping cancelled.");
    }

    m_WaitShopDelay = 0;
    m_WaitShopListName = listName;
    m_State = state;
    m_WaitTimeout = g_Ticks + (uint)(g_AssistantConfig.m_ShopTimeout * 1000);
    CPacketUnicodeSpeechRequest(0, wstr_from(text)).SendClient();
}

void CShopAgent::CancelProgress()
{
    // was ShopProgressForm::on_pb_Cancel_clicked
    g_Assistant.ClientPrintDebugLevel1("Shopping cancelled by user");
    m_ProgressActive = false;
    m_ProgressList.clear();
}

int CShopAgent::ProgressValue() const
{
    if (!m_ProgressActive)
    {
        return 0;
    }

    // was ui->pb_Progress->setValue(m_Delay - elapsed): ms remaining
    const int elapsed = (int)(g_Ticks - m_ProgressStart);
    return (elapsed >= m_ProgressDelay) ? 0 : (m_ProgressDelay - elapsed);
}

void CShopAgent::Reset()
{
    m_State = SLWS_NORMAL;
    m_WaitTimeout = 0;
    m_ProgressActive = false;
    m_ProgressList.clear();
    m_VendorItems.clear();
    m_VendorSerial = 0;
}

bool CShopAgent::ProcessShopList()
{
    // was TabAgentsShop::ProcessShopList (line-faithful port)
    vector<CVendorShopItem> &vendorItems = m_VendorItems;

    if (vendorItems.empty())
    {
        return true;
    }

    vector<CShopListEntry> &shopLists = g_AssistantConfig.m_ShopLists;

    CShopListEntry *shopList = nullptr;
    string seekListName = to_lower_copy(m_WaitShopListName);

    for (size_t i = 0; i < shopLists.size(); i++)
    {
        shopList = &shopLists[i];

        if (to_lower_copy(shopList->Name) == seekListName)
        {
            break;
        }
    }

    if (shopList == nullptr)
    {
        return true;
    }

    vector<CShopItemEntry> &shopItems = shopList->m_Items;

    if (shopItems.empty())
    {
        return true;
    }

    bool isBuyList = m_IsBuyList;
    int maxStacks = g_AssistantConfig.m_MaxShopItems;
    int maxSellPrice = g_AssistantConfig.m_MaxSellPrice;

    if (isBuyList)
    {
        maxSellPrice = 0;
    }

    int totalPrice = 0;

    vector<std::pair<uint, int>> result;

    for (size_t si = 0; si < shopItems.size(); si++)
    {
        const CShopItemEntry &shopItem = shopItems[si];
        CFindItem findItem(shopItem.Graphic, shopItem.Color);
        int checkPrice = shopItem.Price;
        string checkName = to_lower_copy(shopItem.Name);
        bool exit = false;

        for (size_t vi = 0; vi < vendorItems.size(); vi++)
        {
            CVendorShopItem &vendorItem = vendorItems[vi];
            int price = (int)vendorItem.GetPrice();

            if (checkPrice != 0)
            {
                if (isBuyList)
                {
                    // buy: the list price is the maximum acceptable price
                    if (checkPrice < price)
                    {
                        continue;
                    }
                }
                else if (checkPrice > price)
                {
                    // sell: the list price is the minimum acceptable price
                    continue;
                }
            }

            if (findItem.Found(vendorItem.GetGraphic(), vendorItem.GetColor()) &&
                ((checkName.length() == 0) ||
                 checkName == to_lower_copy(vendorItem.GetName())))
            {
                int count = shopItem.Count;

                if ((count == 0) || count > (int)vendorItem.GetCount())
                {
                    count = (int)vendorItem.GetCount();
                }

                int currentPrice = count * price;

                if (maxSellPrice != 0)
                {
                    if (totalPrice + currentPrice >= maxSellPrice)
                    {
                        count = (maxSellPrice - totalPrice) / price;

                        exit = (count != 0);
                    }
                }

                if (count != 0)
                {
                    totalPrice += currentPrice;
                    result.push_back(std::pair<uint, int>(vendorItem.GetSerial(), count));

                    if ((maxStacks != 0) && (int)result.size() >= maxStacks)
                    {
                        exit = true;
                    }

                    if (exit)
                    {
                        break;
                    }
                }
            }
        }

        if (exit)
        {
            break;
        }
    }

    m_State = SLWS_NORMAL;

    if (result.empty())
    {
        if (isBuyList)
        {
            g_Assistant.ClientPrintDebugLevel1("No items to bought. Waiting cancelled.");
        }
        else
        {
            g_Assistant.ClientPrintDebugLevel1("No items to sold. Waiting cancelled.");
        }

        return true;
    }

    // was: new CPacketBuyRequest/SellRequest + ShopProgressForm::Start; the
    // packet is built from the stored pair when the countdown expires
    m_ProgressIsBuy = isBuyList;
    m_ProgressVendorSerial = m_VendorSerial;
    m_ProgressList = result;

    int delay = m_WaitShopDelay;

    if (delay == 0)
    {
        delay = g_AssistantConfig.m_ShopDelay * (int)result.size();
    }

    m_ProgressDelay = delay;
    m_ProgressStart = g_Ticks;
    m_ProgressActive = true;
    m_ProgressShow = g_AssistantConfig.m_ShowShoppingProgressBar;

    return false;
}

bool CShopAgent::OnOpenContainer(uint serial)
{
    // was TabAgentsShop::OnPacketReceivedOpenContaier (line-faithful)
    if (g_World == nullptr || !EnabledFeatureShopAgent())
    {
        return true;
    }

    m_ShopListReplayed = false;
    m_IsBuyList = true;
    m_VendorItems.clear();

    bool result = true;

    if (serial == m_VendorSerial)
    {
        CGameCharacter *vendor = g_World->FindWorldCharacter(serial);

        if (vendor != nullptr)
        {
            for (int i = 0; i < 2; i++)
            {
                CGameItem *container = vendor->FindLayer(i + OL_BUY_RESTOCK);

                if (container == nullptr)
                {
                    continue;
                }

                QFOR(obj, container->m_Items, CGameItem *)
                {
                    CVendorShopItem item;
                    item.SetSerial(obj->GetSerial());
                    item.SetGraphic(obj->GetGraphic());
                    item.SetColor(obj->GetColor());
                    item.SetCount(obj->GetCount());
                    item.SetPrice(obj->GetPrice());
                    item.SetName(obj->GetName());
                    item.SetNameFromCliloc(obj->GetNameFromCliloc());

                    m_VendorItems.push_back(item);
                }
            }

            if (m_AutoBuy || (m_State == SLWS_BUY && m_WaitTimeout >= g_Ticks))
            {
                result = ProcessShopList();
            }
        }
        else
        {
            Error(Assistant, "shop agent: buy vendor is not found");
        }
    }
    else
    {
        // also covers a stale serial from a previous vendor (serial reuse):
        // the snapshot stays empty, no shopping happens
        Error(Assistant,
            "shop agent: buy vendor serial (0x%08X) is incorrent! Wanted serial: 0x%08X",
            serial,
            m_VendorSerial);
    }

    return result;
}

bool CShopAgent::OnBuyList(CDataReader &reader)
{
    // was TabAgentsShop::OnPacketReceivedBuyList (line-faithful; the price
    // list parse below is the whole handler in this pipeline)
    if (g_World == nullptr || !EnabledFeatureShopAgent())
    {
        return true;
    }

    uint serial = reader.ReadUInt32BE();

    CGameItem *container = g_World->FindWorldItem(serial);

    if (container == nullptr)
    {
        Error(Assistant, "shop agent: buy container is not found");
        return true;
    }

    uint vendorSerial = container->GetContainer();

    m_VendorSerial = vendorSerial;

    CGameCharacter *vendor = g_World->FindWorldCharacter(vendorSerial);

    if (vendor == nullptr)
    {
        Error(Assistant, "shop agent: buy vendor is not found");
        return true;
    }

    if (container->GetLayer() == OL_BUY_RESTOCK || container->GetLayer() == OL_BUY)
    {
        uchar count = reader.ReadUInt8();

        CGameItem *item = (CGameItem *)container->m_Items;
        //oldp spams this packet twice in a row on NPC verndors. nullptr check is needed t
        if (item == nullptr)
        {
            return true;
        }

        bool reverse = (item->GetX() > 1);

        if (reverse)
        {
            item = (CGameItem *)container->Last();
        }

        for (int i = 0; i < count; i++)
        {
            if (item == nullptr)
            {
                Error(Assistant, "shop agent: buy item is not found");
                break;
            }

            item->SetPrice(reader.ReadUInt32BE());

            uchar nameLen = reader.ReadUInt8();
            string name = reader.ReadString(nameLen);

            bool intParsed = false;
            name = cliloc_name(name, intParsed);

            item->SetName(name);

            item->SetNameFromCliloc(false);

            if (reverse)
            {
                item = (CGameItem *)item->m_Prev;
            }
            else
            {
                item = (CGameItem *)item->m_Next;
            }
        }
    }
    else
    {
        Warning(Assistant, "shop agent: unknown layer for buy container");
    }

    return true;
}

bool CShopAgent::OnBuyReply(CDataReader &reader)
{
    (void)reader;
    //uint serial = reader.ReadUInt32BE();
    //uchar flag = reader.ReadUInt8();

    return true;
}

bool CShopAgent::OnSellList(CDataReader &reader)
{
    // was TabAgentsShop::OnPacketReceivedSellList (line-faithful)
    if (g_World == nullptr || !EnabledFeatureShopAgent())
    {
        return true;
    }

    m_ShopListReplayed = false;
    m_IsBuyList = false;
    m_VendorItems.clear();

    uint serial = reader.ReadUInt32BE();

    CGameCharacter *vendor = g_World->FindWorldCharacter(serial);

    m_VendorSerial = serial;

    bool result = true;

    if (vendor != nullptr)
    {
        ushort itemsCount = reader.ReadUInt16BE();

        if (itemsCount == 0u)
        {
            Warning(Assistant, "shop agent: sell list is empty");
        }

        for (int i = 0; i < itemsCount; i++)
        {
            uint itemSerial = reader.ReadUInt32BE();
            ushort graphic = reader.ReadUInt16BE();
            ushort color = reader.ReadUInt16BE();
            ushort count = reader.ReadUInt16BE();
            ushort price = reader.ReadUInt16BE();
            int nameLen = reader.ReadInt16BE();
            string name = reader.ReadString(nameLen);

            bool intParsed = false;
            name = cliloc_name(name, intParsed);

            CVendorShopItem item;
            item.SetSerial(itemSerial);
            item.SetGraphic(graphic);
            item.SetColor(color);
            item.SetCount(count);
            item.SetPrice(price);
            item.SetName(name);
            item.SetNameFromCliloc(intParsed);

            m_VendorItems.push_back(item);
        }

        if (m_AutoSell || (m_State == SLWS_SELL && m_WaitTimeout >= g_Ticks))
        {
            result = ProcessShopList();
        }
    }
    else
    {
        Error(Assistant, "shop agent: sell vendor is not found");
    }

    return result;
}

void CShopAgent::OnBuyReplySent()
{
    // was TabAgentsShop::OnPacketSendBuyReply
    if (EnabledFeatureShopAgent())
    {
        m_ShopListReplayed = true;
    }
}

void CShopAgent::OnSellReplySent()
{
    // was TabAgentsShop::OnPacketSendSellReply
    if (EnabledFeatureShopAgent())
    {
        m_ShopListReplayed = true;
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
