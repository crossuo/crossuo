// MIT License
// Copyright (C) August 2016 Hotride

#include "Party.h"
#include "OrionUO.h"
#include "Network/Packets.h"
#include "Managers/ConfigManager.h"
#include "Managers/GumpManager.h"
#include "Managers/MouseManager.h"
#include "GameObjects/GameWorld.h"
#include "GameObjects/GameCharacter.h"
#include "Gumps/GumpStatusbar.h"

CParty g_Party;

CParty::CParty()
{
}

CParty::~CParty()
{
}

bool CParty::Contains(int serial)
{
    DEBUG_TRACE_FUNCTION;

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

void CParty::Clear()
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < 10; i++)
    {
        Member[i].Serial = 0;
        Member[i].Character = nullptr;
    }
}

void CParty::ParsePacketData(Wisp::CDataReader &reader)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t code = reader.ReadUInt8();

    switch (code)
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
                    if (member.Character == nullptr)
                    {
                        break;
                    }
                    CGumpStatusbar *gump = (CGumpStatusbar *)g_GumpManager.UpdateContent(
                        member.Character->Serial, 0, GT_STATUSBAR);
                    if (gump != nullptr)
                    {
                        gump->WantRedraw = true;
                    }
                }
                Clear();
                g_GumpManager.UpdateContent(0, 0, GT_PARTY_MANIFEST);
                break;
            }
            Clear();
            CPoint2Di oldPos = g_MouseManager.Position;
            CPoint2Di mousePos(76, 30);
            g_MouseManager.Position = mousePos;
            CGumpStatusbar *prevGump = nullptr;

            for (int i = 0; i < count; i++)
            {
                uint32_t serial = reader.ReadUInt32BE();
                Member[i].Serial = serial;
                Member[i].Character = g_World->FindWorldCharacter(serial);
                if (i == 0)
                {
                    g_Party.Leader = serial;
                }

                CGumpStatusbar *gump =
                    (CGumpStatusbar *)g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);

                if (gump == nullptr)
                {
                    g_Orion.OpenStatus(serial);
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
            g_GumpManager.UpdateContent(0, 0, GT_PARTY_MANIFEST);
            break;
        }
        case 3: //Private party message
        case 4: //Party message
        {
            uint32_t serial = reader.ReadUInt32BE();
            wstring name = reader.ReadWStringBE();

            for (int i = 0; i < 10; i++)
            {
                if (Member[i].Serial == serial)
                {
                    string str = "[" + Member[i].GetName((int)i) + "]: " + ToString(name);
                    g_Orion.CreateTextMessage(
                        TT_SYSTEM, serial, 3, g_ConfigManager.PartyMessageColor, str);

                    break;
                }
            }

            break;
        }
        case 7: //Party invition
        {
            g_Party.Inviter = reader.ReadUInt32BE();
            break;
        }
        default:
            break;
    }
}
