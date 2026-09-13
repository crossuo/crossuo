// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The dress agent engine - see dressagent.h for the port notes. The step
// order below is lifted 1:1 from the script builder TabAgentsDress::DressSet /
// UndressSet / DisarmSet; "Client.Unequip(layer)" resolves to the assistant
// MoveItem into the backpack and "Client.Equip(serial)" to
// g_Assistant.MoveEquip (OnCommandEquip).
//
// Port delta (documented): Tick() paces ONE step per frame - with
// MoveItemsDelay == 0 the whole sequence still spreads over one step per
// frame (m_NextTick lands on the current tick), while the original engine ran the
// entire script in a single pass when the delay was 0.

#define LOGGER_MODULE Assistant

#include "dressagent.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include "assist.h"
#include "../config/assistantconfig.h"
#include "../model/gameobjects/gameitem.h"
#include "../model/gameobjects/gameplayer.h"
#include "../model/gameobjects/gameworld.h"

namespace xuoassist {

CDressAgent g_DressAgent;

void CDressAgent::Start(vector<XA_DRESS_STEP> &steps)
{
    // was TerminateScript(g_EquipScriptName) + RunScript(...): a new request
    // replaces whatever sequence was running
    m_Steps.swap(steps);
    m_Next = 0;
    m_NextTick = 0;
    m_Active = !m_Steps.empty();
}

void CDressAgent::Abort()
{
    m_Steps.clear();
    m_Next = 0;
    m_Active = false;
}

void CDressAgent::DressSet(const CDressSetEntry *set)
{
    if (set == nullptr || g_Player == nullptr || g_World == nullptr)
    {
        return;
    }

    vector<XA_DRESS_STEP> steps;
    const int moveItemsDelay = g_AssistantConfig.MoveItemsDelay();
    bool ignoreLayer1_2 = false;

    if (g_AssistantConfig.UndressBeforeDressing())
    {
        for (int layer = 1; layer < 3; layer++)
        {
            uint serial = set->m_Items[layer];

            if (serial != 0)
            {
                CGameItem *obj = g_Player->FindLayer(layer);

                if (obj != nullptr && obj->GetSerial() != serial)
                {
                    ignoreLayer1_2 = true;

                    XA_DRESS_STEP step;
                    step.Op = XA_DRESS_OP_UNEQUIP;
                    step.Layer = layer;
                    steps.push_back(step);
                }
            }
        }
    }

    for (int layer = 1; layer < 25; layer++)
    {
        uint serial = set->m_Items[layer];

        if (serial != 0 && g_LayerSafe[layer])
        {
            if (g_World->FindWorldItem(serial) != nullptr)
            {
                CGameItem *obj = g_Player->FindLayer(layer);
                bool equipped = false;

                if (obj != nullptr)
                {
                    equipped = (obj->GetSerial() == serial);

                    if (g_AssistantConfig.UndressBeforeDressing() && !equipped)
                    {
                        if (layer > 2 || !ignoreLayer1_2)
                        {
                            XA_DRESS_STEP step;
                            step.Op = XA_DRESS_OP_UNEQUIP;
                            step.Layer = layer;
                            steps.push_back(step);
                        }
                    }
                }

                if (!equipped)
                {
                    XA_DRESS_STEP step;
                    step.Op = g_AssistantConfig.DoubleClickForEquipItems() ? XA_DRESS_OP_USEOBJECT
                                                                           : XA_DRESS_OP_EQUIP;
                    step.Layer = layer;
                    step.Serial = serial;
                    steps.push_back(step);
                }
            }
        }
    }

    // remember the inter-step delay for Tick (the script embedded it as
    // Client.Wait after every action)
    m_Delay = moveItemsDelay;

    Start(steps);
}

void CDressAgent::Undress()
{
    if (g_Player == nullptr)
    {
        return;
    }

    vector<XA_DRESS_STEP> steps;

    for (int layer = 3; layer < 25; layer++)
    {
        if (!g_LayerSafe[layer])
        {
            continue;
        }

        CGameItem *item = g_Player->FindLayer(layer);

        if (item != nullptr)
        {
            XA_DRESS_STEP step;
            step.Op = XA_DRESS_OP_UNEQUIP;
            step.Layer = layer;
            steps.push_back(step);
        }
    }

    m_Delay = g_AssistantConfig.MoveItemsDelay();

    Start(steps); // a naked player produces no steps: a no-op (parity with the original)
}

void CDressAgent::Disarm()
{
    if (g_Player == nullptr)
    {
        return;
    }

    vector<XA_DRESS_STEP> steps;

    for (int layer = 1; layer < 3; layer++)
    {
        CGameItem *item = g_Player->FindLayer(layer);

        if (item != nullptr)
        {
            XA_DRESS_STEP step;
            step.Op = XA_DRESS_OP_UNEQUIP;
            step.Layer = layer;
            steps.push_back(step);
        }
    }

    m_Delay = g_AssistantConfig.MoveItemsDelay();

    Start(steps);
}

void CDressAgent::Tick()
{
    if (!m_Active)
    {
        return;
    }

    if (g_Player == nullptr || g_World == nullptr)
    {
        Abort();
        return;
    }

    if (g_Ticks < m_NextTick)
    {
        return;
    }

    const XA_DRESS_STEP &step = m_Steps[m_Next];
    m_Next++;

    switch (step.Op)
    {
        case XA_DRESS_OP_UNEQUIP:
        {
            // was Client.Unequip(layer): MoveItem into the backpack
            CGameItem *item = g_Player->FindLayer(step.Layer);

            if (item != nullptr && g_LayerSafe[step.Layer])
            {
                g_MoveItemData.Reset();
                g_MoveItemData.Container = g_Backpack;
                g_Assistant.MoveItem(item);
            }

            // missing item: skip silently (the layer is already empty - the
            // undress-while-naked case is a no-op)
            break;
        }
        case XA_DRESS_OP_EQUIP:
        {
            // was Client.Equip(serial)
            g_Assistant.MoveEquip(step.Serial, step.Layer, g_PlayerSerial);
            break;
        }
        case XA_DRESS_OP_USEOBJECT:
        {
            // was Client.UseObject(serial) (cb_DoubleClickForEquipment)
            g_Assistant.DoubleClick(step.Serial);
            break;
        }
        default:
            break;
    }

    // was the "Client.Wait(MoveItemsDelay)" after each scripted action
    m_NextTick = g_Ticks + (uint)(m_Delay > 0 ? m_Delay : 0);

    if (m_Next >= m_Steps.size())
    {
        Abort();
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
