// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "packetmanager.h"
#include "../core/assistantevents.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include "../core/shopagent.h"
#include "../core/clientversion.h"
#include "../core/target.h"
#include "../core/textcommandmanager.h"
#include "../config/assistantconfig.h"
#include "../managers/assmap.h" // g_MapSettings (ShowOnLogin)
#include "../managers/clientmacromanager.h"
#include "../managers/contextmenumanager.h"
#include "../managers/guildmanager.h"
#include "../managers/gumpmanager.h"
#include "../managers/menumanager.h"
#include "../managers/partymanager.h"
#include "../managers/promptmanager.h"
#include "../managers/skillmanager.h"
#include "../managers/spellmanager.h"
#include "../managers/trademanager.h"
#include "../macros/macroexecutor.h" // g_MacroExecutor (macro recording/playing taps)
#include "../core/world.h"
#include "../model/commonitems/journalmessage.h"
#include <common/str.h> // str_from/wstr_from (wstr <-> utf8)
#include "Managers/ClilocManager.h" // ::g_ClilocManager (client)
#include "Managers/CorpseManager.h" // ::g_CorpseManager (corpse auto-open)
#include "Managers/SkillsManager.h" // ::g_SkillsManager (the client skill table)
#include "Party.h" // ::g_Party (shared party data)

#define LOGGER_MODULE Assistant
#include <common/logging/logging.h>
#include "CrossUO.h" // ::g_Game (ValueInt/ValueString/GetStaticFlags bridge)

