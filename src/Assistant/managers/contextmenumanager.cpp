// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/contextmenumanager.cpp).

#include "contextmenumanager.h"
#include "../core/assist.h"
#include "../macros/macroexecutor.h"
#include "Network/Packets.h" // ::CPacketPopupMenuSelection (the client's own 0xBF/0x15 builder)
#include <common/logging/logging.h>

namespace xuoassist {

CContextMenuManager g_ContextMenuManager;

CContextMenuManager::CContextMenuManager() = default;

void CContextMenuManager::SetHook(const uint32_t &id, const uint32_t &index)
{
    Info(Assistant, "CContextMenuManager::SetHook: 0x%08X %i\n", id, index);
    HookEnabled = true;
    HookID = id;
    HookIndex = index;
}

void CContextMenuManager::RemoveHook()
{
    HookEnabled = false;
}

bool CContextMenuManager::OnPacketReceived(CDataReader &reader)
{
    uint16_t mode = reader.ReadUInt16BE();
    bool isNewClilocs = (mode >= 2);
    Serial = reader.ReadUInt32BE();

    // a playing macro waiting on this context menu consumes the packet (the
    // macro replay answers the server itself); deliberately no g_World gate.
    if (g_MacroExecutor.IsPlaying() && g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_CONTEXT_MENU }))
    {
        return false;
    }

    if (HookEnabled)
    {
        if ((HookID != 0u) && Serial != HookID)
        {
            return true;
        }

        uint8_t count = reader.ReadUInt8();

        uint32_t index = HookIndex;

        for (int i = 0; i < count; i++)
        {
            if (isNewClilocs)
            {
                reader.Move(4);

                if (index == 0u)
                {
                    index = reader.ReadUInt16BE();
                    HookEnabled = false;
                    ::CPacketPopupMenuSelection(Serial, (uint16_t)index).Send();
                    return false;
                }

                index--;

                reader.Move(2);
                reader.Move(2);
            }
            else
            {
                if (index == 0u)
                {
                    index = reader.ReadUInt16BE();
                    HookEnabled = false;
                    ::CPacketPopupMenuSelection(Serial, (uint16_t)index).Send();
                    return false;
                }

                index--;

                reader.Move(2);
                uint16_t flags = reader.ReadUInt16BE();

                if ((flags & 0x84) != 0)
                {
                    reader.Move(2);
                }

                if ((flags & 0x40) != 0)
                {
                    reader.Move(2);
                }

                if ((flags & 0x20) != 0)
                {
                    reader.Move(2);
                }
            }
        }
    }

    return true;
}

bool CContextMenuManager::OnPacketSend(CDataReader &reader) const
{
    (void)reader;
    return true;
}

} // namespace xuoassist
