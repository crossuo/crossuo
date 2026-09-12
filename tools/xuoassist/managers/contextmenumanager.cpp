// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/contextmenumanager.cpp).

#include "contextmenumanager.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include <common/logging/logging.h>

namespace xuoassist {

CContextMenuManager g_ContextMenuManager;

CContextMenuManager::CContextMenuManager() {}

void CContextMenuManager::SetHook(const uint &id, const uint &index)
{
    Info(Assistant, "CContextMenuManager::SetHook: 0x%08X %i\n", id, index);
    m_HookEnabled = true;
    m_HookID = id;
    m_HookIndex = index;
}

void CContextMenuManager::RemoveHook()
{
    m_HookEnabled = false;
}

bool CContextMenuManager::OnPacketReceived(CDataReader &reader)
{
    ushort mode = reader.ReadUInt16BE();
    bool isNewClilocs = (mode >= 2);
    m_Serial = reader.ReadUInt32BE();

    // PORT-TODO(phase-3): was `g_TabMacros->GetPlaying() &&
    // g_TabMacros->PlayWaiting({ MT_WAIT_FOR_CONTEXT_MENU })` -> return false
    // (macro round owns the wait queue).

    if (m_HookEnabled)
    {
        if ((m_HookID != 0u) && m_Serial != m_HookID)
        {
            return true;
        }

        uchar count = reader.ReadUInt8();

        uint index = m_HookIndex;

        for (int i = 0; i < count; i++)
        {
            if (isNewClilocs)
            {
                reader.Move(4);

                if (index == 0u)
                {
                    index = reader.ReadUInt16BE();
                    m_HookEnabled = false;
                    CPacketContextMenuSelection(m_Serial, index).SendServer();
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
                    m_HookEnabled = false;
                    CPacketContextMenuSelection(m_Serial, index).SendServer();
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
