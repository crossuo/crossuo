// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "Party.h"
#include "CrossUO.h"
#include <common/str.h>
#include <xuocore/mappedfile.h>
#include "Network/Packets.h"
#include "Managers/ConfigManager.h"
#include "Managers/GumpManager.h"
#include "Managers/MouseManager.h"
#include "GameObjects/GameWorld.h"
#include "GameObjects/GameCharacter.h"
#include "Gumps/GumpStatusbar.h"
#if defined(XUO_ASSISTANT)
#include <config/assistantconfig.h> // xuoassist::g_AssistantConfig (invitation rules)
#include <core/assistantevents.h>   // xuoassist::g_AssistantEvents (party chat journal)
#include <core/global.h>            // xuoassist::EnabledFeaturePartyAgent
#endif

CParty g_Party;

astr_t CPartyObject::GetName(int index)
{
    if (Serial != 0u)
    {
        if (Character == nullptr)
        {
            Character = g_World->FindWorldCharacter(Serial);
        }
        if (Character != nullptr)
        {
            return Character->GetName();
        }
    }
    char buf[10];
    snprintf(buf, sizeof(buf), "[%i]", index);
    return astr_t(buf);
}

bool CParty::Contains(int serial)
{
    bool result = false;
    if (Leader != 0)
    {
        for (int i = 0; i < 10; i++)
        {
            if (Member[i].Serial == serial)
            {
                result = true;
                break;
            }
        }
    }

    return result;
}

void CParty::SaveMemberInfo(CGameCharacter *obj)
{
    if (obj == nullptr)
    {
        return;
    }

    for (int i = 0; i < 10; i++)
    {
        if (Member[i].Serial == obj->Serial)
        {
            Member[i].X = obj->GetX();
            Member[i].Y = obj->GetY();
            Member[i].Name = obj->GetName();
            break;
        }
    }
}

bool CParty::SetMemberInfo(uint32_t serial, int x, int y, uint8_t mapIndex, const astr_t &name)
{
    for (int i = 0; i < 10; i++)
    {
        if (Member[i].Serial == serial)
        {
            Member[i].X = x;
            Member[i].Y = y;
            Member[i].MapIndex = mapIndex;
            Member[i].Name = name;
            return true;
        }
    }

    return false;
}

void CParty::Clear()
{
    for (int i = 0; i < 10; i++)
    {
        Member[i].Serial = 0;
        Member[i].Character = nullptr;
        Member[i].X = 0;
        Member[i].Y = 0;
        Member[i].MapIndex = 0;
        Member[i].Name.clear();
    }
}

