// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/packetmanager.cpp): the 256-entry dispatch
// table and every handler, decoupled from the Qt widgets (plan §4.1 "REUSE
// AS-IS (decouple UI)", §7 Phase 1 item 2).
//
// Port rules applied here (assistant-plan.md):
//  - world-model mutations (g_World/g_Player/g_Skills/assistant globals) are
//    kept verbatim - this is the packet-parity surface;
//  - widget pokes became g_AssistantEvents notifications;
//  - modules ported in later rounds (target/assist core, command/textcommand,
//    macros recording, filters, profile config, gump/menu/trade/prompt/party/
//    context-menu/corpse/spell/skill managers, UOAM map, shop agents) are
//    PORT-TODO(module) markers with the original call quoted;
//  - g_ClilocManager (client) supersedes the assistant's cliloc manager and is
//    called directly (plan §7 "What NOT to do");
//  - the per-packet LOG/LOG_DUMP lines are dropped (the client has its own
//    DEBUG/DEBUG_DUMP packet logging at the same sites).
//
// Client version: the Qt app got the host's validated version once at plugin
// install (mainscript.cpp:397); in-process the client's own g_Config
// .ClientVersion (validated from the exe at startup) is authoritative and is
// read live at each dispatch.

#include "packetmanager.h"
#include "../core/assistantevents.h"
#include "../core/packets.h"
#include "../core/clientversion.h"
#include "../model/gameobjects/gameplayer.h"
#include "../model/gameobjects/gameworld.h"
#include "../model/commonitems/skill.h"
#include "../model/commonitems/journalmessage.h"
#include <common/str.h> // str_from/wstr_from (wstr <-> utf8)
#include "Managers/ClilocManager.h" // ::g_ClilocManager (client)

#define LOGGER_MODULE Assistant
#include <common/logging/logging.h>
#include "CrossUO.h" // ::g_Game (ValueInt/ValueString/GetStaticFlags bridge)