namespace xuoassist {

CPacketManager g_PacketManager;

const int PACKET_VARIABLE_SIZE = 0;

// Map of UO packets for parsing
#define UMSG(size)                                                                                 \
    {                                                                                              \
        "?", size, DIR_BOTH, nullptr, nullptr                                                      \
    }
#define SMSG(name, size)                                                                           \
    {                                                                                              \
        name, size, DIR_SEND, nullptr, nullptr                                                     \
    }
#define RMSG(name, size)                                                                           \
    {                                                                                              \
        name, size, DIR_RECV, nullptr, nullptr                                                     \
    }
#define BMSG(name, size)                                                                           \
    {                                                                                              \
        name, size, DIR_BOTH, nullptr, nullptr                                                     \
    }
#define RMSGH(name, size, rmethod)                                                                 \
    {                                                                                              \
        name, size, DIR_RECV, nullptr, &CPacketManager::Handle##rmethod                            \
    }
#define SMSGH(name, size, smethod)                                                                 \
    {                                                                                              \
        name, size, DIR_SEND, &CPacketManager::Handle##smethod, nullptr                            \
    }
#define BMSGH(name, size, smethod, rmethod)                                                        \
    {                                                                                              \
        name, size, DIR_BOTH, &CPacketManager::Handle##smethod, &CPacketManager::Handle##rmethod   \
    }

CPacketInfo CPacketManager::m_Packets[0x100] = {
    /*0x00*/ SMSG("Create Character", 0x68),
    /*0x01*/ SMSG("Disconnect", 0x05),
    /*0x02*/ SMSGH("Walk Request", 0x07, WalkRequest),
    /*0x03*/
    BMSGH("Client Talk", PACKET_VARIABLE_SIZE, ClientTalkS, ClientTalkR),
    /*0x04*/ SMSG("Request God mode (God client)", 0x02),
    /*0x05*/ SMSGH("Attack", 0x05, AttackRequest),
    /*0x06*/ SMSGH("Double Click", 0x05, DoubleClick),
    /*0x07*/ SMSG("Pick Up Item", 0x07),
    /*0x08*/ SMSG("Drop Item", 0x0e),
    /*0x09*/ SMSG("Single Click", 0x05),
    /*0x0A*/ SMSG("Edit (God client)", 0x0b),
    /*0x0B*/ RMSG("Damage Visualization", 0x07),
    /*0x0C*/ BMSG("Edit tiledata (God client)", PACKET_VARIABLE_SIZE),
    /*0x0D*/ UMSG(0x03),
    /*0x0E*/ UMSG(0x01),
    /*0x0F*/ UMSG(0x3d),
    /*0x10*/ UMSG(0xd7),
    /*0x11*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x12*/ SMSGH("Perform Action", PACKET_VARIABLE_SIZE, PerformAction),
    /*0x13*/ SMSG("Client Equip Item", 0x0a),
    /*0x14*/ SMSG("Send elevation (God client)", 0x06),
    /*0x15*/ BMSG("Follow", 0x09),
    /*0x16*/ UMSG(0x01),
    /*0x17*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x18*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x19*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x1A*/ RMSG("Update Item", PACKET_VARIABLE_SIZE),
    /*0x1B*/ UMSG(0x25),
    /*0x1C*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x1D*/ UMSG(0x05),
    /*0x1E*/ UMSG(0x04),
    /*0x1F*/ UMSG(0x08),
    /*0x20*/ UMSG(0x13),
    /*0x21*/ RMSG("Deny Walk", 0x08),
    /*0x22*/ BMSGH("Confirm Walk", 0x03, Resend, ConfirmWalk),
    /*0x23*/ RMSG("Drag Animation", 0x1a),
    /*0x24*/ UMSG(0x07),
    /*0x25*/ RMSG("Update Contained Item", 0x14),
    /*0x26*/ UMSG(0x05),
    /*0x27*/ RMSG("Deny Move Item", 0x02),
    /*0x28*/ UMSG(0x05),
    /*0x29*/ RMSG("Drop Item Approved", 0x01),
    /*0x2A*/ UMSG(0x05),
    /*0x2B*/ UMSG(0x02),
    /*0x2C*/ BMSGH("Death Screen", 0x02, DeathScreenS, DeathScreenR),
    /*0x2D*/ UMSG(0x11),
    /*0x2E*/ UMSG(0x0f),
    /*0x2F*/ RMSG("Combat Notification", 0x0a),
    /*0x30*/ RMSG("Attack ok", 0x05),
    /*0x31*/ RMSG("Attack end", 0x01),
    /*0x32*/ UMSG(0x02),
    /*0x33*/ RMSG("Pause Control", 0x02),
    /*0x34*/ SMSG("Status Request", 0x0a),
    /*0x35*/ UMSG(0x28d),
    /*0x36*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x37*/ UMSG(0x08),
    /*0x38*/ BMSG("Pathfinding in Client", 0x07),
    /*0x39*/ RMSG("Remove (Group)", 0x09),
    /*0x3A*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x3B*/
    BMSGH("Vendor Buy Reply", PACKET_VARIABLE_SIZE, BuyReplyS, BuyReplyR),
    /*0x3C*/ RMSG("Update Contained Items", PACKET_VARIABLE_SIZE),
    /*0x3D*/ UMSG(0x02),
    /*0x3E*/ UMSG(0x25),
    /*0x3F*/ RMSG("Update Statics (God Client)", PACKET_VARIABLE_SIZE),
    /*0x40*/ UMSG(0xc9),
    /*0x41*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x42*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x43*/ UMSG(0x229),
    /*0x44*/ UMSG(0x2c9),
    /*0x45*/ BMSG("Version OK", 0x05),
    /*0x46*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x47*/ UMSG(0x0b),
    /*0x48*/ UMSG(0x49),
    /*0x49*/ UMSG(0x5d),
    /*0x4A*/ UMSG(0x05),
    /*0x4B*/ UMSG(0x09),
    /*0x4C*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x4D*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x4E*/ UMSG(0x06),
    /*0x4F*/ UMSG(0x02),
    /*0x50*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x51*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x52*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x53*/ RMSG("Error Code", 0x02),
    /*0x54*/ UMSG(0x0c),
    /*0x55*/ UMSG(0x01),
    // KEEP (recv tap routes 0x56): assistant map engine — the edit-response
    // pin translates into absolute coords + tracker points; no client producer.
    /*0x56*/ RMSGH("Map Data", 0x0b, MapPacket),
    /*0x57*/ BMSG("Update Regions", 0x6e),
    /*0x58*/ UMSG(0x6a),
    /*0x59*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x5A*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x5B*/ RMSG("Set Time", 0x04),
    /*0x5C*/ BMSG("Restart Version", 0x02),
    /*0x5D*/ SMSG("Select Character", 0x49),
    /*0x5E*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x5F*/ UMSG(0x31),
    /*0x60*/ UMSG(0x05),
    /*0x61*/ UMSG(0x09),
    /*0x62*/ UMSG(0x0f),
    /*0x63*/ UMSG(0x0d),
    /*0x64*/ UMSG(0x01),
    /*0x65*/ UMSG(0x04),
    /*0x66*/ BMSG("Book Page Data", PACKET_VARIABLE_SIZE),
    /*0x67*/ UMSG(0x15),
    /*0x68*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x69*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x6A*/ UMSG(0x03),
    /*0x6B*/ UMSG(0x09),
    /*0x6C*/ BMSGH("Target Data", 0x13, TargetS, TargetR),
    /*0x6D*/ RMSG("Play Music", 0x03),
    /*0x6E*/ UMSG(0x0e),
    /*0x6F*/
    BMSGH("Secure Trading", PACKET_VARIABLE_SIZE, SecureTradingS, SecureTradingR),
    /*0x70*/ UMSG(0x1c),
    /*0x71*/ BMSG("Bulletin Board Data", PACKET_VARIABLE_SIZE),
    /*0x72*/ SMSGH("War Mode", 0x05, WarmodeS),
    /*0x73*/ BMSG("Ping", 0x02),
    /*0x74*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x75*/ SMSG("Rename Character", 0x23),
    /*0x76*/ RMSG("New Subserver", 0x10),
    /*0x77*/ UMSG(0x11),
    /*0x78*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x79*/ UMSG(0x09),
    /*0x7A*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x7B*/ UMSG(0x02),
    /*0x7C*/ RMSGH("Open Menu", PACKET_VARIABLE_SIZE, OpenMenu),
    /*0x7D*/ SMSGH("Menu Choice", 0x0d, MenuChoice),
    /*0x7E*/ UMSG(0x02),
    /*0x7F*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x80*/ SMSG("First Login", 0x3e),
    /*0x81*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x82*/ RMSG("Login Error", 0x02),
    /*0x83*/ SMSG("Delete Character", 0x27),
    /*0x84*/ UMSG(0x45),
    /*0x85*/ RMSG("Character List Notification", 0x02),
    /*0x86*/ RMSG("Resend Character List", PACKET_VARIABLE_SIZE),
    /*0x87*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x88*/ RMSG("Open Paperdoll", 0x42),
    /*0x89*/ RMSG("Corpse Equipment", PACKET_VARIABLE_SIZE),
    /*0x8A*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8B*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8C*/ RMSG("Relay Server", 0x0b),
    /*0x8D*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8E*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8F*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x90*/ UMSG(0x13),
    /*0x91*/ SMSGH("Second Login", 0x41, SecondLogin),
    /*0x92*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x93*/ RMSG("Open Book", 0x63),
    /*0x94*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x95*/ BMSGH("Dye Data", 0x09, DyeDataS, DyeDataR),
    /*0x96*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x97*/ RMSG("Move Player", 0x02),
    /*0x98*/ BMSG("All Names (3D Client Only)", PACKET_VARIABLE_SIZE),
    /*0x99*/ UMSG(0x1a),
    /*0x9A*/
    BMSGH("ASCII Prompt", PACKET_VARIABLE_SIZE, ASCIIPromptS, ASCIIPromptR),
    /*0x9B*/ SMSGH("Help Request", 0x102, HelpRequest),
    /*0x9C*/ UMSG(0x135),
    /*0x9D*/ UMSG(0x33),
    /*0x9E*/ RMSGH("Vendor Sell List", PACKET_VARIABLE_SIZE, SellList),
    /*0x9F*/ SMSGH("Vendor Sell Reply", PACKET_VARIABLE_SIZE, SellReply),
    /*0xA0*/ SMSG("Select Server", 0x03),
    /*0xA1*/ UMSG(0x09),
    /*0xA2*/ UMSG(0x09),
    /*0xA3*/ UMSG(0x09),
    /*0xA4*/ SMSG("System Information", 0x95),
    /*0xA5*/ RMSG("Open URL", PACKET_VARIABLE_SIZE),
    /*0xA6*/ RMSG("Tip Window", PACKET_VARIABLE_SIZE),
    /*0xA7*/ SMSG("Request Tip", 0x04),
    /*0xA8*/ RMSG("Server List", PACKET_VARIABLE_SIZE),
    /*0xA9*/ RMSG("Character List", PACKET_VARIABLE_SIZE),
    /*0xAA*/ UMSG(0x05),
    /*0xAB*/ RMSG("Text Entry Dialog", PACKET_VARIABLE_SIZE),
    /*0xAC*/ SMSG("Text Entry Dialog Reply", PACKET_VARIABLE_SIZE),
    /*0xAD*/
    SMSGH("Unicode Client Talk", PACKET_VARIABLE_SIZE, UnicodeClientTalk),
    /*0xAE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xAF*/ UMSG(0x0d),
    /*0xB0*/ RMSGH("Open Gump", PACKET_VARIABLE_SIZE, OpenGump),
    /*0xB1*/ SMSGH("Gump Choice", PACKET_VARIABLE_SIZE, ChoiceGump),
    /*0xB2*/ BMSG("Chat Data", PACKET_VARIABLE_SIZE),
    /*0xB3*/ RMSG("Chat Text ?", PACKET_VARIABLE_SIZE),
    /*0xB4*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xB5*/ BMSG("Open Chat Window", 0x40),
    /*0xB6*/ SMSG("Popup Help Request", 0x09),
    /*0xB7*/ RMSG("Popup Help Data", PACKET_VARIABLE_SIZE),
    /*0xB8*/
    BMSGH("Character Profile", PACKET_VARIABLE_SIZE, CharacterProfileS, CharacterProfileR),
    /*0xB9*/ RMSG("Enable locked client features", 0x03),
    /*0xBA*/ RMSG("Display Quest Arrow", 0x06),
    /*0xBB*/ SMSG("Account ID ?", 0x09),
    /*0xBC*/ UMSG(0x03),
    /*0xBD*/ BMSG("Client Version", PACKET_VARIABLE_SIZE),
    /*0xBE*/ BMSG("Assist Version", PACKET_VARIABLE_SIZE),
    /*0xBF*/
    BMSGH("Extended Command", PACKET_VARIABLE_SIZE, ExtendedCommandS, ExtendedCommandR),
    /*0xC0*/ UMSG(0x24),
    /*0xC1*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xC2*/
    BMSGH("Unicode prompt", PACKET_VARIABLE_SIZE, UnicodePromptS, UnicodePromptR),
    /*0xC3*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xC4*/ UMSG(0x06),
    /*0xC5*/ UMSG(0xcb),
    /*0xC6*/ UMSG(0x01),
    /*0xC7*/ UMSG(0x31),
    /*0xC8*/ BMSG("Client View Range", 0x02),
    /*0xC9*/ UMSG(0x06),
    /*0xCA*/ UMSG(0x06),
    /*0xCB*/ UMSG(0x07),
    /*0xCC*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xCD*/ UMSG(0x01),
    /*0xCE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xCF*/ UMSG(0x4e),
    /*0xD0*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xD1*/ UMSG(0x02),
    /*0xD2*/ UMSG(0x19),
    /*0xD3*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xD4*/ BMSG("Open Book (New)", PACKET_VARIABLE_SIZE),
    /*0xD5*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xD6*/
    BMSGH("Mega cliloc", PACKET_VARIABLE_SIZE, MegaClilocS, MegaClilocR),
    /*0xD7*/
    BMSGH("+AoS command", PACKET_VARIABLE_SIZE, AOSCommandsS, AOSCommandsR),
    /*0xD8*/ RMSG("+Custom house", PACKET_VARIABLE_SIZE),
    /*0xD9*/ SMSG("+Metrics", 0x10c),
    /*0xDA*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xDB*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xDC*/ RMSG("OPL Info Packet", 9),
    /*0xDD*/ RMSG("Compressed Gump", PACKET_VARIABLE_SIZE),
    /*0xDE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xDF*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xE0*/ SMSG("Bug Report KR", PACKET_VARIABLE_SIZE),
    /*0xE1*/ SMSG("Client Type KR/SA", 0x09),
    /*0xE2*/ RMSG("New Character Animation", 0xa),
    /*0xE3*/ RMSG("KR Encryption Responce", 0x4d),
    /*0xE4*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xE5*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xE6*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xE7*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xE8*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xE9*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xEA*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xEB*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xEC*/ SMSG("Equip Macro", PACKET_VARIABLE_SIZE),
    /*0xED*/ SMSG("Unequip item macro", PACKET_VARIABLE_SIZE),
    /*0xEE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xEF*/ SMSG("KR/2D Client Login/Seed", 0x15),
    /*0xF0*/
    BMSGH(
        "Krrios client special", PACKET_VARIABLE_SIZE, KrriosClientSpecialS, KrriosClientSpecialR),
    /*0xF1*/ BMSG("Freeshard List", PACKET_VARIABLE_SIZE),
    /*0xF2*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF3*/ RMSG("Update Item (SA)", 0x18),
    /*0xF4*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF5*/ UMSG(0x15),
    /*0xF6*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF7*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF8*/ SMSG("Character Creation (7.0.16.0)", 0x6a),
    /*0xF9*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFA*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFB*/ UMSG(PACKET_VARIABLE_SIZE),
    // 0xFC submessages are the in-band extension channel: the client itself
    // builds some (skill/spell/macro lists), and external applications may
    // deliver 0xFC packets through the server stream.
    /*0xFC*/
    BMSGH("CrossUO messages", PACKET_VARIABLE_SIZE, ClientMessagesS, ClientMessagesR),
    /*0xFD*/ RMSG("Razor Handshake", 0x8),
    /*0xFE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFF*/ UMSG(PACKET_VARIABLE_SIZE)
};

CPacketManager::CPacketManager()
{
}

CPacketManager::~CPacketManager()
{
}

bool CPacketManager::PacketRecv(uint8_t *buf, size_t size)
{
    CPacketInfo &packet = m_Packets[*buf];
    uint ticks = g_Ticks;
    g_LastPacketTime = ticks;
    g_TotalRecvSize += size;

    // The client logs packets itself (CPacketManager::OnPacket DEBUG_DUMP);
    // the assistant's XUOA line is dropped (see file header).

    SetClientVersion(GetClientVersion());

    if (packet.Direction != DIR_RECV && packet.Direction != DIR_BOTH)
    {
        Warning(Assistant, "Invalid message direction: 0x%02X\n", *buf);
        return true;
    }

    if (packet.RecvHandler != nullptr)
    {
        SetData(buf, size);
        packet.Size == 0 ? Move(3) : Move(1);
        return (this->*(packet.RecvHandler))();
    }

    return true;
}

bool CPacketManager::PacketSend(uint8_t *buf, size_t size)
{
    CPacketInfo &packet = m_Packets[*buf];
    uint ticks = g_Ticks;
    g_LastPacketTime = ticks;
    g_TotalSendSize += size;

    SetClientVersion(GetClientVersion());

    if (packet.Direction != DIR_SEND && packet.Direction != DIR_BOTH)
    {
        Warning(Assistant, "Invalid message direction: 0x%02X\n", *buf);
        return true;
    }

    if (packet.SendHandler != nullptr)
    {
        SetData(buf, size);
        packet.Size == 0 ? Move(3) : Move(1);
        return (this->*(packet.SendHandler))();
    }

    return true;
}

// --- tap entry points (called from src/, XUO_ASSISTANT guarded) ------------

// Wire the host-bridge pointers to the client implementations. The same
// client calls the bridge typedefs name are bound directly here.
static uint64_t CDECL Bridge_GetStaticFlags(unsigned short graphic)
{
    return g_Game.GetStaticFlags(graphic);
}

static int CDECL Bridge_GetValueInt(VALUE_KEY_INT key, int value)
{
    return g_Game.ValueInt(key, value);
}

static void CDECL Bridge_SetValueInt(VALUE_KEY_INT key, int value)
{
    g_Game.ValueInt(key, value);
}

// Backing storage for the IGameString contract (the client's CommonInterfaces
// uses one static instance for the same purpose).
static IGameString s_BridgeGameString;

static IGameString *CDECL Bridge_GetValueString(VALUE_KEY_STRING key, const char *value)
{
    return &s_BridgeGameString(g_Game.ValueString(key, value));
}

static void CDECL Bridge_SetValueString(VALUE_KEY_STRING key, const char *value)
{
    g_Game.ValueString(key, value);
}

// Table-integrity guard: m_Packets is a fixed [0x100] array initialized
// positionally (each row is commented /*0xNN*/), so a row deleted from the
// initializer silently shifts every later opcode by one and zero-fills the
// tail. Every real row names its packet (UMSG rows are "?"); zero-filled rows
// have a null Name, so counting named rows catches any gap. Runs once, at
// init, and only logs on mismatch.
void CPacketManager::CheckPacketsTable()
{
    int named = 0;
    for (int i = 0; i < 0x100; i++)
    {
        if (m_Packets[i].Name != nullptr)
        {
            named++;
        }
    }
    if (named != 0x100)
    {
        Warning(
            Assistant,
            "packet table incomplete: %d/256 entries (a row is missing from "
            "the m_Packets initializer)\n",
            named);
    }
}

static void AssistantPackets_Init()
{
    static bool s_initialized = false;
    if (s_initialized)
    {
        return;
    }

    s_initialized = true;
    CPacketManager::CheckPacketsTable();

    g_GetStaticFlags = Bridge_GetStaticFlags;
    g_GetInt = Bridge_GetValueInt;
    g_SetInt = Bridge_SetValueInt;
    g_GetString = Bridge_GetValueString;
    g_SetString = Bridge_SetValueString;
}

// The assistant's "logged in" gate: handlers only run while the assistant
// considers itself connected, and the game stage is the disconnect signal.
// Handshake packets are let through so the state machine can engage
// (EnterWorld/LoginComplete set g_Connected).
static bool AssistantPackets_GameStage()
{
    return g_GameState == GS_GAME || g_GameState == GS_GAME_BLOCKED;
}

static void AssistantPackets_CheckDisconnect()
{
    if (g_Connected && !AssistantPackets_GameStage())
    {
        // The client tears its own world down on the same stage change; the
        // assistant drops its connected flag and resets its own state only.
        g_MacroExecutor.Reset();
        g_Connected = false;
    }
}

// Returns the consumption result: true when the assistant consumed the packet
// and the client must skip its own processing. The internal handler chain keeps
// the reverse convention (true = pass through, false = consumed), so the
// result is inverted here. The only current producer of "consumed" is the
// WaitTarget hook path (TargetR/0x6C via CTarget::SetRecvData -> CheckHook,
// and its MultiPlacement/0x99 sibling): the auto-target swallows the reply so
// the client keeps no open cursor.
bool AssistantPackets_Recv(uint8_t *buf, size_t size)
{
    AssistantPackets_Init();

    AssistantPackets_CheckDisconnect();

    if (!g_Connected && *buf != 0x1B && *buf != 0x55 // EnterWorld / LoginComplete
        && *buf != 0xFC) // 0xFC submessages (skill/spell/macro lists are
                         // injected at startup, before any world exists)
    {
        return false;
    }

    return !g_PacketManager.PacketRecv(buf, size);
}

void AssistantPackets_Send(uint8_t *buf, size_t size)
{
    AssistantPackets_Init();

    AssistantPackets_CheckDisconnect();

    if (!g_Connected && *buf != 0x91) // Second Login
    {
        return;
    }

    g_PacketManager.PacketSend(buf, size);
}

#define PACKET_HANDLER(name) bool CPacketManager::Handle##name()

PACKET_HANDLER(SecondLogin)
{
    g_Connected = false;

    return true;
}

static string parse_cliloc_arguments(int cliloc, bool toCamelCase, wstr_t args)
{
    return str_from(g_ClilocManager.ParseArgumentsToClilocString(cliloc, toCamelCase, args));
}

PACKET_HANDLER(WarmodeS)
{
    // tabmacros recording (the WarmodeS handler): observe-only - the macro action is
    // noted from the outgoing packet, which still flows to the server.
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        g_MacroExecutor.Record(new CMacroWithIntValue(MT_WAR_MODE, ReadUInt8()));
    }