void CParty::ParsePacketData(CDataReader &reader)
{
    uint8_t subCmd = reader.ReadUInt8();

    switch (subCmd)
    {
        case 1: //Add member
        {
        }
        case 2: //Remove member
        {
            uint8_t count = reader.ReadUInt8();

            if (count <= 1)
            {
                Leader = 0;
                Inviter = 0;
                for (int i = 0; i < 10; i++)
                {
                    CPartyObject &member = Member[i];
                    if (member.Serial == 0u)
                    {
                        break;
                    }
                    CGumpStatusbar *gump =
                        (CGumpStatusbar *)g_GumpManager.UpdateContent(member.Serial, 0, GT_STATUSBAR);
                    if (gump != nullptr)
                    {
                        gump->WantRedraw = true;
                    }
                }
                Clear();
                g_GumpManager.UpdateContent(0, 0, GT_PARTY_MANIFEST);
                break;
            }
            // A remove lists the whole party: the entry matching the removed
            // serial is dropped, the player or the first entry (the leader)
            // being the removed one takes the whole party down
            uint32_t toRemove = 0xFFFFFFFF;
            if (subCmd == 2u)
            {
                toRemove = reader.ReadUInt32BE();
            }

            bool removeAll = (subCmd == 2u && toRemove == g_PlayerSerial);
            int done = 0;

            CPoint2Di oldPos = g_MouseManager.Position;
            CPoint2Di mousePos(76, 30);
            g_MouseManager.Position = mousePos;
            CGumpStatusbar *prevGump = nullptr;

            // the packet carries the full manifest: stale tail entries from a
            // larger party must not survive a shrink
            Clear();

            for (int i = 0; i < count; i++)
            {
                uint32_t serial = reader.ReadUInt32BE();
                const bool remove = (subCmd == 2u && serial == toRemove);

                if (remove && i == 0)
                {
                    removeAll = true;
                }

                CGumpStatusbar *gump =
                    (CGumpStatusbar *)g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);

                if (remove || removeAll)
                {
                    if (gump != nullptr)
                    {
                        gump->WantRedraw = true;
                    }
                    continue;
                }

                Member[i].Serial = serial;
                Member[i].Character = g_World->FindWorldCharacter(serial);
                if (i == 0)
                {
                    Leader = serial;
                }
                done++;

                if (gump == nullptr)
                {
                    g_Game.OpenStatus(serial);
                    gump = (CGumpStatusbar *)g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);

                    if (serial == g_PlayerSerial)
                    {
                        gump->Minimized = false;
                    }

                    if (prevGump != nullptr)
                    {
                        prevGump->AddStatusbar(gump);
                    }

                    prevGump = gump;
                    mousePos.Y += 59;
                    g_MouseManager.Position = mousePos;
                }
                else
                {
                    CPacketStatusRequest(serial).Send();
                    gump->WantRedraw = true;
                }
            }

            g_MouseManager.Position = oldPos;

            // a remove that leaves the leader alone disbands the party
            if (done <= 1 && subCmd == 2u)
            {
                for (int i = 0; i < 10; i++)
                {
                    CPartyObject &member = Member[i];
                    if (member.Serial == 0u)
                    {
                        break;
                    }
                    CGumpStatusbar *gump =
                        (CGumpStatusbar *)g_GumpManager.UpdateContent(member.Serial, 0, GT_STATUSBAR);
                    if (gump != nullptr)
                    {
                        gump->WantRedraw = true;
                    }
                }

                Clear();
            }

            g_GumpManager.UpdateContent(0, 0, GT_PARTY_MANIFEST);
            break;
        }
        case 3: //Private party message
        case 4: //Party message
        {
            uint32_t serial = reader.ReadUInt32BE();
            auto name = reader.ReadWStringBE();

#if defined(XUO_ASSISTANT)
            // the party chat record keeps the speaker serial and the message
            // text (the speaker name resolves from the member list);
            // xuoassist journaled every party message unconditionally
            // (partymanager.cpp) - the feature gate covers only the
            // invitation agent below
            xuoassist::CJournalMessage message(serial, 0x0044, str_from(name));
            xuoassist::g_AssistantEvents->OnJournalMessage(message);
#endif

            for (int i = 0; i < 10; i++)
            {
                if (Member[i].Serial == serial)
                {
                    astr_t str = "[" + Member[i].GetName((int)i) + "]: " + str_from(name);
                    g_Game.CreateTextMessage(
                        TT_SYSTEM, serial, 3, g_ConfigManager.PartyMessageColor, str);

                    break;
                }
            }

            break;
        }
        case 7: //Party invition
        {
            Inviter = reader.ReadUInt32BE();

#if defined(XUO_ASSISTANT)
            // the party agent answers the invitation during the parse: the
            // accept rules take precedence over the decline rules, and an
            // answered invitation is cleared like a manual answer
            if (xuoassist::EnabledFeaturePartyAgent())
            {
                if (xuoassist::g_AssistantConfig.PartyInviteCanBeAutoAccepted(Inviter))
                {
                    CPacketPartyAccept(Inviter).Send();
                    Leader = Inviter;
                    Inviter = 0;
                }
                else if (xuoassist::g_AssistantConfig.PartyInviteCanBeAutoDeclined(Inviter))
                {
                    CPacketPartyDecline(Inviter).Send();
                    Inviter = 0;
                }
            }
#endif
            // TODO: Party Invite Gump?
            break;
        }
        default:
            break;
    }
}