namespace xuoassist {

CPacketManager g_PacketManager;

const int PACKET_VARIABLE_SIZE = 0;

//Карта пакетов УО для анализа
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
    /*0x11*/ RMSGH("Character Status", PACKET_VARIABLE_SIZE, CharacterStatus),
    /*0x12*/ SMSGH("Perform Action", PACKET_VARIABLE_SIZE, PerformAction),
    /*0x13*/ SMSG("Client Equip Item", 0x0a),
    /*0x14*/ SMSG("Send elevation (God client)", 0x06),
    /*0x15*/ BMSG("Follow", 0x09),
    /*0x16*/ UMSG(0x01),
    /*0x17*/
    RMSGH("Health status bar update (KR)", PACKET_VARIABLE_SIZE, NewHealthbarUpdate),
    /*0x18*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x19*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x1A*/ RMSGH("Update Item", PACKET_VARIABLE_SIZE, UpdateItem),
    /*0x1B*/ RMSGH("Enter World", 0x25, EnterWorld),
    /*0x1C*/ RMSGH("Server Talk", PACKET_VARIABLE_SIZE, Talk),
    /*0x1D*/ RMSGH("Delete Object", 0x05, DeleteObject),
    /*0x1E*/ UMSG(0x04),
    /*0x1F*/ UMSG(0x08),
    /*0x20*/ RMSGH("Update Player", 0x13, UpdatePlayer),
    /*0x21*/ RMSG("Deny Walk", 0x08),
    /*0x22*/ BMSGH("Confirm Walk", 0x03, Resend, ConfirmWalk),
    /*0x23*/ RMSG("Drag Animation", 0x1a),
    /*0x24*/ RMSGH("Open Container", 0x07, OpenContainer),
    /*0x25*/ RMSGH("Update Contained Item", 0x14, UpdateContainedItem),
    /*0x26*/ UMSG(0x05),
    /*0x27*/ RMSG("Deny Move Item", 0x02),
    /*0x28*/ UMSG(0x05),
    /*0x29*/ RMSG("Drop Item Approved", 0x01),
    /*0x2A*/ UMSG(0x05),
    /*0x2B*/ UMSG(0x02),
    /*0x2C*/ BMSGH("Death Screen", 0x02, DeathScreenS, DeathScreenR),
    /*0x2D*/ RMSGH("Mobile Attributes", 0x11, MobileAttributes),
    /*0x2E*/ RMSGH("Server Equip Item", 0x0f, EquipItem),
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
    /*0x3A*/
    BMSGH("Update Skills", PACKET_VARIABLE_SIZE, UpdateSkillsS, UpdateSkillsR),
    /*0x3B*/
    BMSGH("Vendor Buy Reply", PACKET_VARIABLE_SIZE, BuyReplyS, BuyReplyR),
    /*0x3C*/
    RMSGH("Update Contained Items", PACKET_VARIABLE_SIZE, UpdateContainedItems),
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
    /*0x4E*/ RMSGH("Personal Light Level", 0x06, PersonalLightLevel),
    /*0x4F*/ RMSGH("Global Light Level", 0x02, LightLevel),
    /*0x50*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x51*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x52*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x53*/ RMSG("Error Code", 0x02),
    /*0x54*/ RMSGH("Sound Effect", 0x0c, PlaySoundEffect),
    /*0x55*/ RMSGH("Login Complete", 0x01, LoginComplete),
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
    /*0x65*/ RMSGH("Set Weather", 0x04, SetWeather),
    /*0x66*/ BMSG("Book Page Data", PACKET_VARIABLE_SIZE),
    /*0x67*/ UMSG(0x15),
    /*0x68*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x69*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x6A*/ UMSG(0x03),
    /*0x6B*/ UMSG(0x09),
    /*0x6C*/ BMSGH("Target Data", 0x13, TargetS, TargetR),
    /*0x6D*/ RMSG("Play Music", 0x03),
    /*0x6E*/ RMSGH("Character Animation", 0x0e, CharacterAnimation),
    /*0x6F*/
    BMSGH("Secure Trading", PACKET_VARIABLE_SIZE, SecureTradingS, SecureTradingR),
    /*0x70*/ RMSGH("Graphic Effect", 0x1c, GraphicEffect),
    /*0x71*/ BMSG("Bulletin Board Data", PACKET_VARIABLE_SIZE),
    /*0x72*/ BMSGH("War Mode", 0x05, WarmodeS, WarmodeR),
    /*0x73*/ BMSG("Ping", 0x02),
    /*0x74*/ RMSGH("Vendor Buy List", PACKET_VARIABLE_SIZE, BuyList),
    /*0x75*/ SMSG("Rename Character", 0x23),
    /*0x76*/ RMSG("New Subserver", 0x10),
    /*0x77*/ RMSGH("Update Character", 0x11, UpdateCharacter),
    /*0x78*/ RMSGH("Update Object", PACKET_VARIABLE_SIZE, UpdateObject),
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
    /*0x88*/ RMSGH("Open Paperdoll", 0x42, OpenPaperdoll),
    /*0x89*/ RMSG("Corpse Equipment", PACKET_VARIABLE_SIZE),
    /*0x8A*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8B*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8C*/ RMSG("Relay Server", 0x0b),
    /*0x8D*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8E*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x8F*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x90*/ RMSGH("Display Map", 0x13, DisplayMap),
    /*0x91*/ SMSGH("Second Login", 0x41, SecondLogin),
    /*0x92*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x93*/ RMSG("Open Book", 0x63),
    /*0x94*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x95*/ BMSGH("Dye Data", 0x09, DyeDataS, DyeDataR),
    /*0x96*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0x97*/ RMSG("Move Player", 0x02),
    /*0x98*/ BMSG("All Names (3D Client Only)", PACKET_VARIABLE_SIZE),
    /*0x99*/ RMSGH("Multi Placement", 0x1a, MultiPlacement),
    /*0x9A*/
    BMSGH("ASCII Prompt", PACKET_VARIABLE_SIZE, ASCIIPromptS, ASCIIPromptR),
    /*0x9B*/ SMSGH("Help Request", 0x102, HelpRequest),
    /*0x9C*/ UMSG(0x135),
    /*0x9D*/ UMSG(0x33),
    /*0x9E*/ RMSGH("Vendor Sell List", PACKET_VARIABLE_SIZE, SellList),
    /*0x9F*/ SMSGH("Vendor Sell Reply", PACKET_VARIABLE_SIZE, SellReply),
    /*0xA0*/ SMSG("Select Server", 0x03),
    /*0xA1*/ RMSGH("Update Hitpoints", 0x09, UpdateHitpoints),
    /*0xA2*/ RMSGH("Update Mana", 0x09, UpdateMana),
    /*0xA3*/ RMSGH("Update Stamina", 0x09, UpdateStamina),
    /*0xA4*/ SMSG("System Information", 0x95),
    /*0xA5*/ RMSG("Open URL", PACKET_VARIABLE_SIZE),
    /*0xA6*/ RMSG("Tip Window", PACKET_VARIABLE_SIZE),
    /*0xA7*/ SMSG("Request Tip", 0x04),
    /*0xA8*/ RMSG("Server List", PACKET_VARIABLE_SIZE),
    /*0xA9*/ RMSG("Character List", PACKET_VARIABLE_SIZE),
    /*0xAA*/ RMSGH("Attack Reply", 0x05, AttackCharacter),
    /*0xAB*/ RMSG("Text Entry Dialog", PACKET_VARIABLE_SIZE),
    /*0xAC*/ SMSG("Text Entry Dialog Reply", PACKET_VARIABLE_SIZE),
    /*0xAD*/
    SMSGH("Unicode Client Talk", PACKET_VARIABLE_SIZE, UnicodeClientTalk),
    /*0xAE*/ RMSGH("Unicode Server Talk", PACKET_VARIABLE_SIZE, UnicodeTalk),
    /*0xAF*/ RMSGH("Display Death", 0x0d, DisplayDeath),
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
    /*0xBC*/ RMSGH("Season", 0x03, Season),
    /*0xBD*/ BMSG("Client Version", PACKET_VARIABLE_SIZE),
    /*0xBE*/ BMSG("Assist Version", PACKET_VARIABLE_SIZE),
    /*0xBF*/
    BMSGH("Extended Command", PACKET_VARIABLE_SIZE, ExtendedCommandS, ExtendedCommandR),
    /*0xC0*/ RMSGH("Graphical Effect", 0x24, GraphicEffect),
    /*0xC1*/
    RMSGH("Display cliloc String", PACKET_VARIABLE_SIZE, DisplayClilocString),
    /*0xC2*/
    BMSGH("Unicode prompt", PACKET_VARIABLE_SIZE, UnicodePromptS, UnicodePromptR),
    /*0xC3*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xC4*/ UMSG(0x06),
    /*0xC5*/ UMSG(0xcb),
    /*0xC6*/ UMSG(0x01),
    /*0xC7*/ RMSGH("Graphical Effect", 0x31, GraphicEffect),
    /*0xC8*/ BMSG("Client View Range", 0x02),
    /*0xC9*/ UMSG(0x06),
    /*0xCA*/ UMSG(0x06),
    /*0xCB*/ UMSG(0x07),
    /*0xCC*/
    RMSGH("Localized Text Plus String", PACKET_VARIABLE_SIZE, DisplayClilocString),
    /*0xCD*/ UMSG(0x01),
    /*0xCE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xCF*/ UMSG(0x4e),
    /*0xD0*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xD1*/ UMSG(0x02),
    /*0xD2*/ RMSGH("Update Character (New)", 0x19, UpdateCharacter),
    /*0xD3*/ RMSGH("Update Object (New)", PACKET_VARIABLE_SIZE, UpdateObject),
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
    /*0xDF*/ RMSGH("Buff/Debuff", PACKET_VARIABLE_SIZE, BuffDebuff),
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
    /*0xF3*/ RMSGH("Update Item (SA)", 0x18, UpdateItemSA),
    /*0xF4*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF5*/ RMSGH("Display New Map", 0x15, DisplayMap),
    /*0xF6*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF7*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xF8*/ SMSG("Character Creation (7.0.16.0)", 0x6a),
    /*0xF9*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFA*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFB*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFC*/
    BMSGH("CrossUO messages", PACKET_VARIABLE_SIZE, ClientMessagesS, ClientMessagesR),
    /*0xFE*/ UMSG(PACKET_VARIABLE_SIZE),
    /*0xFD*/ RMSG("Razor Handshake", 0x8),
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

// Wire the Phase-1a host-bridge pointers to the client implementations. The
// plugin ABI got these as IUltimaOnline function pointers; in-process the same
// client calls are bound directly (the FUNCBODY_* semantics of
// src/CommonInterfaces.cpp).
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

static void AssistantPackets_Init()
{
    static bool s_initialized = false;
    if (s_initialized)
    {
        return;
    }

    s_initialized = true;

    g_GetStaticFlags = Bridge_GetStaticFlags;
    g_GetInt = Bridge_GetValueInt;
    g_SetInt = Bridge_SetValueInt;
    g_GetString = Bridge_GetValueString;
    g_SetString = Bridge_SetValueString;
}

// The assistant's "logged in" gate: handlers only run while the assistant
// considers itself connected (the Qt app relied on the host's OnDisconnect
// callback; in-process the game stage is the disconnect signal, mirroring the
// assistant lifecycle of Phase 0 / ADR D10). Handshake packets are let through
// so the state machine can engage (EnterWorld/LoginComplete set g_Connected).
static bool AssistantPackets_GameStage()
{
    return g_GameState == GS_GAME || g_GameState == GS_GAME_BLOCKED;
}

static void AssistantPackets_CheckDisconnect()
{
    if ((g_Connected || g_World != nullptr) && !AssistantPackets_GameStage())
    {
        // Original host callback (mainscript.cpp OnClientDisconnect):
        // delete the mirror world and drop the connected flag.
        RELEASE_POINTER(g_World);
        g_Connected = false;
    }
}

void AssistantPackets_Recv(uint8_t *buf, size_t size)
{
    AssistantPackets_Init();

    AssistantPackets_CheckDisconnect();

    if (!g_Connected && *buf != 0x1B && *buf != 0x55 // EnterWorld / LoginComplete
        && *buf != 0xFC) // plugin submessages (skill/spell/macro lists are
                         // injected at plugin load, before any world exists)
    {
        return;
    }

    g_PacketManager.PacketRecv(buf, size);
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

static string to_lower(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

PACKET_HANDLER(SecondLogin)
{
    g_Connected = false;

    return true;
}

PACKET_HANDLER(LightLevel)
{
    uchar level = ReadUInt8();

    if (level > 0x1F)
    {
        level = 0x1F;
    }

    g_LightLevel = level;

    // PORT-TODO(config): return !g_TabMain->LightFilter(); (filters/config round)
    return true;
}

PACKET_HANDLER(PersonalLightLevel)
{
    uint serial = ReadUInt32BE();

    if (serial == g_PlayerSerial)
    {
        uchar level = ReadUInt8();

        if (level > 0x1F)
        {
            level = 0x1F;
        }

        g_PersonalLightLevel = level;
    }

    // PORT-TODO(config): return !g_TabMain->LightFilter();
    return true;
}

PACKET_HANDLER(LoginComplete)
{
    g_Connected = true;

    return true;
}

PACKET_HANDLER(EnterWorld)
{
    uint serial = ReadUInt32BE();
    bool runAutostart = false;

    if (g_World != nullptr)
    {
        Warning(Assistant, "Error!!! Duplicate enter world message\n");
    }
    else
    {
        runAutostart = true;
    }

    g_LastPlayerCorpsePosition = CPoint2Di(-1, -1);
    g_LastPlayerDeadTimer = 0;

    g_LastTargetObject = 0;
    g_LastAttackObject = 0;
    g_MorphGraphic = 0;
    g_OriginalGraphic = 0;
    g_LastCorpseObject = 0;
    g_LastContainerObject = 0;
    g_LastStatusRequest = 0;
    g_LastStatusObject = 0;
    g_LastUseObject = 0;
    g_TargetReceived = false;
    g_MenuReceived = false;
    g_GumpReceived = false;
    g_PromptReceived = false;
    // PORT-TODO(target): g_Target.ClearHooks(); (assist/target round)
    // PORT-TODO(macros): g_TabMacros->Reset(); (macro recording round)

    RELEASE_POINTER(g_World);

    g_World = new CGameWorld(serial);

    Move(4);

    g_Player->SetGraphic(ReadUInt16BE());
    g_OriginalGraphic = g_Player->GetGraphic();

    g_Player->SetX(ReadUInt16BE());
    g_Player->SetY(ReadUInt16BE());
    Move(1);
    g_Player->SetZ(ReadUInt8());
    g_Player->SetDirection(ReadUInt8());

    Info(Assistant, "Player 0x%08X entered the world.\n", serial);

    g_Connected = true;

    // PORT-TODO(config): g_AssistantForm->LoadProfileBySerial(serial);
    // PORT-TODO(config): if (g_Player->Dead() && g_TabMain->NoGrayScreen())
    //                       pack16(m_Start + 9, 0x03DB); g_OriginalGraphic = 0x03DB;
    // PORT-TODO(config): if (runAutostart) g_AssistantForm->RunAutostart();
    (void)runAutostart;
    // PORT-TODO(map): g_UOMapManager.UOAMEnterWorld();
    // PORT-TODO(config/map UI): if (g_MapSettings.GetShowOnLogin()) g_TabMain->OpenMap();

    // was: if (g_Map != nullptr && g_Map->isVisible()) g_Map->SetWantRedraw(true);
    g_AssistantEvents->OnMapRedraw();

    return true;
}

PACKET_HANDLER(UpdateHitpoints)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    obj->SetMaxHits(ReadInt16BE());
    obj->SetHits(ReadInt16BE());

    if (obj->IsPlayer())
    {
        g_AssistantEvents->OnTitleRedraw();
        // PORT-TODO(map): g_UOMapManager.UpdateHits();
    }

    // was: if (g_Map != nullptr && g_Map->isVisible()) g_Map->CheckForUpdate(serial);
    g_AssistantEvents->OnMapUpdate(obj->GetSerial());

    return true;
}

PACKET_HANDLER(UpdateMana)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    obj->SetMaxMana(ReadInt16BE());
    obj->SetMana(ReadInt16BE());

    if (obj->IsPlayer())
    {
        g_AssistantEvents->OnTitleRedraw();
        // PORT-TODO(map): g_UOMapManager.UpdateMana();
    }

    g_AssistantEvents->OnMapUpdate(obj->GetSerial());

    return true;
}

PACKET_HANDLER(UpdateStamina)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    obj->SetMaxStam(ReadInt16BE());
    obj->SetStam(ReadInt16BE());

