// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// AssistantEvents: the notification seam introduced at Phase 1 (master plan
// §7 Phase 1 item 2 / §8.1 "Packet-manager UI coupling"). The original app's packet
// handlers poked widget globals directly (g_TabMain, g_TabSkills, g_Map,
// g_ObjectInspectorForm, g_HotkeysForm, ...); in the client build those call
// sites become g_AssistantEvents->OnXxx(...) notifications. Anything that is
// pure world state stays in the model/managers; anything a UI (or a later
// round's default sink) consumes flows through here.
//
// Design choice (one of the two allowed by the plan round): a class with
// virtual methods and ONE replaceable global instance pointer. The default
// instance's methods are inert (except OnClientPrint, whose original behavior
// is pure client-side packet injection and is reproduced verbatim). A later UI
// round installs a subclass instance into g_AssistantEvents; handlers never
// change. No UI-framework coupling, no std::function registry, C++14.

#pragma once

#include "global.h"
#include "../model/commonitems/journalmessage.h"

namespace xuoassist {

// assistantevents.h OnFromTarget kind: which tab requested the target cursor
enum
{
    XA_FROM_TARGET_TYPE = 0,       // TabListsTypes (graphic of the target)
    XA_FROM_TARGET_OBJECT,         // TabListsObjects (serial)
    XA_FROM_TARGET_FIND_LIST,      // TabListsFind content (graphic+color)
    XA_FROM_TARGET_IGNORE_LIST,    // TabListsIgnore content (graphic+color)
    XA_FROM_TARGET_FRIEND,         // TabListsFriends (serial)
    XA_FROM_TARGET_ENEMY,          // TabListsEnemies (serial)
    XA_FROM_TARGET_ANIMAL_NAME,    // AnimalAndVendorControlForm "Name" field
    XA_FROM_TARGET_PARTY,          // TabAgentsParty
    XA_FROM_TARGET_DRESS_ITEM,     // TabAgentsDress "Add item to set from target"
    XA_FROM_TARGET_DISPLAY_ITEM    // TabDisplay items editor (graphic + hue)
};

// Forward-ported from the original app's managers/journalmanager.h (the manager
// itself lands with the journal round; the flags travel with the messages).
enum XA_JOURNAL_MESSAGE_FLAGS
{
    JMT_NORMAL = 0x00,
    JMT_MY = 0x01,
    JMT_SYSTEM = 0x02
};

// Forward-ported from the original app's managers/partymanager.h and
// managers/guildmanager.h (the managers land with the agents round; the packet
// handler parses these today and hands them over as-is). Name is
// std::string per §4.2.
struct XA_PARTY_MEMBER_INFO
{
    uint Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint Map{ 0 };
    string Name{ "" };
};

struct XA_GUILD_MEMBER_INFO
{
    uint Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint Map{ 0 };
    string Name{ "" };
};

class CAssistantEvents
{
public:
    virtual ~CAssistantEvents();

    // --- speech / journal (was g_JournalManager.Add) -----------------------
    // The assistant's own journal cache is fed elsewhere; the UI subscribes
    // for its journal tab. Default: nothing (client adds its own entries).
    virtual void OnJournalMessage(const CJournalMessage &message) { (void)message; }

    // Was CAssistant::ClientPrint: inject a system print into the client.
    // Default reproduces the original packet injection exactly.
    virtual void OnClientPrint(const string &text, ushort color = g_MessagesColor);

    // --- buffs (was g_BuffManager.Add/Remove) ------------------------------
    virtual void OnBuffAdd(ushort graphic, uint timer, const string &name)
    {
        (void)graphic;
        (void)timer;
        (void)name;
    }
    virtual void OnBuffRemove(ushort graphic) { (void)graphic; }

    // --- corpses (was g_CorpseManager.Add; auto-open bookkeeping) ----------
    // expireTicks is an absolute g_Ticks stamp.
    virtual void OnCorpseAdd(uint serial, uint expireTicks)
    {
        (void)serial;
        (void)expireTicks;
    }

