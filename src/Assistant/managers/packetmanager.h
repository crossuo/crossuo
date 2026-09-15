// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/packetmanager.h): the full 256-entry
// dispatch table and the handler set, decoupled from the UI widgets (plan §4.1
// "REUSE AS-IS (decouple UI)", §7 Phase 1 item 2). Handler names and table
// layout are identical; every UI poke became an AssistantEvents notification
// or, in later rounds, a direct manager/executor call; no marker remains.
//
// The class keeps the original CPacketManager name inside namespace xuoassist
// (the client's global-namespace CPacketManager is a different class).

#pragma once

#include "../core/global.h"
#include <common/str.h> // astr_t (the speech-intercept entry)
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

class CPacketManager : public CDataReader
{
public:
    CLIENT_VERSION ClientVersion = CV_OLD;

    // init-time completeness check over m_Packets (see packetmanager.cpp)
    static void CheckPacketsTable();

private:
    static CPacketInfo m_Packets[0x100];

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

// The client's speech send sites (game console tail, MacroManager's say
// actions, CGame::SendASCIIText): were the 0x03 ClientTalkS / 0xAD
// UnicodeClientTalk send taps (the two rows differed only in text extraction
// from the wire - the 0xAD row decoded the encoded-command form to recover
// the typed text, which the send sites already hold). Runs the command-prefix
// intercept (returns false - the caller suppresses the send; the old taps
// returned "consumed" into a void entry point, so prefixed lines both ran
// the command and went out as raw speech, and the client-site early return
// restores the interception's purpose), records the say line and applies the
// font-color override to `color` by reference.
bool AssistantPackets_SpeechIntercept(astr_t &text, uint16_t &color);

} // namespace xuoassist