    if (obj->IsPlayer())
    {
        g_AssistantEvents->OnTitleRedraw();
        // PORT-TODO(map): g_UOMapManager.UpdateStam();
    }

    g_AssistantEvents->OnMapUpdate(obj->GetSerial());

    return true;
}

PACKET_HANDLER(MobileAttributes)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    obj->SetMaxHits(ReadInt16BE());
    obj->SetHits(ReadInt16BE());

    if (obj->IsPlayer())
    {
        obj->SetMaxMana(ReadInt16BE());
        obj->SetMana(ReadInt16BE());

        obj->SetMaxStam(ReadInt16BE());
        obj->SetStam(ReadInt16BE());

        g_AssistantEvents->OnTitleRedraw();

        // PORT-TODO(map): g_UOMapManager.UpdateHits();
        // PORT-TODO(map): g_UOMapManager.UpdateMana();
        // PORT-TODO(map): g_UOMapManager.UpdateStam();
    }

    g_AssistantEvents->OnMapUpdate(obj->GetSerial());

    return true;
}

PACKET_HANDLER(NewHealthbarUpdate)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    ushort count = ReadUInt16BE();

    for (int i = 0; i < count; i++)
    {
        ushort type = ReadUInt16BE();
        uchar enable = ReadUInt8(); // enable/disable

        uchar flags = obj->GetFlags();

        if (type == 1) // Poison, enable as poisonlevel + 1
        {
            if (enable != 0u)
            {
                if (m_ClientVersion >= CV_7000)
                {
                    obj->SetSA_Poisoned(true);
                }
                else
                {
                    flags |= 0x04;
                }
            }
            else
            {
                if (m_ClientVersion >= CV_7000)
                {
                    obj->SetSA_Poisoned(false);
                }
                else
                {
                    flags &= ~0x04;
                }
            }
        }
        else if (type == 2) // Yellow hits
        {
            if (enable != 0u)
            {
                flags |= 0x08;
            }
            else
            {
                flags &= ~0x08;
            }
        }
        else if (type == 3) // Red?
        {
        }

        obj->SetFlags(flags);
    }

    if (obj->IsPlayer())
    {
        g_AssistantEvents->OnTitleRedraw();
    }

    return true;
}

PACKET_HANDLER(UpdatePlayer)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    uchar graphicIncrement = ReadUInt8();
    ushort color = ReadUInt16BE();
    uchar flags = ReadUInt8();
    ushort x = ReadUInt16BE();
    ushort y = ReadUInt16BE();
    ushort serverID = ReadUInt16BE();
    uchar direction = ReadUInt8();
    char z = ReadUInt8();

    bool oldDead = g_Player->Dead();

    g_World->UpdatePlayer(
        serial, graphic, graphicIncrement, color, flags, x, y, serverID, direction, z);

    if (g_MorphGraphic != 0u)
    {
        pack16(m_Start + 5, g_MorphGraphic);
    }

    if (g_Player->Dead())
    {
        // PORT-TODO(config): if (g_TabMain->NoGrayScreen())
        //                        pack16(m_Start + 5, 0x03DB); m_Start[7] = 0;

        if (oldDead != g_Player->Dead())
        {
            g_LastPlayerCorpsePosition = CPoint2Di(g_Player->GetX(), g_Player->GetY());
            g_LastPlayerDeadTimer = g_Ticks + 300000; // FIXME: was ticks, now ms - check
        }
    }

    return true;
}

static string parse_cliloc_arguments(int cliloc, bool toCamelCase, wstr_t args)
{
    return str_from(g_ClilocManager.ParseArgumentsToClilocString(cliloc, toCamelCase, args));
}

PACKET_HANDLER(CharacterStatus)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    obj->SetName(ReadString(30).c_str());

    obj->SetHits(ReadInt16BE());
    obj->SetMaxHits(ReadInt16BE());

    g_AssistantEvents->OnMapUpdate(obj->GetSerial());

    obj->SetCanChangeName(ReadUInt8() != 0);

    uchar flag = ReadUInt8();

    if (flag > 0)
    {
        obj->SetSex(ReadUInt8() != 0u);

        if (serial == g_PlayerSerial)
        {
            g_Player->SetStr(ReadInt16BE());
            g_Player->SetDex(ReadInt16BE());
            g_Player->SetInt(ReadInt16BE());

            g_Player->SetStam(ReadInt16BE());
            g_Player->SetMaxStam(ReadInt16BE());
            g_Player->SetMana(ReadInt16BE());
            g_Player->SetMaxMana(ReadInt16BE());
            g_Player->SetGold(ReadUInt32BE());
            g_Player->SetArmor(ReadInt16BE());
            g_Player->SetWeight(ReadUInt16BE());

            // PORT-TODO(map): g_UOMapManager.UpdateHits/Mana/Stam();

            if (flag >= 5)
            {
                g_Player->SetMaxWeight(ReadUInt16BE());
                g_Player->SetRace(RACE_TYPE(ReadUInt8()));
            }
            else
            {
                if (m_ClientVersion >= CV_500A)
                {
                    g_Player->SetMaxWeight(7 * (g_Player->GetStr() / 2) + 40);
                }
                else
                {
                    g_Player->SetMaxWeight((g_Player->GetStr() * 4) + 25);
                }
            }

            if (flag >= 3)
            {
                g_Player->SetStatsCap(ReadUInt16BE());
                g_Player->SetFollowers(ReadUInt8());
                g_Player->SetMaxFollowers(ReadUInt8());
            }

            if (flag >= 4)
            {
                g_Player->SetFireResistance(ReadInt16BE());
                g_Player->SetColdResistance(ReadInt16BE());
                g_Player->SetPoisonResistance(ReadInt16BE());
                g_Player->SetEnergyResistance(ReadInt16BE());
                g_Player->SetLuck(ReadInt16BE());
                g_Player->SetMinDamage(ReadInt16BE());
                g_Player->SetMaxDamage(ReadInt16BE());
                g_Player->SetTithingPoints(ReadUInt32BE());
            }

            g_AssistantEvents->OnSkillsRefreshed();
            g_AssistantEvents->OnTitleRedraw();
        }
    }

    if (serial == g_LastStatusRequest)
    {
        g_LastStatusRequest = 0;
        g_LastStatusObject = serial;
    }

    g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());

    return true;
}

