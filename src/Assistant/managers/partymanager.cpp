// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// decline invitation lists live in the AssistantConfig module (Party.xml) —
// the original code asked TabAgentsParty.
//
// This parser is not redundant with the client's CParty::ParsePacketData
// (src/Party.cpp): the assistant recv tap runs before the client's 0xBF
// handler, and the client parse does not retain what the accept/decline
// decision needs — sub 7 keeps only the first dword (g_Party.Inviter, never
// cleared when an invitation is answered, only on disband), so a deferred
// decision cannot tell a fresh invitation from stale state, and subs 3/4
// feed the assistant journal record (the client routes party chat into its
// own text engine). Subs 1/2 also differ: the client takes Member[0] as
// leader and clears on count <= 1, this manager marks the leader on join.

#include "partymanager.h"
#include "../core/assistantevents.h"
#include "../core/packets.h"
#include "../config/assistantconfig.h"

#include <string.h>

namespace xuoassist {

CPartyManager g_PartyManager;

CPartyManager::CPartyManager()
{
    memset(&Member[0], 0, sizeof(Member));
}

bool CPartyManager::Contains(const uint &serial)
{
    bool result = false;

    if (m_Leader != 0)
    {
        for (int i = 0; i < 10; i++)
        {
            if (Member[i] == serial)
            {
                result = true;

                break;
            }
        }
    }

    return result;
}


void CPartyManager::Clear()
{
    for (int i = 0; i < 10; i++)
    {
        Member[i] = 0;
    }
}

bool CPartyManager::OnPacketReceived(CDataReader &reader)
{
    if (!EnabledFeaturePartyAgent())
    {
        return true;
    }

    uchar subCmd = reader.ReadUInt8();

    switch (subCmd)
    {
        case 1: //Add member
        case 2: //Remove member
        {
            Clear();

            uchar count = reader.ReadUInt8();

            if (subCmd == 1)
            {
                if (m_Leader == 0)
                {
                    m_Leader = g_PlayerSerial;
                    m_Inviter = 0;
                }
            }
            else if (count <= 1)
            {
                m_Leader = 0;
                m_Inviter = 0;

                break;
            }

            if (count > 10)
            {
                count = 10;
            }

            for (int i = 0; i < count; i++)
            {
                Member[i] = reader.ReadUInt32BE();
            }

            break;
        }
        case 3: //Private party message
        case 4: //Party message
        {
            uint serial = reader.ReadUInt32BE();
            wstring str = reader.ReadWString(0, true);

            CJournalMessage message(serial, 0x0044, str_from(str));
            g_AssistantEvents->OnJournalMessage(message);

            break;
        }
        case 7:
        {
            m_Inviter = reader.ReadUInt32BE();

            if (g_AssistantConfig.PartyInviteCanBeAutoAccepted(serial))
            {
                CPacketPartyAccept(serial).SendServer();
            }
            else if (g_AssistantConfig.PartyInviteCanBeAutoDeclined(serial))
            {
                CPacketPartyDecline(serial).SendServer();
            }

            break;
        }
        default:
            break;
    }

    return true;
}

} // namespace xuoassist