    return true;
}

PACKET_HANDLER(TargetS)
{
    // The assist/target round (Phase 1c) restores the original interception:
    if (g_TargetHandler != nullptr)
    {
        (g_Assistant.*(g_TargetHandler))(Start);
        g_TargetHandler = nullptr;
        g_Target.Reset();
        g_Target.RestoreTarget();
        return false;
    }

    return g_Target.SetSendData(*this);
}

PACKET_HANDLER(TargetR)
{
    g_TargetReceived = true;

    return g_Target.SetRecvData(*this); // (assist/target round)
}

PACKET_HANDLER(ClientTalkS)
{
    Move(1);
    ushort fontColor = ReadUInt16BE();
    Move(2);

    string str = ReadStringChecked(0);

    int prefixLen = g_AssistantConfig.IsCommandPrefix(str);
    if (prefixLen != 0)
    {
        str.erase(0, prefixLen);
        g_TextCommandManager.DoCommand(str);
        return false;
    }
    // DEVIATION: a leading '-' went to g_UOMapManager.UOAnotifyCmd
    // - the external UOA/UOAM command pipe (Windows WM_USER + atoms); there
    // is no in-process equivalent, the command stays a normal speech line.
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        g_MacroExecutor.Record(new CMacroWithStringValue(MT_SAY, str));
    }

    ushort replaceFontColorValue = 0;
    bool replaceFontColor = g_AssistantConfig.GetFontColor(replaceFontColorValue);

    if (!replaceFontColor && replaceFontColorValue != fontColor)
    {
        replaceFontColorValue = fontColor;
        g_AssistantConfig.SetFontColor(false, fontColor);
    }

    if (replaceFontColor)
    {
        pack16(Start + 4, replaceFontColorValue);
    }

    return true;
}