    // --- party / guild member positions (Krrios special packet) ------------
    virtual void OnPartyMemberInfo(const XA_PARTY_MEMBER_INFO &info) { (void)info; }
    virtual void OnGuildMemberInfo(const XA_GUILD_MEMBER_INFO &info) { (void)info; }

    // --- object properties (was g_ObjectPropertiesManager.Add) -------------
    virtual void OnMegaCliloc(
        uint serial,
        uint revision,
        const string &name,
        const string &properties)
    {
        (void)serial;
        (void)revision;
        (void)name;
        (void)properties;
    }

    // --- skills (was g_TabSkills->UpdateSkill/UpdateStatsAndSkillsInfo and
    //     the 0xFC OIPMT_SKILL_LIST hotkey-action feed) ----------------------
    virtual void OnSkillUpdated(int id) { (void)id; }
    virtual void OnSkillsRefreshed() {}
    // "Last" + every use-button skill name, in packet order.
    virtual void OnSkillsList(const vector<string> &hotkeyActions) { (void)hotkeyActions; }

    // --- spells / macros from the client's plugin packets (0xFC) -----------
    // One entry per spellbook (circles), spell names in packet order.
    virtual void OnSpellList(const vector<vector<string>> &books) { (void)books; }
    virtual void OnMacroList(const vector<std::pair<string, vector<string>>> &macros)
    {
        (void)macros;
    }

    // --- macros (was g_TabMacros->AddAction while a recording was open) ----------
    // graphic/color are 0 when the targeted object is unknown to the world.
    virtual void OnRecordTargetObject(uint serial, ushort graphic, ushort color)
    {
        (void)serial;
        (void)graphic;
        (void)color;
    }
    virtual void OnRecordTargetTile(ushort graphic, int x, int y, int z)
    {
        (void)graphic;
        (void)x;
        (void)y;
        (void)z;
    }

    // Was g_AssistantForm->RunAutostart from the
    // EnterWorld handler: the sink checks the Autostart option and starts
    // the "Autostart" script from the editor buffer after 1 second.
    virtual void OnAutostartRequest() {}

    // The closeuo command (was SendMessage(hwnd, WM_CLOSE)): the sink asks
    // the client window to close (CWindow::Destroy -> the SDL_QUIT path).
    virtual void OnClientCloseRequest() {}

    // --- from-target requests (the tablists "from target" buttons; the
    //     HandleTarget* stubs in core/assist.cpp forward the 0x6C reply) ----
    // XA_FROM_TARGET_* identifies which tab asked; name is the object name
    // when known (AnimalVendorName requests), "" otherwise.
    virtual void OnFromTarget(int kind, uint serial, ushort graphic, ushort color, const string &name)
    {
        (void)kind;
        (void)serial;
        (void)graphic;
        (void)color;
        (void)name;
    }

    // --- object inspector (was g_ObjectInspectorForm->CheckUpdate(obj)) -----
    virtual void OnInspectorUpdate(uint serial) { (void)serial; }

    // --- display tab title bar (was g_TabDisplay->RedrawTitle /
    //     CheckRedrawTitle(obj)) ---------------------------------------------
    virtual void OnTitleRedraw() {}
    virtual void OnTitleRedraw(uint serial) { (void)serial; }

    // --- map window (was g_Map->CheckForUpdate / SetWantRedraw) -------------
    virtual void OnMapUpdate(uint serial) { (void)serial; }
    virtual void OnMapRedraw() {}
    // Was g_TabMain->OpenMap: the OIPMT_OPEN_MAP app packet, the MapSettings
    // "Show on log in" flag and the hotkey launchers.
    virtual void OnMapOpenRequest() {}

    // --- tracker (was CAssistant::Track; map-window round consumes) ---------
    virtual void OnTrack(bool state, int x, int y)
    {
        (void)state;
        (void)x;
        (void)y;
    }
};

// The installed sink. Never null: assistantevents.cpp installs the default
// instance at static-init; a UI round replaces (and later restores) it.
extern CAssistantEvents *g_AssistantEvents;

} // namespace xuoassist