PACKET_HANDLER(UpdateItem)
{
    if (g_World == nullptr)
    {
        return true;
    }

    UPDATE_GAME_OBJECT_TYPE updateType = UGOT_ITEM;
    uint serial = ReadUInt32BE();
    ushort count = 0;
    uchar graphicIncrement = 0;
    uchar direction = 0;
    ushort color = 0;
    uchar flags = 0;

    if ((serial & 0x80000000) != 0u)
    {
        serial &= 0x7FFFFFFF;
        count = 1;
    }

    ushort graphic = ReadUInt16BE();

    if ((graphic & 0x8000) != 0)
    {
        graphic &= 0x7FFF;
        graphicIncrement = ReadUInt8();
    }

    if (count != 0u)
    {
        count = ReadUInt16BE();
    }
    else
    {
        count++;
    }

    ushort x = ReadUInt16BE();

    if ((x & 0x8000) != 0)
    {
        x &= 0x7FFF;
        direction = 1;
    }

    ushort y = ReadUInt16BE();

    if ((y & 0x8000) != 0)
    {
        y &= 0x7FFF;
        color = 1;
    }

    if ((y & 0x4000) != 0)
    {
        y &= 0x3FFF;
        flags = 1;
    }

    if (direction != 0u)
    {
        direction = ReadUInt8();
    }

    auto z = ReadInt8();
    if (color != 0u)
    {
        color = ReadUInt16BE();
    }

    if (flags != 0u)
    {
        flags = ReadUInt8();
    }

    if (graphic >= 0x4000)
    {
        updateType = UGOT_MULTI;
    }

    if (serial != g_PlayerSerial)
    {
        g_World->UpdateGameObject(
            serial,
            graphic,
            graphicIncrement,
            count,
            x,
            y,
            z,
            direction,
            color,
            flags,
            count,
            updateType,
            1);
    }

    return true;
}

PACKET_HANDLER(UpdateItemSA)
{
    if (g_World == nullptr)
    {
        return true;
    }

    Move(2);
    auto updateType = UPDATE_GAME_OBJECT_TYPE(ReadUInt8());
    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    uchar graphicIncrement = ReadUInt8();
    ushort count = ReadUInt16BE();
    ushort unknown = ReadUInt16BE();
    ushort x = ReadUInt16BE();
    ushort y = ReadUInt16BE();
    char z = ReadInt8();
    uchar direction = ReadUInt8();
    ushort color = ReadUInt16BE();
    uchar flags = ReadUInt8();
    ushort unknown2 = ReadUInt16BE();

    g_World->UpdateGameObject(
        serial,
        graphic,
        graphicIncrement,
        count,
        x,
        y,
        z,
        direction,
        color,
        flags,
        unknown,
        updateType,
        unknown2);

    return true;
}

PACKET_HANDLER(UpdateObject)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    ushort x = ReadUInt16BE();
    ushort y = ReadUInt16BE();
    uchar z = ReadUInt8();
    uchar direction = ReadUInt8();
    ushort color = ReadUInt16BE();
    uchar flags = ReadUInt8();
    uchar notoriety = ReadUInt8();

    if (serial == g_PlayerSerial)
    {
        if (g_Player != nullptr)
        {
            bool oldDead = g_Player->Dead();

            g_Player->SetGraphic(graphic);
            g_Player->SetColor(g_World->FixColor(color, 0));
            g_Player->SetFlags(flags);

            g_OriginalGraphic = graphic;

            if (g_MorphGraphic != 0u)
            {
                pack16(m_Start + 7, g_MorphGraphic);
            }

            if (g_Player->Dead())
            {
                // PORT-TODO(config): if (g_TabMain->NoGrayScreen())
                //     g_OriginalGraphic = 0x03DB; pack16(m_Start + 7, 0x03DB);

                if (oldDead != g_Player->Dead())
                {
                    g_LastPlayerCorpsePosition = CPoint2Di(g_Player->GetX(), g_Player->GetY());
                    g_LastPlayerDeadTimer = g_Ticks + 300000; // FIXME: check ticks to ms
                }
            }
        }
    }
    else
    {
        g_World->UpdateGameObject(
            serial, graphic, 0, 0, x, y, z, direction, color, flags, 0, UGOT_ITEM, 1);
    }

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (obj == nullptr)
    {
        return true;
    }

    if (obj->GetNPC())
    {
        ((CGameCharacter *)obj)->SetNotoriety(notoriety);
        g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());

        if (((CGameCharacter *)obj)->GetMaxHits() == 0)
        {
            // PORT-TODO(assist): g_Assistant.GetStatus(obj->GetSerial()); (assist core round)
        }
    }

    if (*m_Start != 0x78)
    {
        Move(6);
    }

    uint itemSerial = ReadUInt32BE();

    while (itemSerial != 0 && !IsEOF())
    {
        ushort itemGraphic = ReadUInt16BE();
        uchar layer = ReadUInt8();
        ushort itemColor = 0;

        if (m_ClientVersion >= CV_70331)
        {
            itemColor = ReadUInt16BE();
        }
        else if ((itemGraphic & 0x8000) != 0)
        {
            itemGraphic &= 0x7FFF;
            itemColor = ReadUInt16BE();
        }

        CGameItem *item = g_World->GetWorldItem(itemSerial);

        item->SetMapIndex(g_CurrentMap);

        item->SetGraphic(itemGraphic);
        item->SetTiledataFlags(g_GetStaticFlags(itemGraphic));
        item->SetColor(g_World->FixColor(itemColor, 0));

        if (layer == OL_BACKPACK && obj->IsPlayer())
        {
            g_Backpack = itemSerial;
        }

        g_AssistantEvents->OnInspectorUpdate(item->GetSerial());
        g_World->PutEquipment(item, obj, layer);

        g_World->MoveToTop(item);

        itemSerial = ReadUInt32BE();
    }

    if (obj->IsPlayer())
    {
        g_Player->UpdateAbilities();
    }

    return true;
}

PACKET_HANDLER(EquipItem)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameItem *obj = g_World->GetWorldItem(serial);
    obj->SetMapIndex(g_CurrentMap);
    ushort graphic = ReadUInt16BE();
    obj->SetGraphic(graphic);
    obj->SetTiledataFlags(g_GetStaticFlags(graphic));
    Move(1);
    int layer = ReadUInt8();
    uint cserial = ReadUInt32BE();
    obj->SetColor(g_World->FixColor(ReadUInt16BE(), 0));

    g_World->PutEquipment(obj, cserial, layer);

    if (layer >= OL_BUY_RESTOCK && layer <= OL_SELL)
    {
        obj->Clear();
    }
    else if (layer == OL_BACKPACK && cserial == g_PlayerSerial)
    {
        g_Backpack = serial;
    }

    if (cserial == g_PlayerSerial && (layer == OL_1_HAND || layer == OL_2_HAND))
    {
        g_Player->UpdateAbilities();
    }

    g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());

    return true;
}

PACKET_HANDLER(UpdateContainedItem)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    uchar graphicIncrement = ReadUInt8();
    ushort count = ReadUInt16BE();
    ushort x = ReadUInt16BE();
    ushort y = ReadUInt16BE();

    if (m_ClientVersion >= CV_6017)
    {
        Move(1);
    }

    uint containerSerial = ReadUInt32BE();
    ushort color = ReadUInt16BE();

    CGameItem *contobj = g_World->FindWorldItem(containerSerial);

    // do not update items inside spellbook. Bug with oldstyle spellbooks.
    if (contobj != nullptr && contobj->GetGraphic() == 0x0EFA)
    {
        return true;
    }

    g_World->UpdateContainedItem(
        serial, graphic, graphicIncrement, count, x, y, containerSerial, color);

    return true;
}

PACKET_HANDLER(UpdateContainedItems)
{
    if (g_World == nullptr)
    {
        return true;
    }

    ushort itemsCount = ReadUInt16BE();

    for (int i = 0; i < itemsCount; i++)
    {
        uint serial = ReadUInt32BE();
        ushort graphic = ReadUInt16BE();
        uchar graphicIncrement = ReadUInt8();
        ushort count = ReadUInt16BE();
        ushort x = ReadUInt16BE();
        ushort y = ReadUInt16BE();

        if (m_ClientVersion >= CV_6017)
        {
            Move(1);
        }

        uint containerSerial = ReadUInt32BE();
        ushort color = ReadUInt16BE();

        if (i == 0)
        {
            CGameObject *container = g_World->FindWorldObject(containerSerial);

            if (container != nullptr)
            {
                // do not update items inside spellbook. Bug with oldstyle spellbooks.
                if (container->GetGraphic() == 0x0EFA)
                {
                    break;
                }

                if (container->IsCorpse())
                {
                    container->ClearUnequipped();
                }
                else
                {
                    container->Clear();
                }
            }
        }

        g_World->UpdateContainedItem(
            serial, graphic, graphicIncrement, count, x, y, containerSerial, color);
    }

    return true;
}