PACKET_HANDLER(ClientTalkR)
{
    return true;
}

PACKET_HANDLER(UnicodeClientTalk)
{
    SPEECH_TYPE type = (SPEECH_TYPE)ReadUInt8();

    ushort fontColor = ReadUInt16BE();

    Move(6);
    string str = "";

    if ((type & ST_ENCODED_COMMAND) != 0)
    {
        ushort word = ReadUInt16BE();

        int bits = (word >> 4) * 12 - 4;

        Move(bits / 8);

        if ((bits % 8) != 0)
        {
            Move(1);
        }

        str = ReadStringChecked(0);
    }
    else
    {
        str = str_from(ReadWString());
    }

    int prefixLen = g_AssistantConfig.IsCommandPrefix(str);
    if (prefixLen != 0)
    {
        str.erase(0, prefixLen);
        g_TextCommandManager.DoCommand(str);
        return false;
    }
    // DEVIATION: the '-' UOA command notify (external UOAM command
    // pipe) - see the ASCII branch above.
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        g_MacroExecutor.Record(new CMacroWithStringValue(MT_SAY, str));
    }

    ushort replaceFontColorValue = 0;
    bool replaceFontColor = g_AssistantConfig.GetFontColor(replaceFontColorValue);

    if (!replaceFontColor && replaceFontColorValue != fontColor)
    {
        replaceFontColorValue = fontColor;
        g_AssistantConfig.SetFontColor(false, fontColor);
    }

    if (replaceFontColor)
    {
        pack16(Start + 4, replaceFontColorValue);
    }

    return true;
}

