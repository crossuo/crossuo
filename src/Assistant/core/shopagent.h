// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The shop buy/sell agent (Phase 4a). Ports the engine that lived in the
// original tab classes (assist/tabagentsshop.cpp: ProcessShopList + the packet hooks,
// assist/shopprogressform.cpp: the delay-then-send countdown) into a
// frame-tick state machine, per the master plan §7 Phase 4 item 3: the original app
// ran it from frame ticks and a modal progress dialog; here AssistantUI_Tick drives
// CShopAgent::Tick and the progress popup is an ImGui window fed from the
// same state.
//
// Behavior parity notes:
//  - the flow is speech-driven ("<vendor name> buy" / "sell"); the reply
//    packets (OpenContainer gump 0x0030, BuyList 0x74, SellList 0x9E) are
//    tapped from the client's packet manager (src/Managers/PacketManager.cpp)
//    exactly where the original handlers called g_TabAgentsShop;
//  - the per-list "Auto" buy/sell checkboxes were UI-only in the original app
//    (never serialized into ShopList.xml) - they stay runtime state here;
//  - the progress countdown sends the queued Buy/Sell request when it
//    expires; Cancel drops the queued request (was ShopProgressForm).
//
// The vendor-side snapshot (m_VendorItems/m_VendorSerial/m_IsBuyList/
// m_ShopListReplayed) used to live in ShopListEditorForm because that window
// showed it; it is agent state here and the editor window reads it.

#pragma once

#include "core/global.h"
#include "../model/commonitems/vendorshopitem.h"

class CGameCharacter; // the client's world character (global namespace)

namespace xuoassist {

struct CDressSetEntry; // (dressagent.h keeps its own forward decl)

// was SHOP_LIST_WAIT_STATE in tabagentsshop.h (the SLWS_NORNAL typo is fixed)
enum SHOP_LIST_WAIT_STATE
{
    SLWS_NORMAL = 0,
    SLWS_BUY,
    SLWS_SELL
};

class CShopAgent
{
    // waiting for the vendor reply (SendShopRequest state)
    int m_State{ SLWS_NORMAL };
    uint m_WaitTimeout{ 0 }; // absolute g_Ticks stamp
    int m_WaitShopDelay{ 0 };
    string m_WaitShopListName;

    // vendor snapshot (was ShopListEditorForm state)
    uint m_VendorSerial{ 0 };
    vector<CVendorShopItem> m_VendorItems;
    bool m_IsBuyList{ true };
    bool m_ShopListReplayed{ false };

    // the per-list Auto checkboxes (cb_AutoBuy/SellSelectedList; UI-set)
    bool m_AutoBuy{ false };
    bool m_AutoSell{ false };

    // the "Vendor name:" field (le_VendorName; UI-set, not serialized - the
    // original app kept it in the line edit only, ShopList.xml has no attribute)
    string m_VendorName{ "" };

    // the queued request (was ShopProgressForm): countdown then send
    bool m_ProgressActive{ false };
    bool m_ProgressShow{ false };
    int m_ProgressDelay{ 0 };
    uint m_ProgressStart{ 0 };
    bool m_ProgressIsBuy{ false };
    uint m_ProgressVendorSerial{ 0 };
    vector<std::pair<uint, int>> m_ProgressList;

public:
    // the le_VendorName field (not serialized, parity with the original)
    void SetVendorName(const string &name) { m_VendorName = name; }
    const string &GetVendorName() const { return m_VendorName; }

    // was TabAgentsShop::SendShopRequest (the script shop API uses it too)
    void SendShopRequest(int state, const string &listName, const string &text);

private:
    bool ProcessShopList();

public:
    CShopAgent() {}

    // frame tick: expires the progress countdown (sends the queued request)
    void Tick();

    // was TabAgentsShop::IsActiveShopping (request wait OR queued progress)
    bool IsActiveShopping();

    // --- the Buy/Sell buttons (SendShopRequest + the speech text) -----------
    void Buy(const string &listName);
    void Sell(const string &listName);

    // --- agent-side UI switches (cb_AutoBuy/SellSelectedList) ----------------
    void SetAutoBuy(bool state) { m_AutoBuy = state; }
    void SetAutoSell(bool state) { m_AutoSell = state; }
    bool AutoBuy() const { return m_AutoBuy; }
    bool AutoSell() const { return m_AutoSell; }

    // --- the progress popup state (was ShopProgressForm) ---------------------
    bool ProgressActive() const { return m_ProgressActive; }
    bool ProgressShow() const { return m_ProgressShow; }
    int ProgressValue() const; // ms remaining (was pb_Progress value/max)
    int ProgressMax() const { return m_ProgressDelay; }
    // Cancel shopping! (drops the queued request)
    void CancelProgress();

    // --- client call-ins (were the packetmanager.cpp taps; bool = pass-through, parity with the original) -----
    bool OnOpenContainer(uint serial);
    // fed from the client's parsed 0x9E values (vendor serial, lookup result,
    // the sell entries); bool = pass-through, parity with the original
    bool OnSellList(uint serial, CGameCharacter *vendor, const vector<CVendorShopItem> &items);
    void OnBuyReplySent();
    void OnSellReplySent();

    // --- vendor snapshot for the Shop List Editor -----------------------------
    uint GetVendorSerial() const { return m_VendorSerial; }
    void SetVendorSerial(uint serial) { m_VendorSerial = serial; }
    bool GetIsBuyList() const { return m_IsBuyList; }
    void SetIsBuyList(bool state) { m_IsBuyList = state; }
    bool GetShopListReplayed() const { return m_ShopListReplayed; }
    void SetShopListReplayed(bool state) { m_ShopListReplayed = state; }
    const vector<CVendorShopItem> &GetVendorItems() const { return m_VendorItems; }

    // disarm everything (assistant shutdown)
    void Reset();
};

extern CShopAgent g_ShopAgent;

} // namespace xuoassist