PACKET_HANDLER(DeleteObject)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    if (serial == g_PlayerSerial)
    {
        return true;
    }

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (obj != nullptr)
    {
        bool updateAbilities = false;
        CGameObject *top = obj->GetTopObject();

        if (top != nullptr && top->IsPlayer())
        {
            g_AssistantEvents->OnTitleRedraw(obj->GetSerial());

            CGameItem *item = (CGameItem *)obj;
            updateAbilities = (item->GetLayer() == OL_1_HAND || item->GetLayer() == OL_2_HAND);
        }
        else if (EnabledCommandRemovedPtrPosition())
        {
            if (obj->GetSerial() == g_LastTargetObject)
            {
                g_LastTargetPosition = CPoint2Di(obj->GetX(), obj->GetY());
            }
            else if (obj->GetSerial() == g_LastAttackObject)
            {
                g_LastAttackPosition = CPoint2Di(obj->GetX(), obj->GetY());
            }
        }

        // PORT-TODO(party): if (g_PartyManager.Contains(obj->GetSerial()))
        //                       g_PartyManager.SaveMemberInfo((CGameCharacter *)obj);
        // (agents/party round; same marker as model/gameworld.cpp)

        g_World->RemoveObject(obj);

        if (updateAbilities)
        {
            g_Player->UpdateAbilities();
        }
    }

    return true;
}

PACKET_HANDLER(UpdateCharacter)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj == nullptr)
    {
        return true;
    }

    ushort graphic = ReadUInt16BE();
    ushort x = ReadUInt16BE();
    ushort y = ReadUInt16BE();
    auto z = ReadUInt8();
    uchar direction = ReadUInt8();
    ushort color = ReadUInt16BE();
    uchar flags = ReadUInt8();
    uchar notoriety = ReadUInt8();

    obj->SetNotoriety(notoriety);

    if (obj->IsPlayer())
    {
        obj->SetFlags(flags);
    }
    else
    {
        if (obj->GetMaxHits() == 0)
        {
            // PORT-TODO(assist): g_Assistant.GetStatus(serial); (assist core round)
        }

        g_World->UpdateGameObject(
            serial, graphic, 0, 0, x, y, z, direction, color, flags, 0, UGOT_ITEM, 1);
    }

    g_World->MoveToTop(obj);

    return true;
}

PACKET_HANDLER(WarmodeS)
{
    // PORT-TODO(macros): if (g_World != nullptr && g_TabMacros->GetRecording())
    //     g_TabMacros->AddAction(new CMacroWithIntValue(MT_WAR_MODE, ReadUInt8()), false);
    // (macro recording round)

    return true;
}

PACKET_HANDLER(WarmodeR)
{
    if (g_World != nullptr)
    {
        g_Player->SetWarmode(ReadUInt8() != 0u);
        g_World->MoveToTop(g_Player);
    }

    return true;
}

PACKET_HANDLER(UpdateSkillsS)
{
    ushort id = ReadUInt16BE();

    if (id < g_SkillsCount)
    {
        g_Skills[id].SetStatus(ReadInt8());
        g_AssistantEvents->OnSkillUpdated(id);
    }

    return true;
}

PACKET_HANDLER(UpdateSkillsR)
{
    int type = ReadUInt8();
    bool haveCap = (type == 0x02 || type == 0xDF);

    if (type == 0xFE)
    {
        return true;
    }

    while (m_Ptr < m_End)
    {
        ushort id = ReadUInt16BE();

        if ((id == 0u) && (type == 0))
        {
            break;
        }
        if ((type == 0) || type == 0x02)
        {
            id--;
        }

        ushort baseVal = ReadUInt16BE();
        ushort realVal = ReadUInt16BE();
        char lock = ReadInt8();
        ushort cap = 0;

        if (haveCap)
        {
            cap = ReadUInt16BE();
        }

        if (id < g_SkillsCount)
        {
            CSkill &skill = g_Skills[id];
            skill.SetBaseValue((float)(baseVal / 10.0f));
            skill.SetValue((float)(realVal / 10.0f));
            if ((int)skill.GetOldValue() == 0)
            {
                skill.SetOldValue(skill.GetValue());
            }
            skill.SetCap((float)(cap / 10.0f));
            skill.SetStatus(lock);

            g_AssistantEvents->OnSkillUpdated(id);
        }
    }

    g_AssistantEvents->OnSkillsRefreshed();

    return true;
}

PACKET_HANDLER(TargetS)
{
    // PORT-TODO(target): the assist/target round (Phase 1c) restores:
    //   if (g_TargetHandler != nullptr) { (g_Assistant.*(g_TargetHandler))(m_Start);
    //       g_TargetHandler = nullptr; g_Target.Reset(); g_Target.RestoreTarget();
    //       return false; }
    //   return g_Target.SetSendData(*this);
    // Until then the packet is never intercepted (original behavior when no
    // target hook is armed).

    return true;
}

PACKET_HANDLER(TargetR)
{
    g_TargetReceived = true;

    // PORT-TODO(target): return g_Target.SetRecvData(*this); (assist/target round)

    return true;
}

PACKET_HANDLER(MultiPlacement)
{
    g_TargetReceived = true;

    // PORT-TODO(target): return g_Target.SetMultiData(*this); (assist/target round)

    return true;
}

PACKET_HANDLER(ClientTalkS)
{
    Move(1);
    ushort fontColor = ReadUInt16BE();
    Move(2);

    string str = ReadString(0);

    // PORT-TODO(commands): int prefixLen = g_TabHotkeys->IsCommandPrefix(str);
    //                      g_TextCommandManager.DoCommand(...) (Phase 1c)
    // PORT-TODO(map): str starting with '-' -> g_UOMapManager.UOAnotifyCmd
    // PORT-TODO(macros): macro recording of MT_SAY
    // PORT-TODO(config): g_TabMain->GetFontColor/SetFontColor + pack16(m_Start + 4, ...)

    (void)fontColor;
    (void)str;

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

        str = ReadString(0);
    }
    else
    {
        str = str_from(ReadWString());
    }

    // PORT-TODO(commands): command prefix handling (see ClientTalkS)
    // PORT-TODO(map): '-' UOA command notify
    // PORT-TODO(macros): macro recording of MT_SAY
    // PORT-TODO(config): font color replace + pack16(m_Start + 4, ...)

    (void)fontColor;

    return true;
}

PACKET_HANDLER(Talk)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    auto type = SPEECH_TYPE(ReadUInt8());
    ushort textColor = ReadUInt16BE();
    ushort font = ReadUInt16BE();

    if ((serial == 0u) && font == 0xFFFF && textColor == 0xFFFF)
    {
        return true;
    }
    if (font >= 10)
    {
        return true;
    }

    string name = ReadString(0);
    string str = "";

    if (m_Size > 44)
    {
        m_Ptr = m_Start + 44;
        str = ReadString(0);
    }

    CJournalMessage message(serial, textColor, str);

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (type == ST_BROADCAST || serial == 0xFFFFFFFF || (serial == 0u) ||
        (to_lower(name) == "system" && obj == nullptr))
    {
        message.SetFlags(JMT_SYSTEM);
    }
    else
    {
        if (obj != nullptr)
        {
            if (obj->GetName().length() == 0)
            {
                obj->SetName(name);
                g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());
            }

            if (serial == g_PlayerSerial)
            {
                message.SetFlags(JMT_MY);
            }
            else if (obj->GetName() == name && str != name)
            {
                // PORT-TODO(config): characters font color replace + pack16(m_Start + 10, ...)
            }
        }
    }

    g_AssistantEvents->OnJournalMessage(message);

    // PORT-TODO(filters): g_TabFiltersReplaces->CheckReplaceText(...) -> return false;
    // PORT-TODO(filters): g_TabFiltersSpeech->CheckSpeechInFilter(...) -> return false;

    (void)graphic;

    return true;
}

PACKET_HANDLER(UnicodeTalk)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    auto type = SPEECH_TYPE(ReadUInt8());
    ushort textColor = ReadUInt16BE();
    ushort font = ReadUInt16BE();
    uint language = ReadUInt32BE();

    if ((serial == 0u) && font == 0xFFFF && textColor == 0xFFFF)
    {
        return true;
    }

    string name = ReadString(0);
    string str = "";

    if (m_Size > 48)
    {
        m_Ptr = m_Start + 48;
        int strLen = (m_Size - 48) / 2;
        str = str_from(ReadWString(strLen).c_str());
    }

    CJournalMessage message(serial, textColor, str);

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (type == ST_GUILD_CHAT && (name.length() != 0))
    {
        // PORT-TODO(guild): g_GuildManager.SetMemberName(serial, name); (agents round)
    }

    if (type == ST_BROADCAST || serial == 0xFFFFFFFF || (serial == 0u) ||
        (to_lower(name) == "system" && obj == nullptr))
    {
        message.SetFlags(JMT_SYSTEM);
    }
    else
    {
        if (obj != nullptr)
        {
            if (obj->GetName().length() == 0)
            {
                obj->SetName(name);
                g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());
            }

            if (serial == g_PlayerSerial)
            {
                message.SetFlags(JMT_MY);
            }
            else if (obj->GetName() == name && str != name)
            {
                // PORT-TODO(config): characters font color replace + pack16(m_Start + 10, ...)
            }
        }
    }

    g_AssistantEvents->OnJournalMessage(message);

    // PORT-TODO(filters): replace/speech filters -> return false;

    (void)graphic;
    (void)language;

    return true;
}

