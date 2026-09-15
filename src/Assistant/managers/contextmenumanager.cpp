// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/contextmenumanager.cpp).

#include "contextmenumanager.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include "../macros/macroexecutor.h"
#include <common/logging/logging.h>

namespace xuoassist {

CContextMenuManager g_ContextMenuManager;

CContextMenuManager::CContextMenuManager() {}

void CContextMenuManager::SetHook(const uint &id, const uint &index)
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
    ushort mode = reader.ReadUInt16BE();
    bool isNewClilocs = (mode >= 2);
    Serial = reader.ReadUInt32BE();

    // was `g_TabMacros->GetPlaying() && g_TabMacros->PlayWaiting({
    // MT_WAIT_FOR_CONTEXT_MENU })` -> return false: a playing macro waiting
    // on this context menu consumes the packet (the PlayWaiting
    // semantics; the original had no g_World gate here - kept that way).
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

        uchar count = reader.ReadUInt8();

        uint index = HookIndex;

        for (int i = 0; i < count; i++)
        {
            if (isNewClilocs)
            {
                reader.Move(4);

                if (index == 0u)
                {
                    index = reader.ReadUInt16BE();
                    HookEnabled = false;
                    CPacketContextMenuSelection(Serial, index).SendServer();
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
                    CPacketContextMenuSelection(Serial, index).SendServer();
                    return false;
                }

                index--;

                reader.Move(2);
                ushort flags = reader.ReadUInt16BE();

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

bool CContextMenuManager::OnPacketSend(CDataReader &reader)
{
    (void)reader;
    return true;
}

} // namespace xuoassist