PACKET_HANDLER(OpenMenu)
{
    // KEEP (recv tap routes 0x7C): the menu agent parses its own CMenu model
    // for the auto-answer hooks and a playing macro can consume the packet
    // (the client then keeps no menu gump); the client parse builds only the
    // UI gump.
    g_MenuReceived = true;

    return g_MenuManager.OnPacketReceived(*this); // (menu round)
}

PACKET_HANDLER(MenuChoice)
{
    return g_MenuManager.OnPacketSend(*this); // (menu round)
}

PACKET_HANDLER(DeathScreenS)
{
    return true;
}

PACKET_HANDLER(DeathScreenR)
{
    if (g_World != nullptr /*&& !ReadUInt8()*/)
    {
        if (g_AssistantConfig.Tracker())
        {
            g_Assistant.Track(true, -1, -1);
        }

        g_LastPlayerCorpsePosition = CPoint2Di(g_Player->GetX(), g_Player->GetY());
        g_LastPlayerDeadTimer = g_Ticks + 300000; // ms: g_Ticks is SDL_GetTicks (Main.cpp), 5 min

        if (g_AssistantConfig.NoDeathScreen())
        {
            g_Assistant.ClientPrint("You are DEAD!");
            return false;
        }
    }

    return true;
}

PACKET_HANDLER(ExtendedCommandS)
{
    ushort cmd = ReadUInt16BE();

    if (cmd == 0x0009) // Wrestling stun ability
    {
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(new CMacro(MT_WRESTLING_STUN));
        }
    }
    else if (cmd == 0x000A) // Wrestling disarm ability
    {
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(new CMacro(MT_WRESTLING_DISARM));
        }
    }
    else if (cmd == 0x001C) // Cast spell
    {
        bool lastSpell = g_LastSpellIndex != 0;

        Move(2);
        g_LastSpellIndex = ReadInt16BE();

        if (static_cast<int>(lastSpell) != g_LastSpellIndex)
        {
            g_AssistantEvents->OnTitleRedraw();
        }

        // tabmacros recording (ExtendedCommandS 0x001C): the MCT_LAST
        // conversion fires when the new spell index equals the previous
        // index's "was set" flag - kept verbatim from the original site.
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            CMacro *macro = new CMacroWithIntValue(MT_CAST_NEW, g_LastSpellIndex);

            if (static_cast<int>(lastSpell) == g_LastSpellIndex)
            {
                macro->SetConvertType(MCT_LAST);
            }

            g_MacroExecutor.Record(macro);
        }
    }
    else if (cmd == 0x0013) // Context menu request
    {
        uint serial = ReadUInt32BE();

        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(new CMacroWithObject(MT_REQUEST_CONTEXT_MENU, serial, 0, 0));
        }
    }
    else if (cmd == 0x0015) // Context menu selection
    {
        uint serial = ReadUInt32BE();
        ushort menuID = ReadUInt16BE();

        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            // the menu id rides the graphic field, like the original CMacroWithObject
            g_MacroExecutor.Record(new CMacroWithObject(MT_WAIT_FOR_CONTEXT_MENU, serial, menuID, 0));
        }
    }
    else if (cmd == 0x0032) // Toggle gargoyle flying
    {
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(new CMacro(MT_TOGGLE_GARGOYLE_FLYING));
        }
    }

    return true;
}

PACKET_HANDLER(ExtendedCommandR)
{
    // KEEP (recv tap routes 0xBF): the client's party parse (case 6) drops
    // the invite events and party chat the party agent needs; the
    // equipment-info journal/inspector round, the context-menu round and the
    // assistant facet map (case 8) ride the same row.
    ushort cmd = ReadUInt16BE();

    switch (cmd)
    {
        case 6: // party
        {
            return g_PartyManager.OnPacketReceived(*this); // (agents/party round)
        }
        case 8: //
        {
            uint oldMap = g_CurrentMap;
            g_CurrentMap = ReadUInt8();

            if (g_CurrentMap > 5)
            {
                g_CurrentMap = 0;
            }

            if (oldMap != g_CurrentMap)
            {
                // DEVIATION: g_UOMapManager.UOAMnotifyLand() was a
                // WM_USER notify to the external UOAM app (no host here); the
                // map window follows g_CurrentMap for the "Current" facet.
                // g_FileManager.OnMapChange() is superseded: the client loads
                // and patches its own maps (g_MapManager tracks the facets).
            }

            break;
        }
        case 0x10: // DisplayEquipmentInfo
        {
            ushort textColor = 0x3B2;
            uint serial = ReadUInt32BE();
            CGameItem *item = g_World->FindWorldItem(serial);
            if (item == nullptr)
            {
                return true;
            }

            uint clilocNum = ReadUInt32BE();
            if (clilocNum == 0u)
            {
                return true;
            }

            (void)item;
            string str = parse_cliloc_arguments(clilocNum, true, wstr_from_bytes(Ptr, End));
            if (str.length() > 0)
            {
                // the client handler stores the property line as the item
                // name; the assistant refreshes its inspector row
                g_AssistantEvents->OnInspectorUpdate(serial);
            }

            if (str.length() != 0)
            {
                CJournalMessage message(serial, textColor, str);
                message.SetFlags(JMT_SYSTEM);
                g_AssistantEvents->OnJournalMessage(message);
            }

            ushort crafterNameLen = 0;
            uint next = ReadUInt32BE();
            if (next == 0xFFFFFFFD)
            {
                crafterNameLen = ReadUInt16BE();
                if (crafterNameLen != 0u)
                {
                    str = "Crafted by ";
                    str += ReadStringChecked(crafterNameLen).c_str();
                }
            }

            if (crafterNameLen != 0)
            {
                next = ReadUInt32BE();
            }
            if (next == 0xFFFFFFFC)
            {
                str += "[Unidentified";
            }

            uchar *end = Start + Size - 4;
            uchar count = 0;
            while (Ptr < end)
            {
                if (count != 0 || next == 0xFFFFFFFD || next == 0xFFFFFFFC)
                {
                    next = ReadInt32BE();
                }
                short charges = ReadInt16BE();
                string attrsString = parse_cliloc_arguments(
                    next, true, wstr_from_bytes(Ptr, End));
                if (charges == -1)
                {
                    if (count > 0)
                    {
                        str += "/";
                        str += attrsString;
                    }
                    else
                    {
                        str += " [";
                        str += attrsString;
                    }
                }
                else
                {
                    str += "\n[";
                    str += attrsString;
                    str += " : ";
                    str += std::to_string(charges);
                    str += "]";
                    count += 20;
                }
                count++;
            }
            if ((count < 20 && count > 0) || (next == 0xFFFFFFFC && count == 0))
            {
                str += "]";
            }

            if (str.length() != 0)
            {
                CJournalMessage message(serial, textColor, str);
                message.SetFlags(JMT_SYSTEM);
                g_AssistantEvents->OnJournalMessage(message);
            }
            break;
        }
        case 0x14: // Display Popup/context menu (2D and KR)
        {
            g_ContextMenuReceived = true;

            return g_ContextMenuManager.OnPacketReceived(*this);
        }
        case 0x18: // Enable map (diff) patches
        {
            // RESOLVED (phase 5): was g_FileManager.ApplyPatches(*this) on the
            // original side after its file transfer; the client applies the patches
            // itself in its own 0x18 handler (g_MapManager.ApplyPatches,
            // src/Managers/PacketManager.cpp:2952) - no assistant action.

            break;
        }
        default:
            break;
    }

    return true;
}