PACKET_HANDLER(SetWeather)
{
    g_Weather.Type = ReadUInt8();
    g_Weather.Count = ReadUInt8();
    g_Weather.Temperature = ReadUInt8();

    // PORT-TODO(config): return !g_TabMain->WeatherFilter();
    return true;
}

PACKET_HANDLER(Season)
{
    g_Season.Index = ReadUInt8();
    g_Season.Music = ReadUInt8();

    // PORT-TODO(config): return !g_TabMain->SeasonFilter();
    return true;
}

PACKET_HANDLER(DisplayDeath)
{
    if (g_World == nullptr)
    {
        return true;
    }

    g_LastCorpseObject = ReadUInt32BE(4);

    // PORT-TODO(config/corpse): if (g_TabMain->CorpsesAutoopen())
    //     g_CorpseManager.Add(CCorpseInfo(g_LastCorpseObject, g_Ticks + g_TabMain->CorpseKeepDelay()));
    // (corpse auto-open round; the KeepDelay default lands with the config round)

    return true;
}

PACKET_HANDLER(OpenMenu)
{
    g_MenuReceived = true;

    // PORT-TODO(menu): return g_MenuManager.OnPacketReceived(*this); (menu round)

    return true;
}

PACKET_HANDLER(MenuChoice)
{
    // PORT-TODO(menu): return g_MenuManager.OnPacketSend(*this); (menu round)

    return true;
}

PACKET_HANDLER(DeathScreenS)
{
    return true;
}

PACKET_HANDLER(DeathScreenR)
{
    if (g_World != nullptr /*&& !ReadUInt8()*/)
    {
        // PORT-TODO(config/map): if (g_TabMain->Tracker()) g_Assistant.Track(true, -1, -1);

        g_LastPlayerCorpsePosition = CPoint2Di(g_Player->GetX(), g_Player->GetY());
        g_LastPlayerDeadTimer = g_Ticks + 300000; // FIXME: ticks to ms

        // PORT-TODO(config): if (g_TabMain->NoDeathScreen())
        //                        g_Assistant.ClientPrint("You are DEAD!"); return false;
    }

    return true;
}

PACKET_HANDLER(OpenContainer)
{
    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();

    if (graphic == 0x0030)
    {
        // PORT-TODO(shop): return g_TabAgentsShop->OnPacketReceivedOpenContaier(serial);
        // (shop agent round, Phase 4)
        return true;
    }

    g_LastContainerObject = serial;

    return true;
}

PACKET_HANDLER(BuffDebuff)
{
    if (g_World == nullptr)
    {
        return true;
    }

    const int tableCount = 126;

    static const ushort table[tableCount] = {
        0x754C, 0x754A, 0x0000, 0x0000, 0x755E, 0x7549, 0x7551, 0x7556, 0x753A, 0x754D, 0x754E,
        0x7565, 0x753B, 0x7543, 0x7544, 0x7546, 0x755C, 0x755F, 0x7566, 0x7554, 0x7540, 0x7568,
        0x754F, 0x7550, 0x7553, 0x753E, 0x755D, 0x7563, 0x7562, 0x753F, 0x7559, 0x7557, 0x754B,
        0x753D, 0x7561, 0x7558, 0x755B, 0x7560, 0x7541, 0x7545, 0x7552, 0x7569, 0x7548, 0x755A,
        0x753C, 0x7547, 0x7567, 0x7542, 0x758A, 0x758B, 0x758C, 0x758D, 0x0000, 0x758E, 0x094B,
        0x094C, 0x094D, 0x094E, 0x094F, 0x0950, 0x753E, 0x5011, 0x7590, 0x7591, 0x7592, 0x7593,
        0x7594, 0x7595, 0x7596, 0x7598, 0x7599, 0x759B, 0x759C, 0x759E, 0x759F, 0x75A0, 0x75A1,
        0x75A3, 0x75A4, 0x75A5, 0x75A6, 0x75A7, 0x75C0, 0x75C1, 0x75C2, 0x75C3, 0x75C4, 0x75F2,
        0x75F3, 0x75F4, 0x75F5, 0x75F6, 0x75F7, 0x75F8, 0x75F9, 0x75FA, 0x75FB, 0x75FC, 0x75FD,
        0x75FE, 0x75FF, 0x7600, 0x7601, 0x7602, 0x7603, 0x7604, 0x7605, 0x7606, 0x7607, 0x7608,
        0x7609, 0x760A, 0x760B, 0x760C, 0x760D, 0x760E, 0x760F, 0x7610, 0x7611, 0x7612, 0x7613,
        0x7614, 0x7615, 0x75C5, 0x75F6, 0x761B
    };

    static const string nameTable[tableCount] = { "dismount",
                                                   "disarm",
                                                   "0x0000",
                                                   "0x0000",
                                                   "night sight",
                                                   "death strike",
                                                   "evil omen",
                                                   "0x7556",
                                                   "regeneration",
                                                   "divine fury",
                                                   "enemy of one",
                                                   "hiding",
                                                   "meditation",
                                                   "blood oath caster",
                                                   "blood oath",
                                                   "corpse skin",
                                                   "mind rot",
                                                   "pain spike",
                                                   "strangle",
                                                   "gift of renewal",
                                                   "attune weapon",
                                                   "thunderstorm",
                                                   "essence of wind",
                                                   "ethereal voyage",
                                                   "gift of life",
                                                   "arcane empowerment",
                                                   "mortal strike",
                                                   "reactive armor",
                                                   "protection",
                                                   "arch protection",
                                                   "magic reflection",
                                                   "incognito",
                                                   "disguised",
                                                   "animal form",
                                                   "polymorph",
                                                   "invisibility",
                                                   "paralyze",
                                                   "poison",
                                                   "bleed",
                                                   "clumsy",
                                                   "feeblemind",
                                                   "weaken",
                                                   "curse",
                                                   "mass curse",
                                                   "agility",
                                                   "cunning",
                                                   "strength",
                                                   "bless",
                                                   "sleep",
                                                   "0x758B",
                                                   "spell plague",
                                                   "0x758D",
                                                   "0x0000",
                                                   "0x758E",
                                                   "0x094B",
                                                   "0x094C",
                                                   "0x094D",
                                                   "0x094E",
                                                   "0x094F",
                                                   "0x0950",
                                                   "0x753E",
                                                   "0x5011",
                                                   "0x7590",
                                                   "0x7591",
                                                   "0x7592",
                                                   "0x7593",
                                                   "0x7594",
                                                   "0x7595",
                                                   "0x7596",
                                                   "0x7598",
                                                   "0x7599",
                                                   "0x759B",
                                                   "0x759C",
                                                   "0x759E",
                                                   "0x759F",
                                                   "0x75A0",
                                                   "0x75A1",
                                                   "0x75A3",
                                                   "0x75A4",
                                                   "0x75A5",
                                                   "0x75A6",
                                                   "0x75A7",
                                                   "0x75C0",
                                                   "0x75C1",
                                                   "0x75C2",
                                                   "0x75C3",
                                                   "0x75C4",
                                                   "0x75F2",
                                                   "0x75F3",
                                                   "0x75F4",
                                                   "0x75F5",
                                                   "0x75F6",
                                                   "0x75F7",
                                                   "0x75F8",
                                                   "counter attack",
                                                   "0x75FA",
                                                   "0x75FB",
                                                   "0x75FC",
                                                   "0x75FD",
                                                   "0x75FE",
                                                   "0x75FF",
                                                   "0x7600",
                                                   "0x7601",
                                                   "0x7602",
                                                   "0x7603",
                                                   "0x7604",
                                                   "0x7605",
                                                   "0x7606",
                                                   "0x7607",
                                                   "0x7608",
                                                   "0x7609",
                                                   "0x760A",
                                                   "0x760B",
                                                   "0x760C",
                                                   "0x760D",
                                                   "0x760E",
                                                   "0x760F",
                                                   "0x7610",
                                                   "0x7611",
                                                   "0x7612",
                                                   "0x7613",
                                                   "0x7614",
                                                   "0x7615",
                                                   "0x75C5",
                                                   "0x75F6",
                                                   "0x761B" };

    const ushort buffIconStart = 0x03E9;

    uint serial = ReadUInt32BE();

    ushort iconID = ReadUInt16BE() - buffIconStart;

    if (iconID < tableCount && serial == g_PlayerSerial) // buff
    {
        ushort mode = ReadUInt16BE();

        if (mode != 0u)
        {
            Move(12);

            ushort timer = ReadUInt16BE();

            Move(3);

            /*uint titleCliloc =*/ReadUInt32BE();
            uint descriptionCliloc = ReadUInt32BE();
            /*uint wtfCliloc =*/ReadUInt32BE();

            Move(4);

            if (descriptionCliloc != 0u)
            {
                ReadWString(0, false);
            }

            g_AssistantEvents->OnBuffAdd(table[iconID], timer, nameTable[iconID]);
        }
        else
        {
            g_AssistantEvents->OnBuffRemove(table[iconID]);
        }
    }

    return true;
}

