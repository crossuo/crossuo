// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The dress agent (Phase 4a): the dress/undress/disarm sequences the original app
// generated as a throwaway script (assist/tabagentsdress.cpp DressSet/
// UndressSet/DisarmSet feeding the script engine) become a frame-tick state
// machine here - same steps, same ordering, same per-step delays:
//   - a step is Unequip(layer) [MoveItem into the backpack] or
//     Equip(serial)/UseObject(serial) [MoveEquip / double click];
//   - whenever the generated script emitted "Client.Wait(MoveItemsDelay)" after an
//     action the state machine waits MoveItemsDelay ticks before the next
//     step (delay 0 = no wait, exactly like the script without the Wait);
//   - "Undress before dressing" layers 1-2 pass (ignoreLayer1_2) is kept;
//   - a new request replaces the running one (the original app terminated the
//     g_EquipScriptName script before starting a new one).
//
// Equipping reads the CURRENT world (a vanished item is skipped at build
// time like the original builder, which only emitted steps for items still in the
// world); missing items mid-run skip silently - the server rejects an equip
// for an object it no longer considers ours, exactly as with the script.

#pragma once

#include "core/global.h"

namespace xuoassist {

struct CDressSetEntry;

enum XA_DRESS_OP
{
    XA_DRESS_OP_UNEQUIP = 0, // undress the current item on a layer
    XA_DRESS_OP_EQUIP,       // MoveEquip(serial, layer, player)
    XA_DRESS_OP_USEOBJECT    // double click the item (cb_DoubleClickForEquipment)
};

struct XA_DRESS_STEP
{
    int Op{ XA_DRESS_OP_UNEQUIP };
    int Layer{ 0 };
    uint32_t Serial{ 0 };
};

class CDressAgent
{
    vector<XA_DRESS_STEP> m_Steps;
    size_t m_Next{ 0 };
    uint32_t m_NextTick{ 0 };
    int m_Delay{ 0 }; // the inter-step wait (MoveItemsDelay at request time)
    bool m_Active{ false };

    void Start(vector<XA_DRESS_STEP> &steps);

public:
    CDressAgent() = default;

    CDressAgent(const CDressAgent &) = delete;
    CDressAgent &operator=(const CDressAgent &) = delete;

    // frame tick: executes the next step when its delay expired
    void Tick();
    bool Active() const { return m_Active; }

    // "Dress selected set" (was TabAgentsDress::DressSet)
    void DressSet(const CDressSetEntry *set);
    // "Undress" (layers 3..24, was UndressSet)
    void Undress();
    // "Apply weapon and shield" inverse - undress layers 1-2 (was DisarmSet)
    void Disarm();
    // stops a running sequence (assistant shutdown / a replacing request)
    void Abort();
};

extern CDressAgent g_DressAgent;

} // namespace xuoassist

