// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The hotkey action registry implementation (see actions.h).

#define LOGGER_MODULE Assistant

#include "actions.h"

#if defined(XUO_ASSISTANT)

#include <stdio.h>

#include <common/logging/logging.h>

#include "../config/assistantconfig.h"
#include "assist.h"
#include "assistantevents.h"
#include "global.h"
#include "Network/Packets.h" // ::CPacketTargetSelectedObject (the client's own 0xBF/0x2C builder)
#include "Target.h" // ::g_Target (the fused target cursor: hooks, senders, CancelCursor)
#include "Managers/MacroManager.h" // ::g_MacroManager (client-side selection macros)
#include "textcommandmanager.h"
#include "../managers/gumpmanager.h"
#include "../managers/spellmanager.h"
#include "world.h"

namespace xuoassist {

// --- the type table ---------------------------------------------------------

const char *Actions_TypeName(uint32_t type)
{
    static const char *const NAMES[] = {
        "Play macro",                            // HT_PLAY_MACRO
        "Stop macro",                            // HT_STOP_MACRO
        "Run script",                            // HT_SCRIPT
        "External code",                         // HT_EXTERNAL_CODE
        "Toggle hotkeys",                        // HT_TOGGLE_HOTKEYS
        "Toggle Map",                            // HT_TOGGLE_MAP
        "Attack",                                // HT_ATTACK
        "Use object",                            // HT_USE_OBJECT
        "All names",                             // HT_ALLNAMES
        "Cast spell",                            // HT_CAST
        "Cast spell on self",                    // HT_CAST_SELF
        "Cast spell on last target",             // HT_CAST_LAST_TARGET
        "Cast spell on last attack",             // HT_CAST_LAST_ATTACK
        "Cast spell on enemy",                   // HT_CAST_ENEMY
        "Cast spell on friend",                  // HT_CAST_FRIEND
        "Cast spell on new target system",       // HT_CAST_NEW_TARGET_SYSTEM
        "Use skill",                             // HT_USE_SKILL
        "Use skill on self",                     // HT_USE_SKILL_SELF
        "Use skill on last target",              // HT_USE_SKILL_LAST_TARGET
        "Use skill on last attack",              // HT_USE_SKILL_LAST_ATTACK
        "Use skill on enemy",                    // HT_USE_SKILL_ENEMY
        "Use skill on friend",                   // HT_USE_SKILL_FRIEND
        "Use skill on new target system",        // HT_USE_SKILL_NEW_TARGET_SYSTEM
        "Targeting",                             // HT_TARGETING
        "Bandage healing",                       // HT_BANDAGE_HEALING
        "New targeting system set",              // HT_NEW_TARGETING_SYSTEM_SET
    };
    if (type < (sizeof(NAMES) / sizeof(NAMES[0])))
    {
        return NAMES[type];
    }
    return "";
}

uint32_t Actions_TypeCount()
{
    return (uint32_t)HT_LAST; // the original HT_COUNT_OF_ITEMS (26 entries)
}

namespace {

// The fixed action lists (Init:262-292); spells/skills/macros are dynamic.
struct XA_ACTION_LIST
{
    uint32_t Type;
    const char *const *Items;
    int Count;
};

const char *const TOGGLE_HOTKEYS_ITEMS[] = { "Toggle", "On", "Off" };
const char *const TOGGLE_MAP_ITEMS[] = { "Toggle", "Show", "Hide" };
const char *const ATTACK_ITEMS[] = { "Last attack", "Last target", "Enemy" };
const char *const USE_OBJECT_ITEMS[] = { "Last object", "Left hand", "Right hand" };
const char *const ALLNAMES_ITEMS[] = { "All", "Mobiles", "Humanoids", "Corpses" };
const char *const TARGETING_ITEMS[] = {
    "Self", "Last target", "Last attack", "Enemy", "Friend", "Last tile"
};
const char *const BANDAGE_ITEMS[] = { "Self", "Friend", "Last target", "New target system" };
const char *const NTS_SET_ITEMS[] = {
    "Self",
    "Last target",
    "Last attack",
    "Enemy",
    "Friend",
    "Find next hostile",
    "Find preveous hostile", // (sic) original spelling
    "Find nearest hostile",
    "Find next mobile",
    "Find preveous mobile",
    "Find nearest mobile",
};

const XA_ACTION_LIST s_FixedLists[] = {
    { HT_TOGGLE_HOTKEYS, TOGGLE_HOTKEYS_ITEMS, 3 },
    { HT_TOGGLE_MAP, TOGGLE_MAP_ITEMS, 3 },
    { HT_ATTACK, ATTACK_ITEMS, 3 },
    { HT_USE_OBJECT, USE_OBJECT_ITEMS, 3 },
    { HT_ALLNAMES, ALLNAMES_ITEMS, 4 },
    { HT_TARGETING, TARGETING_ITEMS, 6 },
    { HT_BANDAGE_HEALING, BANDAGE_ITEMS, 4 },
    { HT_NEW_TARGETING_SYSTEM_SET, NTS_SET_ITEMS, 11 },
};

vector<astr_t> &dynamic_list(uint32_t type)
{
    // the spell/skill lists: every HT_CAST_*/HT_USE_SKILL_* type shares the
    // base list
    static vector<astr_t> s_Spells;
    static vector<astr_t> s_Skills;
    static vector<astr_t> s_Empty;
    if (type >= HT_CAST && type <= HT_CAST_NEW_TARGET_SYSTEM)
    {
        return s_Spells;
    }
    if (type >= HT_USE_SKILL && type <= HT_USE_SKILL_NEW_TARGET_SYSTEM)
    {
        return s_Skills;
    }
    return s_Empty;
}

} // namespace

int Actions_ActionCount(uint32_t type)
{
    if (type == HT_PLAY_MACRO)
    {
        return (int)g_AssistantConfig.Macros().size(); // UpdateMacroList
    }
    const vector<astr_t> &list = dynamic_list(type);
    if (!list.empty())
    {
        return (int)list.size();
    }
    for (const XA_ACTION_LIST &fixed : s_FixedLists)
    {
        if (fixed.Type == type)
        {
            return fixed.Count;
        }
    }
    return 0;
}

void Actions_ActionName(uint32_t type, int action, char *out, size_t size)
{
    if (size == 0)
    {
        return;
    }
    out[0] = '\0';
    if (type == HT_PLAY_MACRO)
    {
        const vector<CMacroEntry> &macros = g_AssistantConfig.Macros();
        if (action >= 0 && action < (int)macros.size())
        {
            snprintf(out, size, "%s", macros[action].Name.c_str());
        }
        return;
    }
    const vector<astr_t> &list = dynamic_list(type);
    if (!list.empty())
    {
        if (action >= 0 && action < (int)list.size())
        {
            snprintf(out, size, "%s", list[action].c_str());
        }
        return;
    }
    for (const XA_ACTION_LIST &fixed : s_FixedLists)
    {
        if (fixed.Type == type)
        {
            if (action >= 0 && action < fixed.Count)
            {
                snprintf(out, size, "%s", fixed.Items[action]);
            }
            return;
        }
    }
}

// --- the Data column (UpdateHotkeyData) --------------------------------------

void Actions_FormatDataText(const CHotkeyEntry &entry, char *out, size_t size)
{
    if (size == 0)
    {
        return;
    }
    out[0] = '\0';
    const uint32_t type = entry.Type;
    const uint32_t action = entry.Action;

    char name[256] = { 0 };
    switch (type)
    {
        case HT_PLAY_MACRO:
            if (!entry.ScriptOrMacroName.empty())
            {
                snprintf(name, sizeof(name), "Macro: %s", entry.ScriptOrMacroName.c_str());
            }
            break;
        case HT_STOP_MACRO:
            snprintf(name, sizeof(name), "Stop macro");
            break;
        case HT_SCRIPT:
            if (!entry.ScriptOrMacroName.empty())
            {
                snprintf(name, sizeof(name), "Script: %s", entry.ScriptOrMacroName.c_str());
            }
            break;
        case HT_EXTERNAL_CODE:
        {
            // first non-comment line becomes the label
            const astr_t &code = entry.ExternalCode;
            size_t start = 0;
            bool first = true;
            while (start <= code.size())
            {
                size_t end = code.find('\n', start);
                if (end == astr_t::npos)
                {
                    end = code.size();
                }
                astr_t line = code.substr(start, end - start);
                // trim
                const size_t textStart = line.find_first_not_of(" \t\r");
                if (textStart != astr_t::npos)
                {
                    line = line.substr(textStart, line.find_last_not_of(" \t\r") + 1 - textStart);
                }
                else
                {
                    line.clear();
                }
                if (first && line.rfind("//", 0) == 0)
                {
                    snprintf(name, sizeof(name), "Code: %s", line.c_str() + 2);
                    break;
                }
                if (!line.empty())
                {
                    snprintf(name + strlen(name), sizeof(name) - strlen(name), "%s ", line.c_str());
                }
                first = false;
                if (end == code.size())
                {
                    break;
                }
                start = end + 1;
            }
            break;
        }
        case HT_TOGGLE_HOTKEYS:
            snprintf(
                name, sizeof(name), "Hotkeys: %s",
                (action == 0 ? "toggle" : (action == 1 ? "on" : "off")));
            break;
        case HT_TOGGLE_MAP:
            snprintf(
                name, sizeof(name), "Map: %s", (action == 0 ? "toggle" : (action == 1 ? "show" : "hide")));
            break;
        case HT_ATTACK:
            snprintf(
                name, sizeof(name), "Attack: %s",
                (action == 0 ? "last" : (action == 1 ? "last target" : "enemy")));
            break;
        case HT_USE_OBJECT:
            snprintf(
                name, sizeof(name), "Use: %s",
                (action == 0 ? "last" : (action == 1 ? "left hand" : "right hand")));
            break;
        case HT_ALLNAMES:
        {
            const char *what = (action == 0 ?
                                    "all" :
                                (action == 1 ? "mobiles" : (action == 2 ? "humanoids" : "corpses")));
            snprintf(name, sizeof(name), "All names: %s", what);
            break;
        }
        case HT_CAST:
        case HT_CAST_SELF:
        case HT_CAST_LAST_TARGET:
        case HT_CAST_LAST_ATTACK:
        case HT_CAST_ENEMY:
        case HT_CAST_FRIEND:
        case HT_CAST_NEW_TARGET_SYSTEM:
        {
            static const char *const CAST_PREFIX[] = {
                "Cast: ",
                "Cast self: ",
                "Cast last target: ",
                "Cast last attack: ",
                "Cast enemy: ",
                "Cast friend: ",
                "Cast new target system: ",
            };
            const vector<astr_t> &spells = dynamic_list(HT_CAST);
            char spell[128] = { 0 };
            if (action < spells.size())
            {
                snprintf(spell, sizeof(spell), "%s", spells[action].c_str());
            }
            snprintf(name, sizeof(name), "%s%s", CAST_PREFIX[type - HT_CAST], spell);
            break;
        }
        case HT_USE_SKILL:
        case HT_USE_SKILL_SELF:
        case HT_USE_SKILL_LAST_TARGET:
        case HT_USE_SKILL_LAST_ATTACK:
        case HT_USE_SKILL_ENEMY:
        case HT_USE_SKILL_FRIEND:
        case HT_USE_SKILL_NEW_TARGET_SYSTEM:
        {
            static const char *const SKILL_PREFIX[] = {
                "Use skill: ",
                "Use skill self: ",
                "Use skill last target: ",
                "Use skill last attack: ",
                "Use skill enemy: ",
                "Use skill friend: ",
                "Use skill new target system: ",
            };
            const vector<astr_t> &skills = dynamic_list(HT_USE_SKILL);
            char skill[128] = { 0 };
            if (action < skills.size())
            {
                snprintf(skill, sizeof(skill), "%s", skills[action].c_str());
            }
            snprintf(name, sizeof(name), "%s%s", SKILL_PREFIX[type - HT_USE_SKILL], skill);
            break;
        }
        case HT_TARGETING:
        {
            static const char *const TARGET_ITEMS[] = {
                "Target: self",
                "Target: last target",
                "Target: last attack",
                "Target: enemy",
                "Target: friend",
                "Target: last tile",
            };
            if (action <= 5)
            {
                snprintf(name, sizeof(name), "%s", TARGET_ITEMS[action]);
            }
            break;
        }
        case HT_BANDAGE_HEALING:
        {
            static const char *const BANDAGE_NAMES[] = {
                "BandageHealing: self",
                "BandageHealing: friend",
                "BandageHealing: last target",
                "BandageHealing: new target system",
            };
            if (action <= 3)
            {
                snprintf(name, sizeof(name), "%s", BANDAGE_NAMES[action]);
            }
            break;
        }
        case HT_NEW_TARGETING_SYSTEM_SET:
        {
            static const char *const NTS_NAMES[] = {
                "NewTargettingSystemSet: self", // (sic) original spelling
                "NewTargettingSystemSet: last target",
                "NewTargettingSystemSet: last attack",
                "NewTargettingSystemSet: enemy",
                "NewTargettingSystemSet: friend",
                "NewTargettingSystemSet: Find next hostile",
                "NewTargettingSystemSet: Find preveous hostile",
                "NewTargettingSystemSet: Find nearest hostile",
                "NewTargettingSystemSet: Find next mobile",
                "NewTargettingSystemSet: Find preveous mobile",
                "NewTargettingSystemSet: Find nearest mobile",
            };
            if (action <= 10)
            {
                snprintf(name, sizeof(name), "%s", NTS_NAMES[action]);
            }
            break;
        }
        default:
            break;
    }

    if (name[0] != '\0')
    {
        snprintf(out, size, "%s", name);
    }
    // else: the caller renders "Blank hotkey"
}

// --- the executor side (PlayHotkeyAction) -------------------------------------

namespace {

void hook_objects(const astr_t &a)
{
    vector<astr_t> objects;
    objects.push_back(a);
    g_Target.SetObjectsHook(objects);
}

} // namespace

void Actions_Execute(const CHotkeyEntry &entry)
{
    const uint32_t type = entry.Type;
    const int action = (int)entry.Action;

    switch (type)
    {
        case HT_TOGGLE_MAP:
            // the map window is Phase 5: the binding is recognized (and
            // consumed by the engine) but there is nothing to toggle yet
            break;
        case HT_ATTACK:
        {
            uint32_t serial = 0;
            if (action == 0)
            {
                serial = CAssistant::TextToSerial("lastattack");
            }
            else if (action == 1)
            {
                serial = CAssistant::TextToSerial("lasttarget");
            }
            else
            {
                serial = CAssistant::TextToSerial("enemy");
            }
            if (serial != 0u)
            {
                g_Assistant.Attack(serial);
            }
            break;
        }
        case HT_USE_OBJECT:
        {
            uint32_t serial = 0;
            if (action == 0)
            {
                serial = CAssistant::TextToSerial("lastobject");
            }
            // Parity with the original: only action 1 resolves a hand;
            // the "Use: right hand" slot is a no-op in the original (the
            // label advertises it, the executor never handled it) - kept.
            else if (action == 1)
            {
                CGameItem *objAtLayer = g_Player->FindLayer(OL_1_HAND);
                if (objAtLayer != nullptr)
                {
                    serial = objAtLayer->Serial;
                }
            }
            if (serial != 0u)
            {
                g_Assistant.DoubleClick(serial);
            }
            break;
        }
        case HT_ALLNAMES:
        {
            const bool showMobiles = (action == 0 || action == 1);
            const bool showHumanoids = (action == 0 || action == 2);
            const bool showCorpses = (action == 0 || action == 3);
            if (g_World == nullptr)
            {
                break;
            }
            for (CGameObject *obj = g_World->m_Items; obj != nullptr; obj = (CGameObject *)obj->m_Next)
            {
                if (obj->IsCorpse())
                {
                    if (showCorpses)
                    {
                        g_Assistant.Click(obj->Serial);
                    }
                }
                else if (obj->NPC && !obj->IsPlayer())
                {
                    if (showMobiles || (showHumanoids && obj->IsHuman()))
                    {
                        g_Assistant.Click(obj->Serial);
                    }
                }
            }
            break;
        }
        case HT_CAST:
        case HT_CAST_SELF:
        case HT_CAST_LAST_TARGET:
        case HT_CAST_LAST_ATTACK:
        case HT_CAST_ENEMY:
        case HT_CAST_FRIEND:
        case HT_CAST_NEW_TARGET_SYSTEM:
        {
            char spellName[128] = { 0 };
            Actions_ActionName(type, action, spellName, sizeof(spellName));
            if (spellName[0] == '\0')
            {
                break;
            }
            if (type == HT_CAST_NEW_TARGET_SYSTEM)
            {
                g_SpellManager.TargetCast(spellName, Actions_NewTargetSystemSerial());
            }
            else
            {
                bool withTarget = true;
                switch (type)
                {
                    case HT_CAST_SELF:
                        hook_objects("self");
                        break;
                    case HT_CAST_LAST_TARGET:
                        hook_objects("lasttarget");
                        break;
                    case HT_CAST_LAST_ATTACK:
                        hook_objects("lastattack");
                        break;
                    case HT_CAST_ENEMY:
                        hook_objects("enemy");
                        break;
                    case HT_CAST_FRIEND:
                        hook_objects("friend");
                        break;
                    default:
                        withTarget = false;
                        break;
                }
                (void)withTarget;
                g_SpellManager.Cast(spellName);
            }
            break;
        }
        case HT_USE_SKILL:
        case HT_USE_SKILL_SELF:
        case HT_USE_SKILL_LAST_TARGET:
        case HT_USE_SKILL_LAST_ATTACK:
        case HT_USE_SKILL_ENEMY:
        case HT_USE_SKILL_FRIEND:
        case HT_USE_SKILL_NEW_TARGET_SYSTEM:
        {
            char skillName[128] = { 0 };
            Actions_ActionName(type, action, skillName, sizeof(skillName));
            if (skillName[0] == '\0')
            {
                break;
            }
            if (type == HT_USE_SKILL_NEW_TARGET_SYSTEM)
            {
                const int index = g_SkillsManager.Find(skillName);
                if (index == -1)
                {
                    g_Assistant.ClientPrint("Skill '" + astr_t(skillName) + "' is not found.");
                }
                else
                {
                    // the wire skill ids are 1-based; a raw packet send runs
                    // no recording taps, so no suspension is needed
                    CPacketTargetUseSkill(index + 1, Actions_NewTargetSystemSerial()).Send();
                }
            }
            else
            {
                switch (type)
                {
                    case HT_USE_SKILL_SELF:
                        hook_objects("self");
                        break;
                    case HT_USE_SKILL_LAST_TARGET:
                        hook_objects("lasttarget");
                        break;
                    case HT_USE_SKILL_LAST_ATTACK:
                        hook_objects("lastattack");
                        break;
                    case HT_USE_SKILL_ENEMY:
                        hook_objects("enemy");
                        break;
                    case HT_USE_SKILL_FRIEND:
                        hook_objects("friend");
                        break;
                    default:
                        // Parity with the original: plain use-skill
                        // still calls SetObjectsHook with an empty list,
                        // clearing any stale wait-target hook so it cannot
                        // auto-target a later cursor.
                        g_Target.SetObjectsHook(vector<astr_t>());
                        break;
                }
                g_SkillsManager.Use(skillName);
            }
            break;
        }
        case HT_TARGETING:
        {
            uint32_t serial = 0;
            if (action == 0)
            {
                serial = g_PlayerSerial;
            }
            else if (action == 1)
            {
                serial = g_LastTargetObject;
            }
            else if (action == 2)
            {
                serial = g_LastAttackObject;
            }
            else if (action == 3)
            {
                serial = CAssistant::TextToSerial("enemy");
            }
            else if (action == 4)
            {
                serial = CAssistant::TextToSerial("friend");
            }
            if (action == 5)
            {
                // the last tile a tile-target answered with (was the
                // assistant sender; the fused client sender records)
                g_Target.SendTargetTile(
                    g_Target.LastTileGraphic,
                    g_Target.LastTileX,
                    g_Target.LastTileY,
                    (char)g_Target.LastTileZ);
            }
            else if (serial != 0u)
            {
                g_Target.SendTargetObject(serial);
            }
            ::g_Target.CancelCursor();
            break;
        }
        case HT_BANDAGE_HEALING:
        {
            CGameItem *bandage = g_Player != nullptr ? g_Player->FindBandage() : nullptr;
            if (bandage != nullptr)
            {
                if (action == 3)
                {
                    ::CPacketTargetSelectedObject(bandage->Serial, Actions_NewTargetSystemSerial())
                        .Send();
                }
                else
                {
                    if (action == 0)
                    {
                        hook_objects("self");
                    }
                    else if (action == 1)
                    {
                        hook_objects("friend");
                    }
                    else if (action == 2)
                    {
                        hook_objects("lasttarget");
                    }
                    g_Assistant.DoubleClick(bandage->Serial);
                }
            }
            break;
        }
        case HT_NEW_TARGETING_SYSTEM_SET:
        {
            if (action == 0)
            {
                Actions_SetNewTargetSystemSerial(g_PlayerSerial);
            }
            else if (action == 1)
            {
                Actions_SetNewTargetSystemSerial(g_LastTargetObject);
            }
            else if (action == 2)
            {
                Actions_SetNewTargetSystemSerial(g_LastAttackObject);
            }
            else if (action == 3)
            {
                Actions_SetNewTargetSystemSerial(CAssistant::TextToSerial("enemy"));
            }
            else if (action == 4)
            {
                Actions_SetNewTargetSystemSerial(CAssistant::TextToSerial("friend"));
            }
            else if (action >= 5 && action <= 10)
            {
                // client-side selection macros
                vector<astr_t> clientMacroActions;
                vector<astr_t> clientMacroSubActions;
                if (action == 5 || action == 8)
                {
                    clientMacroActions.push_back("SelectNext");
                }
                else if (action == 6 || action == 9)
                {
                    clientMacroActions.push_back("SelectPreveous");
                }
                else if (action == 7 || action == 10)
                {
                    clientMacroActions.push_back("SelectNearest");
                }
                clientMacroSubActions.push_back(action <= 7 ? "Hostile" : "Mobile");
                ::g_MacroManager.ExecuteFromLists(clientMacroActions, clientMacroSubActions);
            }
            break;
        }
        default:
            break;
    }
}

void Actions_RunExternalCode(const astr_t &code)
{
    // The external-code text runs through the same text-command
    // pipeline as typed "_cmd" speech when it
    // starts with the configured command prefix; otherwise it is inert (the
    // script engine arrives in Phase 4).
    int prefixLen = g_AssistantConfig.IsCommandPrefix(code);
    if (prefixLen > 0)
    {
        g_TextCommandManager.DoCommand(code);
    }
    else
    {
        Info(Assistant, "external code hotkey ignored (no command prefix, script engine is phase 4)");
    }
}

// --- the 0xFC feeds -----------------------------------------------------------

void Actions_SetSpellList(const vector<vector<astr_t>> &books)
{
    // "Last" + every book's names, flattened
    vector<astr_t> spells;
    spells.reserve(8);
    spells.push_back("Last");
    for (const vector<astr_t> &book : books)
    {
        for (const astr_t &name : book)
        {
            spells.push_back(name);
        }
    }
    dynamic_list(HT_CAST) = spells;
}

void Actions_SetSkillList(const vector<astr_t> &names)
{
    // the packet handler already ships "Last" + the use-button skills
    dynamic_list(HT_USE_SKILL) = names;
}

// --- new target system serial ---------------------------------------------------

// Was the assistant virtual-int VKI_NEW_TARGET_SYSTEM_SERIAL:
// assistant-owned state, so it lives here instead of the client's
// virtual-int table.
namespace {
uint32_t s_NewTargetSystemSerial = 0;
} // namespace

uint32_t Actions_NewTargetSystemSerial()
{
    return s_NewTargetSystemSerial;
}

void Actions_SetNewTargetSystemSerial(uint32_t serial)
{
    s_NewTargetSystemSerial = serial;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
