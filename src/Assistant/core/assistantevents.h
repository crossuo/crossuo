// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// AssistantEvents: the notification seam between packet handling and the
// assistant UI. Anything that is pure world state stays in the
// model/managers; anything a UI consumes flows through here as
// g_AssistantEvents->OnXxx(...) notifications.
//
// Design: a class with virtual methods and ONE replaceable global instance
// pointer. The default instance's methods are inert (except OnClientPrint,
// which performs pure client-side packet injection). The UI layer installs a
// subclass instance into g_AssistantEvents; handlers never change. No
// UI-framework coupling, no std::function registry, C++14.

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

// Journal message flags; the flags travel with the messages.
enum XA_JOURNAL_MESSAGE_FLAGS
{
    JMT_NORMAL = 0x00,
    JMT_MY = 0x01,
    JMT_SYSTEM = 0x02
};

// Party/guild member positions as parsed from the Krrios special packet;
// the packet handler hands them over as-is.
struct XA_PARTY_MEMBER_INFO
{
    uint32_t Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint32_t Map{ 0 };
    astr_t Name{ "" };
};

struct XA_GUILD_MEMBER_INFO
{
    uint32_t Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint32_t Map{ 0 };
    astr_t Name{ "" };
};

class CAssistantEvents
{
public:
    CAssistantEvents() = default;
    virtual ~CAssistantEvents() = default;

    CAssistantEvents(const CAssistantEvents &) = delete;
    CAssistantEvents &operator=(const CAssistantEvents &) = delete;

    // --- speech / journal --------------------------------------------------------
    // The assistant's own journal cache is fed elsewhere; the UI subscribes
    // for its journal tab. Default: nothing (client adds its own entries).
    virtual void OnJournalMessage(const CJournalMessage &message) { (void)message; }

    // Inject a system print into the client; the default performs the
    // client-side packet injection.
    virtual void OnClientPrint(const astr_t &text, uint16_t color = g_MessagesColor);

    // --- buffs --------------------------------------------------------------------
    virtual void OnBuffAdd(uint16_t graphic, uint32_t timer, const astr_t &name)
    {
        (void)graphic;
        (void)timer;
        (void)name;
    }
    virtual void OnBuffRemove(uint16_t graphic) { (void)graphic; }
    // --- corpses (auto-open bookkeeping) ------------------------------------------
    // expireTicks is an absolute g_Ticks stamp.
    virtual void OnCorpseAdd(uint32_t serial, uint32_t expireTicks)
    {
        (void)serial;
        (void)expireTicks;
    }

    // --- party / guild member positions (Krrios special packet) ------------
    virtual void OnPartyMemberInfo(const XA_PARTY_MEMBER_INFO &info) { (void)info; }
    virtual void OnGuildMemberInfo(const XA_GUILD_MEMBER_INFO &info) { (void)info; }

    // --- object properties ---------------------------------------------------------
    virtual void OnMegaCliloc(
        uint32_t serial,
        uint32_t revision,
        const astr_t &name,
        const astr_t &properties)
    {
        (void)serial;
        (void)revision;
        (void)name;
        (void)properties;
    }

    // --- skills (the skills-tab refresh and the hotkey-action feed) ---------
    virtual void OnSkillUpdated(int id) { (void)id; }
    virtual void OnSkillsRefreshed() {}
    // "Last" + every use-button skill name, in packet order.
    virtual void OnSkillsList(const vector<astr_t> &hotkeyActions) { (void)hotkeyActions; }

    // --- spells / macros from the client's 0xFC packets --------------------
    // One entry per spellbook (circles), spell names in packet order.
    virtual void OnSpellList(const vector<vector<astr_t>> &books) { (void)books; }
    virtual void OnMacroList(const vector<std::pair<astr_t, vector<astr_t>>> &macros)
    {
        (void)macros;
    }

    // --- macros (recording capture) ------------------------------------------
    // graphic/color are 0 when the targeted object is unknown to the world.
    virtual void OnRecordTargetObject(uint32_t serial, uint16_t graphic, uint16_t color)
    {
        (void)serial;
        (void)graphic;
        (void)color;
    }
    virtual void OnRecordTargetTile(uint16_t graphic, int x, int y, int z)
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

    // The closeuo command: the sink asks
    // the client window to close (CWindow::Destroy -> the SDL_QUIT path).
    virtual void OnClientCloseRequest() {}

    // --- from-target requests (the tablists "from target" buttons; the
    //     HandleTarget* stubs in core/assist.cpp forward the 0x6C reply) ----
    // XA_FROM_TARGET_* identifies which tab asked; name is the object name
    // when known (AnimalVendorName requests), "" otherwise.
    virtual void OnFromTarget(int kind, uint32_t serial, uint16_t graphic, uint16_t color, const astr_t &name)
    {
        (void)kind;
        (void)serial;
        (void)graphic;
        (void)color;
        (void)name;
    }

    // --- object inspector ------------------------------------------------------
    virtual void OnInspectorUpdate(uint32_t serial) { (void)serial; }

    // --- display tab title bar -----------------------------------------------
    virtual void OnTitleRedraw() {}
    virtual void OnTitleRedraw(uint32_t serial) { (void)serial; }

    // --- map window -----------------------------------------------------------
    virtual void OnMapUpdate(uint32_t serial) { (void)serial; }
    virtual void OnMapRedraw() {}
    // The map-open request: the OIPMT_OPEN_MAP app packet, the MapSettings
    // "Show on log in" flag and the hotkey launchers.
    virtual void OnMapOpenRequest() {}

    // --- tracker ---------------------------------------------------------------
    virtual void OnTrack(bool state, int x, int y)
    {
        (void)state;
        (void)x;
        (void)y;
    }

    virtual void OnMapData(uint16_t x, uint16_t y);
};

// The installed sink. Never null: assistantevents.cpp installs the default
// instance at static-init; the UI layer replaces (and later restores) it.
extern CAssistantEvents *g_AssistantEvents;

// Script-visible buff display name for the 0xDF mirror (the static name
// table); iconIndex is the icon id minus 0x03E9.
const astr_t &AssistantBuffName(uint16_t iconIndex);

} // namespace xuoassist