PACKET_HANDLER(AttackCharacter)
{
    uint serial = ReadUInt32BE();

    if (serial != 0u)
    {
        g_LastAttackObject = serial;
    }

    return true;
}

PACKET_HANDLER(ExtendedCommandS)
{
    ushort cmd = ReadUInt16BE();

    if (cmd == 0x0009) // Wrestling stun ability
    {
        // PORT-TODO(macros): recording of MT_WRESTLING_STUN
    }
    else if (cmd == 0x000A) // Wrestling disarm ability
    {
        // PORT-TODO(macros): recording of MT_WRESTLING_DISARM
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

        // PORT-TODO(macros): recording of MT_CAST_NEW (with MCT_LAST conversion)
    }
    else if (cmd == 0x0013) // Context menu request
    {
        uint serial = ReadUInt32BE();

        // PORT-TODO(macros): recording of MT_REQUEST_CONTEXT_MENU
        (void)serial;
    }
    else if (cmd == 0x0015) // Context menu selection
    {
        uint serial = ReadUInt32BE();
        ushort menuID = ReadUInt16BE();

        // PORT-TODO(macros): recording of MT_WAIT_FOR_CONTEXT_MENU
        (void)serial;
        (void)menuID;
    }
    else if (cmd == 0x0032) // Toggle gargoyle flying
    {
        // PORT-TODO(macros): recording of MT_TOGGLE_GARGOYLE_FLYING
    }

    return true;
}