PACKET_HANDLER(SecureTradingS)
{
    if (g_World == nullptr)
    {
        return true;
    }

    return g_TradeManager.OnPacketSend(*this); // (trade round)
}

PACKET_HANDLER(SecureTradingR)
{
    if (g_World == nullptr)
    {
        return true;
    }

    g_TradeReceived = true;

    return g_TradeManager.OnPacketReceived(*this); // (trade round)
}

PACKET_HANDLER(DyeDataS)
{
    uint serial = ReadUInt32BE();
    Move(2);

    if (serial == 0xFFFFFFFF)
    {
        g_AssistantConfig.SetFontColor(true, ReadUInt16BE());
    }
    else if (serial == 0xFFFFFFFE)
    {
        g_AssistantConfig.SetCharactersFontColor(true, ReadUInt16BE());


    }





    return true;
}

PACKET_HANDLER(DyeDataR)
{
    return true;
}

PACKET_HANDLER(OpenGump)
{
    // KEEP (recv tap routes 0xB0): the gump hooks parse the command/text-line
    // stream into their own gump model and a playing macro can consume the
    // packet; the client parse builds only the UI gump.
    if (g_World == nullptr)
    {
        return true;
    }

    g_GumpReceived = true;

    return g_GumpManager.OnPacketReceived(*this); // (gump round)
}

PACKET_HANDLER(ChoiceGump)
{
    if (g_World == nullptr)
    {
        return true;
    }

    return g_GumpManager.OnPacketSend(*this); // (gump round)
}

PACKET_HANDLER(MegaClilocS)
{
    return true;
}

PACKET_HANDLER(MegaClilocR)
{
    if (g_World == nullptr)
    {
        return true;
    }

    ushort unknown = ReadUInt16BE();

    if (unknown > 1)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    Move(2);
    uint clilocRevision = ReadUInt32BE();

    string name = "";
    string message = "";

    uchar *end = Start + Size;
    vector<string> list;

    while (Ptr < end)
    {
        uint cliloc = ReadUInt32BE();

        if (cliloc == 0u)
        {
            break;
        }

        short len = ReadInt16BE();

        wstring argument;

        if (len > 0)
        {
            // bounds-checked UTF-16LE read; advances past the argument bytes
            argument = ReadWStringLE(len / 2);
        }

        string str = parse_cliloc_arguments(cliloc, true, argument);

        bool found = false;

        if (!list.empty())
        {
            for (const string &tempStr : list)
            {
                if (tempStr == str)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            list.push_back(str);
        }
    }

    bool first = true;

    if (!list.empty())
    {
        for (const string &str : list)
        {
            if (first)
            {
                name = str;
                first = false;
            }

            if (message.length() != 0)
            {
                message += "\n";
            }

            message += str;
        }
    }

    g_AssistantEvents->OnMegaCliloc(serial, clilocRevision, name, message);

    return true;
}

PACKET_HANDLER(Resend)
{
    return true;
}

PACKET_HANDLER(WalkRequest)
{
    int direction = ReadUInt8();

    // dir is a plain value copy: a reference parameter named like the outer
    // direction local shadows it (gcc -Wshadow) for no benefit
    auto getNewXY = [](const int dir, int &x, int &y) {
        switch (dir & 7)
        {
            case 0:
                y--;
                break;
            case 1:
                x++;
                y--;
                break;
            case 2:
                x++;
                break;
            case 3:
                x++;
                y++;
                break;
            case 4:
                y++;
                break;
            case 5:
                x--;
                y++;
                break;
            case 6:
                x--;
                break;
            case 7:
                x--;
                y--;
                break;
            default:
                break;
        }
    };

    // The auto-open-door step (config round + assist core):
    if (g_Player != nullptr && g_AssistantConfig.AutoOpenDoors() && !g_Player->IsDead() &&
        !g_Player->IsGM() && ((direction % 2) == 0))
    {
        int x = g_Player->GetX();
        int y = g_Player->GetY();

        getNewXY(direction, x, y);

        if ((direction & 7) == (g_Player->Direction & 7))
        {
            getNewXY(direction, x, y);
        }

        g_Assistant.AutoOpenDoor(x, y, g_Player->GetZ());
    }

    // tabmacros recording (WalkRequest): a turn (the run bit set on a
    // direction that differs from the current facing) records as a negative
    // value - the MT_WALK turn encoding.
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        if ((direction & 7) != (g_Player->Direction & 7))
        {
            direction = -(direction | 0x80);
        }

        g_MacroExecutor.Record(new CMacroWithIntValue(MT_WALK, direction));
    }

    return true;
}

