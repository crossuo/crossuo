// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/packetmanager.h): the full 256-entry
// dispatch table and the handler set, decoupled from the UI widgets (plan §4.1
// "REUSE AS-IS (decouple UI)", §7 Phase 1 item 2). Handler names and table
// layout are identical; every UI poke became an AssistantEvents notification
// or, in later rounds, a direct manager/executor call; no marker remains.
//
// The class keeps the original CPacketManager name inside namespace xuoassist
// (the client's global-namespace CPacketManager is a different class); the
// PACKET_DIRECTION/CPacketInfo/HANDLER_PACKET pieces collide with the client
// header's macro, so the handler declaration macro is XA_HANDLER_PACKET here.

#pragma once

#include "../core/global.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

class CPacketManager;
typedef bool (CPacketManager::*PACKET_FUNCTION)();

//! Packet direction
enum PACKET_DIRECTION
{
    DIR_SEND = 0, //!From client to server
    DIR_RECV,     //!From server to client
    DIR_BOTH      //!Both directions
};

//! Class that stores packet information
class CPacketInfo
{
public:
    //! Packet name
    const char *Name;

    //! Packet size
    int Size;

    //! Packet direction
    PACKET_DIRECTION Direction;

    //! Handler for the sent packet
    PACKET_FUNCTION SendHandler;

    //! Handler for the received packet
    PACKET_FUNCTION RecvHandler;
};

#define XA_HANDLER_PACKET(name) bool Handle##name()

class CPacketManager : public CDataReader
{
public:
    CLIENT_VERSION ClientVersion = CV_OLD;

    // init-time completeness check over m_Packets (see packetmanager.cpp)
    static void CheckPacketsTable();

private:
    static CPacketInfo m_Packets[0x100];

protected:
    //! Packet handlers
    XA_HANDLER_PACKET(WarmodeS);
    XA_HANDLER_PACKET(TargetS);
    XA_HANDLER_PACKET(ClientTalkS);
    XA_HANDLER_PACKET(UnicodeClientTalk);
    XA_HANDLER_PACKET(OpenMenu);
    XA_HANDLER_PACKET(MenuChoice);
    XA_HANDLER_PACKET(ClientMessagesS);
    XA_HANDLER_PACKET(ClientMessagesR);
    XA_HANDLER_PACKET(ExtendedCommandS);
    XA_HANDLER_PACKET(SecureTradingS);
    XA_HANDLER_PACKET(DyeDataS);
    XA_HANDLER_PACKET(OpenGump);
    XA_HANDLER_PACKET(ChoiceGump);
    XA_HANDLER_PACKET(Resend);
    XA_HANDLER_PACKET(WalkRequest);
    XA_HANDLER_PACKET(ASCIIPromptS);
    XA_HANDLER_PACKET(UnicodePromptS);
    XA_HANDLER_PACKET(BuyReplyS);
    XA_HANDLER_PACKET(SellList);
    XA_HANDLER_PACKET(SellReply);
    XA_HANDLER_PACKET(PerformAction);
    XA_HANDLER_PACKET(DoubleClick);
    XA_HANDLER_PACKET(AttackRequest);
    XA_HANDLER_PACKET(HelpRequest);
    XA_HANDLER_PACKET(AOSCommandsS);
    XA_HANDLER_PACKET(MapPacket);

public:
    CPacketManager();
    virtual ~CPacketManager();

    CPacketInfo GetInfo(const uint8_t &buf) const { return m_Packets[buf]; }

    bool PacketRecv(uint8_t *buf, size_t size);
    bool PacketSend(uint8_t *buf, size_t size);
};

extern CPacketManager g_PacketManager;

// Tap entry points for the client's packet paths (src/Managers/PacketManager
// .cpp OnPacket, src/CrossUO.cpp CGame::Send, src/Network/PluginPackets.cpp
// SendToAssistant). Each is a single connected-check before dispatch. Recv may
// consume packets (returns true when the
// packet was swallowed - the WaitTarget auto-target eats the 0x6C target-cursor
// reply so the client keeps no open cursor); Send never consumes.
// Returns true when the packet was consumed by the assistant.
bool AssistantPackets_Recv(uint8_t *buf, size_t size);
void AssistantPackets_Send(uint8_t *buf, size_t size);

} // namespace xuoassist