PACKET_HANDLER(ExtendedCommandR)
{
    ushort cmd = ReadUInt16BE();

    switch (cmd)
    {
        case 6: // party
        {
            // PORT-TODO(party): return g_PartyManager.OnPacketReceived(*this);
            // (agents/party round, Phase 4: invites, accept/decline lists, auto-accept)
            break;
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
                // PORT-TODO(map): g_UOMapManager.UOAMnotifyLand();
                // PORT-TODO(filemanager): g_FileManager.OnMapChange(); (the client
                // loads its own map diffs; the assistant's copy is not maintained)
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

            string str = parse_cliloc_arguments(
                clilocNum, true, wstr_t((wchar_t *)m_Ptr));
            if (str.length() > 0)
            {
                item->SetName(str);
                g_AssistantEvents->OnInspectorUpdate(item->GetSerial());
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
                    str += ReadString(crafterNameLen).c_str();
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

            uchar *end = m_Start + m_Size - 4;
            uchar count = 0;
            while (m_Ptr < end)
            {
                if (count != 0 || next == 0xFFFFFFFD || next == 0xFFFFFFFC)
                {
                    next = ReadInt32BE();
                }
                short charges = ReadInt16BE();
                string attrsString = parse_cliloc_arguments(
                    next, true, wstr_t((wchar_t *)m_Ptr));
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

            // PORT-TODO(contextmenu): return g_ContextMenuManager.OnPacketReceived(*this);

            break;
        }
        case 0x18: // Enable map (diff) patches
        {
            // PORT-TODO(filemanager): g_FileManager.ApplyPatches(*this); (see case 8)

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

    // PORT-TODO(trade): return g_TradeManager.OnPacketSend(*this); (trade round)

    return true;
}

PACKET_HANDLER(SecureTradingR)
{
    if (g_World == nullptr)
    {
        return true;
    }

    g_TradeReceived = true;

    // PORT-TODO(trade): return g_TradeManager.OnPacketReceived(*this); (trade round)

    return true;
}

PACKET_HANDLER(DyeDataS)
{
    uint serial = ReadUInt32BE();
    Move(2);

    if (serial == 0xFFFFFFFF)
    {
        // PORT-TODO(config): g_TabMain->SetFontColor(true, ReadUInt16BE());
    }
    else if (serial == 0xFFFFFFFE)
    {
        // PORT-TODO(config): g_TabMain->SetCharactersFontColor(true, ReadUInt16BE());
    }

    return true;
}

PACKET_HANDLER(DyeDataR)
{
    return true;
}

PACKET_HANDLER(OpenGump)
{
    if (g_World == nullptr)
    {
        return true;
    }

    g_GumpReceived = true;

    // PORT-TODO(gump): return g_GumpManager.OnPacketReceived(*this); (gump round)

    return true;
}

PACKET_HANDLER(ChoiceGump)
{
    if (g_World == nullptr)
    {
        return true;
    }

    // PORT-TODO(gump): return g_GumpManager.OnPacketSend(*this); (gump round)

    return true;
}

PACKET_HANDLER(PlaySoundEffect)
{
    if (g_World == nullptr)
    {
        return true;
    }

    Move(1);
    ushort index = ReadUInt16BE();
    ushort volume = ReadUInt16BE();
    ushort x = ReadUInt16BE();
    ushort y = ReadUInt16BE();

    // PORT-TODO(config): sound echo journal message when g_TabMain->SoundEcho()
    // PORT-TODO(filters): return !g_TabFiltersSound->CheckSoundInFilter(index);
    (void)index;
    (void)volume;
    (void)x;
    (void)y;

    return true;
}

PACKET_HANDLER(GraphicEffect)
{
    if (g_World == nullptr)
    {
        return true;
    }

    // PORT-TODO(config): the whole effect echo journal message block, gated by
    // g_TabMain->EffectEcho() (config round): parse type 0..3 fields, journal
    // "effectecho: ..." system message.

    return true;
}

PACKET_HANDLER(CharacterAnimation)
{
    if (g_World == nullptr)
    {
        return true;
    }

    // PORT-TODO(config): animation echo journal message block, gated by
    // g_TabMain->AnimationEcho() (config round).

    return true;
}

PACKET_HANDLER(DisplayMap)
{
    if (g_World == nullptr)
    {
        return true;
    }

    if (EnabledFeatureMapEcho())
    {
        Move(6);

        ushort startX = ReadUInt16BE();
        ushort startY = ReadUInt16BE();
        // ushort endX = ReadUInt16BE();
        // ushort endY = ReadUInt16BE();

        if (*m_Start != 0xF5)
        {
            g_LastTMapX = startX;
            g_LastTMapY = startY;
        }
    }

    return true;
}

PACKET_HANDLER(DisplayClilocString)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();
    ushort graphic = ReadUInt16BE();
    uchar type = ReadUInt8();
    ushort color = ReadUInt16BE();
    ushort font = (uchar)ReadUInt16BE();
    uint cliloc = ReadUInt32BE();

    uchar flags = 0;

    if (*m_Start == 0xCC)
    {
        flags = ReadUInt8();
    }

    string name = ReadString(30);

    if (*m_Start == 0xCC)
    {
        // (affix is parsed for the pointer side effect, like the original)
        string affix = ReadString(0);
        (void)affix;
    }

    string str = parse_cliloc_arguments(cliloc, false, wstr_t((wchar_t *)m_Ptr));

    CJournalMessage message(serial, color, str);

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (/*type == ST_BROADCAST ||*/ serial == 0xFFFFFFFF || (serial == 0u) ||
        (to_lower(name) == "system" && obj == nullptr))
    {
        message.SetFlags(JMT_SYSTEM);
    }
    else
    {
        if (obj != nullptr)
        {
            if (obj->GetName().length() == 0)
            {
                obj->SetName(name);
            }

            if (serial == g_PlayerSerial)
            {
                message.SetFlags(JMT_MY);
            }
            else if (obj->GetName() == name && str != name)
            {
                // PORT-TODO(config): characters font color replace + pack16(m_Start + 10, ...)
            }
        }
    }

    g_AssistantEvents->OnJournalMessage(message);

    // PORT-TODO(filters): replace/speech filters -> return false;

    (void)flags;
    (void)graphic;
    (void)type;
    (void)font;

    return true;
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

    uchar *end = m_Start + m_Size;
    vector<string> list;

    while (m_Ptr < end)
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
            argument = wstring((wchar_t *)m_Ptr, len / 2);
            m_Ptr += len;
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

    CGameObject *obj = g_World->FindWorldObject(serial);

    if (obj != nullptr && !obj->GetNPC())
    {
        obj->SetName(name);
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

    // PORT-TODO(config/assist): the auto-open-door step
    // (g_TabMain->AutoOpenDoors() gate, g_AssistantForm->AutoOpenDoor(x, y, z))
    // lands with the assist-core/config round; the getNewXY walk math moves
    // with it.
    // PORT-TODO(macros): walk recording (MT_WALK with the run-bit conversion).

    (void)direction;

    return true;
}

PACKET_HANDLER(ConfirmWalk)
{
    if (g_World == nullptr)
    {
        return true;
    }

    // PORT-TODO(config): if (g_TabDisplay->ApplyNotorietyColorOnName())
    // {
    //     Move(1);
    //     uchar noto = ReadUInt8() & 0x0F;
    //     if (noto != g_Player->GetNotoriety())
    //     {
    //         g_Player->SetNotoriety(noto);
    //         g_AssistantEvents->OnTitleRedraw();
    //     }
    // }
    // (config round restores the notoriety-on-name tracking; default off, the
    // packet is left untouched until then)

    return true;
}

PACKET_HANDLER(ASCIIPromptS)
{
    // PORT-TODO(prompt): return g_PromptManager.OnPacketSend(*this); (prompt round)

    return true;
}

PACKET_HANDLER(ASCIIPromptR)
{
    g_PromptReceived = true;

    // PORT-TODO(prompt): return g_PromptManager.OnPacketReceived(*this);

    return true;
}

PACKET_HANDLER(UnicodePromptS)
{
    // PORT-TODO(prompt): return g_PromptManager.OnPacketSend(*this);

    return true;
}

PACKET_HANDLER(UnicodePromptR)
{
    g_PromptReceived = true;

    // PORT-TODO(prompt): return g_PromptManager.OnPacketReceived(*this);

    return true;
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
        string topText = ReadString();

        wstring bottomText = ReadWString();
        wstring dataText = ReadWString() + L"\n" + bottomText;

        obj->SetProfileReceived(true);
        obj->SetProfile(topText + "\n" + str_from(dataText));
        bool block = obj->GetBlockProfile();
        obj->SetBlockProfile(false);

        if (block)
        {
            return false;
        }
    }

    return true;
}

PACKET_HANDLER(OpenPaperdoll)
{
    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = ReadUInt32BE();

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    if (obj != nullptr)
    {
        obj->SetTitle(ReadString(60).c_str());
    }

    return true;
}

PACKET_HANDLER(BuyList)
{
    g_ShopReceived = true;

    // PORT-TODO(shop): return g_TabAgentsShop->OnPacketReceivedBuyList(*this); (Phase 4)

    return true;
}

PACKET_HANDLER(BuyReplyS)
{
    // PORT-TODO(shop): return g_TabAgentsShop->OnPacketSendBuyReply();

    return true;
}

PACKET_HANDLER(BuyReplyR)
{
    // PORT-TODO(shop): return g_TabAgentsShop->OnPacketReceivedBuyReply(*this);

    return true;
}

PACKET_HANDLER(SellList)
{
    g_ShopReceived = true;

    // PORT-TODO(shop): return g_TabAgentsShop->OnPacketReceivedSellList(*this);

    return true;
}

PACKET_HANDLER(SellReply)
{
    // PORT-TODO(shop): return g_TabAgentsShop->OnPacketSendSellReply();

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
        const int count = int(m_Size - 4) / 9;
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
        const int count = int(m_Size - 4) / bytesPerEntry;
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
    string str = ReadString(m_Size - /*4*/ 5);

    bool lastSpell = g_LastSpellIndex != 0;

    if (type == 0x24) // Skill
    {
        int index = atoi(str.c_str());

        // PORT-TODO(macros): recording of MT_USE_SKILL (with MCT_LAST conversion)

        g_LastSkillIndex = index;
    }
    else if (type == 0x56) // Spell
    {
        int index = atoi(str.c_str());

        // PORT-TODO(macros): recording of MT_CAST (with MCT_LAST conversion)

        g_LastSpellIndex = index;
    }
    else if (type == 0x27) // Spell from book
    {
        int index = atoi(str.c_str());

        // PORT-TODO(macros): recording of MT_CAST_FROM_BOOK (with MCT_LAST conversion)

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

    // PORT-TODO(macros): MT_USE recording (with MCT_SELF conversion).

    g_LastUseObject = serial;

    return true;
}

PACKET_HANDLER(AttackRequest)
{
    // PORT-TODO(macros): MT_ATTACK recording (MCT_LASTATTACK/MCT_LASTTARGET/
    // MCT_ENEMY conversions; CAssistant::TextToSerial is in core/textconv).

    return true;
}

PACKET_HANDLER(HelpRequest)
{
    // PORT-TODO(macros): MT_HELP_MENU_REQUEST recording.

    return true;
}

PACKET_HANDLER(AOSCommandsS)
{
    uint serial = ReadUInt32BE();
    ushort cmd = ReadUInt16BE();

    if (cmd == 0x0019) // Combat ability
    {
        // PORT-TODO(macros): MT_COMBAT_ABILITY recording (MCT_PRIMARY/SECONDARY);
        // the original only did Move(4) + ReadUInt8() inside the recording gate.
    }
    else if (cmd == 0x0028) // Guild
    {
        // PORT-TODO(macros): MT_GUILD_MENU_REQUEST recording.
    }
    else if (cmd == 0x0032) // Quests
    {
        // PORT-TODO(macros): MT_QUEST_MENU_REQUEST recording.
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

        // PORT-TODO(config/map): if (g_TabMain->Tracker()) g_Assistant.Track(true, x, y);
    }
    return true;
}

PACKET_HANDLER(ClientMessagesS)
{
    return true;
}

PACKET_HANDLER(ClientMessagesR)
{
    // 0xFC "CrossUO messages". In the external-plugin days the client pushed
    // its CPluginPacket buffers to plugins via g_PluginManager.PacketRecv
    // (src/Network/PluginPackets.cpp SendToPlugin); the assistant now sits on
    // the same two paths (OnPacket tap + SendToPlugin tap, see the src/ taps)
    // so this handler sees exactly the submessages the plugin ABI saw.

    ushort message = ReadUInt16BE();
    ushort application = (message >> 12);
    message &= 0x0FFF;

    if (application == 0x0000)
    {
        switch (message)
        {
            case OIPMT_SKILL_LIST:
            {
                // was: g_TabSkills->SkillsListUpdated(*this) (parse below) plus a
                // hotkey action-list update (UI round consumes the event).
                g_SkillsCount = ReadUInt16BE();
                vector<string> hotkeyActions;
                hotkeyActions.push_back("Last");

                for (int i = 0; i < g_SkillsCount; i++)
                {
                    uchar button = ReadUInt8();
                    string name = ReadString();

                    if (button != 0u)
                    {
                        hotkeyActions.push_back(name);
                    }

                    g_Skills[i].SetButton(button != 0u);
                    g_Skills[i].SetName(name);
                }

                // PORT-TODO(skillmanager): g_SkillManager.Insert(name, button, i);

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
                        string spellName = ReadString();
                        hotkeyActions.back().push_back(spellName);
                        // PORT-TODO(spells): g_SpellManager.Insert(spellName, j + offset + 1)
                        // (spellbook circle offset: i * 100, +100 for i > 2)
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
                    macro.first = ReadString();

                    int actionsCount = ReadUInt16BE();

                    for (int j = 0; j < actionsCount; j++)
                    {
                        macro.second.push_back(ReadString());
                    }

                    macros.push_back(macro);
                }

                // PORT-TODO(macros): g_ClientMacro cache (macro round consumes
                // the event payload).

                g_AssistantEvents->OnMacroList(macros);
                break;
            }
            case OIPMT_OPEN_MAP:
            {
                // PORT-TODO(map UI): g_TabMain->OpenMap(); (map window round)
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

                // PORT-TODO(commands): g_TextCommandManager.UpdateCommands();
                // PORT-TODO(config UI): g_TabMain->UpdateFeatures();

                break;
            }
            default:
                break;
        }
    }

    return true;
}

} // namespace xuoassist