PACKET_HANDLER(ConfirmWalk)
{
    if (g_World == nullptr)
    {
        return true;
    }

    if (g_AssistantConfig.ApplyNotorietyColorOnName())
    {
        // the client handler stores the notoriety byte; the assistant only
        // watches for a change to redraw its title row
        Move(1);
        uchar noto = ReadUInt8() & 0x0F;
        if (noto != g_Player->Notoriety)
        {
            g_AssistantEvents->OnTitleRedraw();
        }
    }

    return true;
}

PACKET_HANDLER(ASCIIPromptS)
{
    return g_PromptManager.OnPacketSend(*this); // (prompt round)
}

PACKET_HANDLER(ASCIIPromptR)
{
    g_PromptReceived = true;

    return g_PromptManager.OnPacketReceived(*this);
}

PACKET_HANDLER(UnicodePromptS)
{
    return g_PromptManager.OnPacketSend(*this);
}

PACKET_HANDLER(UnicodePromptR)
{
    g_PromptReceived = true;

    return g_PromptManager.OnPacketReceived(*this);
}

PACKET_HANDLER(CharacterProfileS)
{
    return true;
}

PACKET_HANDLER(CharacterProfileR)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj != nullptr)
    {
        string topText = ReadStringChecked();

        wstring bottomText = ReadWString();
        wstring dataText = ReadWString() + L"\n" + bottomText;

        obj->ProfileReceived = true;
        obj->Profile = topText + "\n" + str_from(dataText);
        bool block = obj->BlockProfile;
        obj->BlockProfile = false;

        if (block)
        {
            return false;
        }
    }

    return true;
}

PACKET_HANDLER(BuyReplyS)
{
    // was: return g_TabAgentsShop->OnPacketSendBuyReply();
    g_ShopAgent.OnBuyReplySent();

    return true;
}

PACKET_HANDLER(BuyReplyR)
{
    // was: return g_TabAgentsShop->OnPacketReceivedBuyReply(*this);
    return g_ShopAgent.OnBuyReply(*this);
}

PACKET_HANDLER(SellList)
{
    // KEEP (recv tap routes 0x9E): the sell fields map 1:1 onto the client
    // parse, but the auto-sell agent consumes the packet when it replays the
    // list itself (the client then keeps no sell gump); an in-handler hook
    // cannot preserve that consume.
    g_ShopReceived = true;

    // was: return g_TabAgentsShop->OnPacketReceivedSellList(*this); (Phase 4)
    return g_ShopAgent.OnSellList(*this);
}

PACKET_HANDLER(SellReply)
{
    // was: return g_TabAgentsShop->OnPacketSendSellReply();
    g_ShopAgent.OnSellReplySent();

    return true;
}

PACKET_HANDLER(KrriosClientSpecialS)
{
    return true;
}

PACKET_HANDLER(KrriosClientSpecialR)
{
    const uchar type = ReadUInt8();
    if (type == 1)
    {
        const int count = int(Size - 4) / 9;
        for (int i = 0; i < count; i++)
        {
            XA_PARTY_MEMBER_INFO info;
            info.Serial = ReadUInt32BE();
            info.X = ReadUInt16BE();
            info.Y = ReadUInt16BE();
            info.Map = ReadUInt8();
            g_AssistantEvents->OnPartyMemberInfo(info);
        }
    }
    else if (type == 2)
    {
        const uchar locations = ReadUInt8();
        const int bytesPerEntry = locations != 0u ? 4 + 6 : 4;
        const int count = int(Size - 4) / bytesPerEntry;
        for (int i = 0; i < count; i++)
        {
            XA_GUILD_MEMBER_INFO info;
            info.Serial = ReadUInt32BE();
            info.X = ReadUInt16BE();
            info.Y = ReadUInt16BE();
            info.Map = ReadUInt8();
            g_AssistantEvents->OnGuildMemberInfo(info);
        }
    }

    return true;
}

PACKET_HANDLER(PerformAction)
{
    uchar type = ReadUInt8();
    string str = ReadStringChecked(Size - /*4*/ 5);

    bool lastSpell = g_LastSpellIndex != 0;

    if (type == 0x24) // Skill
    {
        int index = atoi(str.c_str());

        // the conversion compares against the PREVIOUS index (the assignment
        // below) - tabmacros recording, kept verbatim
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            CMacro *macro = new CMacroWithIntValue(MT_USE_SKILL, index);

            if (index == g_LastSkillIndex)
            {
                macro->SetConvertType(MCT_LAST);
            }

            g_MacroExecutor.Record(macro);
        }

        g_LastSkillIndex = index;
    }
    else if (type == 0x56) // Spell
    {
        int index = atoi(str.c_str());

        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            CMacro *macro = new CMacroWithIntValue(MT_CAST, index);

            if (index == g_LastSpellIndex)
            {
                macro->SetConvertType(MCT_LAST);
            }

            g_MacroExecutor.Record(macro);
        }

        g_LastSpellIndex = index;
    }
    else if (type == 0x27) // Spell from book
    {
        int index = atoi(str.c_str());

        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            CMacro *macro = new CMacroWithIntValue(MT_CAST_FROM_BOOK, index);

            if (index == g_LastSpellIndex)
            {
                macro->SetConvertType(MCT_LAST);
            }

            g_MacroExecutor.Record(macro);
        }

        g_LastSpellIndex = index;
    }

    if (static_cast<int>(lastSpell) != g_LastSpellIndex)
    {
        g_AssistantEvents->OnTitleRedraw();
    }

    return true;
}

PACKET_HANDLER(DoubleClick)
{
    uint serial = ReadUInt32BE();

    // tabmacros recording (DoubleClick): the graphic/color come from the
    // world object when known; the player records as MCT_SELF
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        CGameObject *obj = g_World->FindWorldObject(serial);

        CMacro *macro = nullptr;

        if (obj != nullptr)
        {
            macro = new CMacroWithObject(MT_USE, serial, obj->Graphic, obj->Color);
        }
        else
        {
            macro = new CMacroWithObject(MT_USE, serial, 0, 0);
        }

        if (serial == g_PlayerSerial)
        {
            macro->SetConvertType(MCT_SELF);
        }

        g_MacroExecutor.Record(macro);
    }

    g_LastUseObject = serial;

    return true;
}

PACKET_HANDLER(AttackRequest)
{
    // tabmacros recording (AttackRequest): MCT_LASTATTACK / MCT_LASTTARGET
    // / MCT_ENEMY conversions, in the original's order.
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        uint serial = ReadUInt32BE();

        CGameCharacter *obj = g_World->FindWorldCharacter(serial);

        CMacro *macro = nullptr;

        if (obj != nullptr)
        {
            macro = new CMacroWithObject(MT_ATTACK, serial, obj->Graphic, obj->Color);
        }
        else
        {
            macro = new CMacroWithObject(MT_ATTACK, serial, 0, 0);
        }

        if (serial == g_LastAttackObject)
        {
            macro->SetConvertType(MCT_LASTATTACK);
        }
        else if (serial == g_LastTargetObject)
        {
            macro->SetConvertType(MCT_LASTTARGET);
        }
        else if (serial == CAssistant::TextToSerial("enemy"))
        {
            macro->SetConvertType(MCT_ENEMY);
        }

        g_MacroExecutor.Record(macro);
    }

    return true;
}

