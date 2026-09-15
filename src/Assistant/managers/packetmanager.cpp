// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "packetmanager.h"
#include "../core/assistantevents.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include "../core/clientversion.h"
#include "../core/target.h"
#include "../core/textcommandmanager.h"
#include "../config/assistantconfig.h"
#include "../managers/assmap.h" // g_MapSettings (ShowOnLogin)
#include "../managers/clientmacromanager.h"
#include "../managers/contextmenumanager.h"
#include "../managers/guildmanager.h"
#include "../managers/skillmanager.h"
#include "../managers/spellmanager.h"
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
#define BMSG(name, size)                                                                           \
    {                                                                                              \
        name, size, DIR_BOTH, nullptr, nullptr                                                     \
    }
CPacketInfo CPacketManager::m_Packets[0x100] = {
    /*0x00*/ SMSG("Create Character", 0x68),
    /*0x01*/ SMSG("Disconnect", 0x05),
    /*0x02*/ SMSG("Walk Request", 0x07),
    /*0x03*/
    SMSG("Client Talk", PACKET_VARIABLE_SIZE),
    /*0x04*/ SMSG("Request God mode (God client)", 0x02),
    /*0x05*/ SMSG("Attack", 0x05),
    /*0x06*/ SMSG("Double Click", 0x05),
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
    /*0x12*/ SMSG("Perform Action", PACKET_VARIABLE_SIZE),
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
    /*0x22*/ SMSG("Confirm Walk", 0x03),
    /*0x23*/ RMSG("Drag Animation", 0x1a),
    /*0x24*/ UMSG(0x07),
    /*0x25*/ RMSG("Update Contained Item", 0x14),
    /*0x26*/ UMSG(0x05),
    /*0x27*/ RMSG("Deny Move Item", 0x02),
    /*0x28*/ UMSG(0x05),
    /*0x29*/ RMSG("Drop Item Approved", 0x01),
    /*0x2A*/ UMSG(0x05),
    /*0x2B*/ UMSG(0x02),
    /*0x2C*/ UMSG(0x02),
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
    // the 0x3B send tap moved to CGumpShop::SendList (shop agent replay flag)
    /*0x3B*/ SMSG("Vendor Buy Reply", PACKET_VARIABLE_SIZE),
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
    // plain row: the mapecho/tracker work lives fused in the client MapData
    // handler, gated on EnabledFeatureMapEcho
    /*0x56*/ RMSG("Map Data", 0x0b),
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
    /*0x6C*/ SMSG("Target Data", 0x13),
    /*0x6D*/ RMSG("Play Music", 0x03),
    /*0x6E*/ UMSG(0x0e),
    // both halves own no assistant code: the send tap moved to the client's
    // trade gumps, the recv half fused into the client's 0x6F handler
    /*0x6F*/ BMSG("Secure Trading", PACKET_VARIABLE_SIZE),
    /*0x70*/ UMSG(0x1c),
    /*0x71*/ BMSG("Bulletin Board Data", PACKET_VARIABLE_SIZE),
    /*0x72*/ SMSG("War Mode", 0x05),
    /*0x73*/ BMSG("Ping", 0x02),
    /*0x74*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x75*/ SMSG("Rename Character", 0x23),
    /*0x76*/ RMSG("New Subserver", 0x10),
    /*0x77*/ UMSG(0x11),
    /*0x78*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x79*/ UMSG(0x09),
    /*0x7A*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x7B*/ UMSG(0x02),
    /*0x7C*/ UMSG(PACKET_VARIABLE_SIZE),
    // the 0x7D send tap moved to the client's menu-choice send sites
    /*0x7D*/ SMSG("Menu Choice", 0x0d),
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
    /*0x91*/
    // (fusion closeout) the old SecondLogin handler only reset g_Connected; 0x91
    // is sent once per game-server session by CGame::RelayServer during the
    // login relay, before the game stage is entered, where
    // AssistantPackets_CheckDisconnect has already cleared g_Connected - the
    // reset was a no-op. Row kept plain: the integrity guard below counts 256
    // named rows.
    SMSG("Second Login", 0x41),
    /*0x92*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x93*/ RMSG("Open Book", 0x63),
    /*0x94*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x95*/ SMSG("Dye Data", 0x09),
    /*0x96*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x97*/ RMSG("Move Player", 0x02),
    /*0x98*/ BMSG("All Names (3D Client Only)", PACKET_VARIABLE_SIZE),
    /*0x99*/ UMSG(0x1a),
    // the send taps moved to the client's prompt-response send sites; the
    // rows stay both-direction (the server sends prompts, fused recv-side)
    /*0x9A*/ BMSG("ASCII Prompt", PACKET_VARIABLE_SIZE),
    /*0x9B*/ SMSG("Help Request", 0x102),
    /*0x9C*/ UMSG(0x135),
    /*0x9D*/ UMSG(0x33),
    /*0x9E*/ UMSG(PACKET_VARIABLE_SIZE),
    // the 0x9F send tap moved to CGumpShop::SendList (shop agent replay flag)
    /*0x9F*/ SMSG("Vendor Sell Reply", PACKET_VARIABLE_SIZE),
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
    SMSG("Unicode Client Talk", PACKET_VARIABLE_SIZE),
    /*0xAE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xAF*/ UMSG(0x0d),
    /*0xB0*/ UMSG(PACKET_VARIABLE_SIZE),
    // the 0xB1 send tap moved to the client's gump-choice send sites
    /*0xB1*/ SMSG("Gump Choice", PACKET_VARIABLE_SIZE),
    /*0xB2*/ BMSG("Chat Data", PACKET_VARIABLE_SIZE),
    /*0xB3*/ RMSG("Chat Text ?", PACKET_VARIABLE_SIZE),
    /*0xB4*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xB5*/ BMSG("Open Chat Window", 0x40),
    /*0xB6*/ SMSG("Popup Help Request", 0x09),
    /*0xB7*/ RMSG("Popup Help Data", PACKET_VARIABLE_SIZE),
    /*0xB8*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xB9*/ RMSG("Enable locked client features", 0x03),
    /*0xBA*/ RMSG("Display Quest Arrow", 0x06),
    /*0xBB*/ SMSG("Account ID ?", 0x09),
    /*0xBC*/ UMSG(0x03),
    /*0xBD*/ BMSG("Client Version", PACKET_VARIABLE_SIZE),
    /*0xBE*/ BMSG("Assist Version", PACKET_VARIABLE_SIZE),
    /*0xBF*/
    // both halves left the table: the recv half is fused into the client's
    // ExtendedCommand handler (facet map, equipment-info journal,
    // context-menu round; 0x18 is a no-op) and the send subcommands are
    // recorded at their client call sites - the server sends 0xBF constantly
    // (party chat), so the row stays DIR_BOTH instead of a
    // warning-per-packet DIR_SEND
    BMSG("Extended Command", PACKET_VARIABLE_SIZE),
    /*0xC0*/ UMSG(0x24),
    /*0xC1*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xC2*/ BMSG("Unicode prompt", PACKET_VARIABLE_SIZE),
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
    /*0xD6*/ BMSG("Mega cliloc", PACKET_VARIABLE_SIZE),
    /*0xD7*/
    // the 0xD7 send tap moved to the client's call sites (combat ability,
    // guild/quest menu requests)
    UMSG(PACKET_VARIABLE_SIZE),
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
    /*0xF0*/ BMSG("Krrios client special", PACKET_VARIABLE_SIZE),
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
    /*0xFC*/ BMSG("CrossUO messages", PACKET_VARIABLE_SIZE),
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

    ClientVersion = GetClientVersion();

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

    ClientVersion = GetClientVersion();

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
// result is inverted here. The current producers of "consumed" are the fused
// WaitTarget hook paths in the client handlers (0x6C Target and 0x99
// MultiPlacement via CTarget::SetRecvData/SetMultiData -> CheckHook): the
// auto-target swallows the reply so the client keeps no open cursor.
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

    if (!g_Connected)
    {
        return;
    }

    g_PacketManager.PacketSend(buf, size);
}

// parse_cliloc_arguments was only used by the fused 0xBF equipment-info
// half; the client calls g_ClilocManager.ParseArgumentsToClilocString
// directly in the injected block.

// The fused 0x03 ClientTalkS / 0xAD UnicodeClientTalk send taps (the two
// handlers differed only in text extraction from the wire; the 0xAD row
// decoded the encoded-command form - the keyword-code blob preceding the
// UTF-8 text - to recover the typed text, which the send sites already
// hold). Called at each client speech send site before the packet build;
// the packet is then built from the (possibly color-adjusted) values.
bool AssistantPackets_SpeechIntercept(astr_t &text, uint16_t &color)
{
    // Command prefix: run the assistant command and suppress the send. The
    // old taps returned "consumed" into a void tap entry, so a prefixed line
    // both ran the command and went out as raw speech; the client-site early
    // return restores the interception's purpose.
    int prefixLen = g_AssistantConfig.IsCommandPrefix(text);
    if (prefixLen != 0)
    {
        g_TextCommandManager.DoCommand(text.substr(prefixLen));
        return false;
    }
    // DEVIATION: a leading '-' went to g_UOMapManager.UOAnotifyCmd
    // - the external UOA/UOAM command pipe (Windows WM_USER + atoms); there
    // is no in-process equivalent, the command stays a normal speech line.

    g_MacroExecutor.RecordSay(text);

    ushort replaceFontColorValue = 0;
    bool replaceFontColor = g_AssistantConfig.GetFontColor(replaceFontColorValue);

    if (!replaceFontColor && replaceFontColorValue != color)
    {
        g_AssistantConfig.SetFontColor(false, color);
    }

    if (replaceFontColor)
    {
        color = replaceFontColorValue;
    }

    return true;
}

} // namespace xuoassist