PACKET_HANDLER(HelpRequest)
{
    if (g_World != nullptr && g_MacroExecutor.IsRecording())
    {
        g_MacroExecutor.Record(new CMacro(MT_HELP_MENU_REQUEST));
    }

    return true;
}

PACKET_HANDLER(AOSCommandsS)
{
    uint serial = ReadUInt32BE();
    ushort cmd = ReadUInt16BE();

    if (cmd == 0x0019) // Combat ability
    {
        // the ability byte is read only while recording (the original read
        // nothing outside the gate - the send packet is a copy here, so the
        // position is the assistant's own cursor)
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            Move(4);
            uchar ability = ReadUInt8();

            CMacro *macro = new CMacro(MT_COMBAT_ABILITY);

            if (g_Ability[0] == ability)
            {
                macro->SetConvertType(MCT_PRIMARY);
            }
            else
            {
                macro->SetConvertType(MCT_SECONDARY);
            }

            g_MacroExecutor.Record(macro);
        }
    }
    else if (cmd == 0x0028) // Guild
    {
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(new CMacro(MT_GUILD_MENU_REQUEST));
        }
    }
    else if (cmd == 0x0032) // Quests
    {
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(new CMacro(MT_QUEST_MENU_REQUEST));
        }
    }

    (void)serial;

    return true;
}

PACKET_HANDLER(AOSCommandsR)
{
    return true;
}

PACKET_HANDLER(MapPacket)
{
    if (EnabledFeatureMapEcho())
    {
        Move(4);

        ushort Aflag = ReadUInt8();
        ushort pinID = ReadUInt8();
        ushort x = ReadUInt16BE();
        ushort y = ReadUInt16BE();

        if (Aflag != 1)
        {
            return true;
        }
        if (pinID != 0)
        {
            return true;
        }

        x *= 2;
        y *= 2;

        x += g_LastTMapX;
        y += g_LastTMapY;

        string text = "";
        {
            char buf[64] = { 0 };
            snprintf(buf, sizeof(buf), "g_Map opened to: %i:%i", x, y);
            text = buf;
        }
        g_AssistantEvents->OnClientPrint(text);

        {
            char buf[64] = { 0 };
            snprintf(buf, sizeof(buf), "mapecho: x=%i y=%i", x, y);
            text = buf;
        }
        CJournalMessage message(0xFFFFFFFF, 0x0000, text);
        message.SetFlags(JMT_SYSTEM);

        g_AssistantEvents->OnJournalMessage(message);

        // was: if (g_AssistantConfig.Tracker()) g_Assistant.Track(true, x, y)
        // (the assistant map draws the point now - see CAssistant::Track)
        if (g_AssistantConfig.Tracker())
        {
            g_AssistantEvents->OnTrack(true, x, y);
        }
    }
    return true;
}

PACKET_HANDLER(ClientMessagesS)
{
    return true;
}

PACKET_HANDLER(ClientMessagesR)
{
    // 0xFC "CrossUO messages". The client delivers its 0xFC builders here via
    // the SendToAssistant tap (src/Network/PluginPackets.cpp); server traffic
    // reaches this handler through the OnPacket tap in the client's recv path.

    ushort message = ReadUInt16BE();
    ushort application = (message >> 12);
    message &= 0x0FFF;

    if (application == 0x0000)
    {
        switch (message)
        {
            case OIPMT_SKILL_LIST:
            {
                // the skill names/buttons themselves live in the client skill
                // table; the assistant keeps its name->index lookup and the
                // hotkey action list
                int count = ReadUInt16BE();
                vector<string> hotkeyActions;
                hotkeyActions.push_back("Last");

                for (int i = 0; i < count; i++)
                {
                    uchar button = ReadUInt8();
                    string name = ReadStringChecked();

                    if (button != 0u)
                    {
                        hotkeyActions.push_back(name);
                    }

                    g_SkillManager.Insert(name, (button != 0u), i);
                }

                g_AssistantEvents->OnSkillsList(hotkeyActions);
                break;
            }
            case OIPMT_SPELL_LIST:
            {
                int count = ReadUInt16BE();

                vector<vector<string>> hotkeyActions;
                hotkeyActions.push_back({ "Last" });

                for (int i = 0; i < count; i++)
                {
                    hotkeyActions.push_back(vector<string>());

                    int spellsCount = ReadUInt16BE();

                    for (int j = 0; j < spellsCount; j++)
                    {
                        string spellName = ReadStringChecked();
                        hotkeyActions.back().push_back(spellName);
                        int offset = (i * 100) + (i > 2 ? 100 : 0);
                        g_SpellManager.Insert(spellName, j + offset + 1);
                    }
                }

                g_AssistantEvents->OnSpellList(hotkeyActions);
                break;
            }
            case OIPMT_MACRO_LIST:
            {
                int count = ReadUInt16BE();

                vector<std::pair<string, vector<string>>> macros;

                for (int i = 0; i < count; i++)
                {
                    std::pair<string, vector<string>> macro;
                    macro.first = ReadStringChecked();

                    int actionsCount = ReadUInt16BE();

                    for (int j = 0; j < actionsCount; j++)
                    {
                        macro.second.push_back(ReadStringChecked());
                    }

                    macros.push_back(macro);
                }

                g_ClientMacro.clear(); // g_ClientMacro cache (macro round
                for (const auto &macro : macros) // consumes the event payload)
                {
                    ClientMacroInsert(macro.first, macro.second);
                }

                g_AssistantEvents->OnMacroList(macros);
                break;
            }
            case OIPMT_OPEN_MAP:
            {
                // was g_TabMain->OpenMap(); the map window round consumes it
                g_AssistantEvents->OnMapOpenRequest();
                break;
            }
            default:
                break;
        }
    }
    else if (application == 0x000A)
    {
        switch (message)
        {
            case XAPMT_SET_FLAGS:
            {
                g_EnabledFeature = (uint64_t)ReadInt64BE();
                g_EnabledCommands = (uint64_t)ReadInt64BE();

                g_TextCommandManager.UpdateCommands();
                g_AssistantConfig.UpdateFeatures(); // was: g_TabMain->UpdateFeatures();

                break;
            }
            default:
                break;
        }
    }

    return true;
}

} // namespace xuoassist
