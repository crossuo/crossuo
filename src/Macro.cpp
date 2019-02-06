// Copyright (C) August 2016 Hotride

#include "Macro.h"

#define MODKEY_ALT 0x0100
#define MODKEY_CTRL 0x0200
#define MODKEY_SHIFT 0x0400

CMacroObject *g_MacroPointer = nullptr;

CMacroObject::CMacroObject(const MACRO_CODE &code, const MACRO_SUB_CODE &subCode)
    : Code(code)
    , SubCode(subCode)
{
    DEBUG_TRACE_FUNCTION;
    switch (code)
    {
        //With sub menu
        case MC_WALK:
        case MC_OPEN:
        case MC_CLOSE:
        case MC_MINIMIZE:
        case MC_MAXIMIZE:
        case MC_USE_SKILL:
        case MC_ARM_DISARM:
        case MC_INVOKE_VIRTUE:
        case MC_CAST_SPELL:
        case MC_SELECT_NEXT:
        case MC_SELECT_PREVIOUS:
        case MC_SELECT_NEAREST:
        {
            if (subCode == MSC_NONE)
            {
                int count = 0;
                int offset = 0;
                CMacro::GetBoundByCode(code, count, offset);
                SubCode = (MACRO_SUB_CODE)offset;
            }
            HasSubMenu = 1;
            break;
        }
        //With entry text
        case MC_SAY:
        case MC_EMOTE:
        case MC_WHISPER:
        case MC_YELL:
        case MC_DELAY:
        case MC_SET_UPDATE_RANGE:
        case MC_MODIFY_UPDATE_RANGE:
        {
            HasSubMenu = 2;
            break;
        }
        default:
        {
            HasSubMenu = 0;
            break;
        }
    }
}

CMacroObject::~CMacroObject()
{
}

CMacroObjectString::CMacroObjectString(
    const MACRO_CODE &code, const MACRO_SUB_CODE &subCode, const string &str)
    : CMacroObject(code, subCode)
    , m_String(str)
{
}

CMacroObjectString::~CMacroObjectString()
{
}

CMacro::CMacro(Keycode key, bool alt, bool ctrl, bool shift)
    : Key(key)
    , Alt(alt)
    , Ctrl(ctrl)
    , Shift(shift)
{
}

CMacro::~CMacro()
{
}

CMacro *CMacro::CreateBlankMacro()
{
    DEBUG_TRACE_FUNCTION;
    auto obj = new CMacro(0, false, false, false);
    obj->Add(new CMacroObject(MC_NONE, MSC_NONE));
    return obj;
}

CMacroObject *CMacro::CreateMacro(const MACRO_CODE &code)
{
    DEBUG_TRACE_FUNCTION;
    CMacroObject *obj = nullptr;
    switch (code)
    {
        //With entry text
        case MC_SAY:
        case MC_EMOTE:
        case MC_WHISPER:
        case MC_YELL:
        case MC_DELAY:
        case MC_SET_UPDATE_RANGE:
        case MC_MODIFY_UPDATE_RANGE:
        {
            obj = new CMacroObjectString(code, MSC_NONE, "");
            break;
        }
        default:
        {
            obj = new CMacroObject(code, MSC_NONE);
            break;
        }
    }

    return obj;
}

void CMacro::ChangeObject(CMacroObject *source, CMacroObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    obj->m_Prev = source->m_Prev;
    obj->m_Next = source->m_Next;
    if (source->m_Prev == nullptr)
    {
        m_Items = obj;
    }
    else
    {
        source->m_Prev->m_Next = obj;
    }

    if (source->m_Next != nullptr)
    {
        source->m_Next->m_Prev = obj;
    }
    source->m_Prev = nullptr;
    source->m_Next = nullptr;
    delete source;
}

CMacro *CMacro::Load(Wisp::CMappedFile &file)
{
    DEBUG_TRACE_FUNCTION;
    uint8_t *next = file.Ptr;
    short size = file.ReadInt16LE();
    next += size;

    auto key = file.ReadInt32LE();
    bool alt = false;
    if ((key & MODKEY_ALT) != 0)
    {
        key -= MODKEY_ALT;
        alt = true;
    }

    bool ctrl = false;
    if ((key & MODKEY_CTRL) != 0)
    {
        key -= MODKEY_CTRL;
        ctrl = true;
    }

    bool shift = false;
    if ((key & MODKEY_SHIFT) != 0)
    {
        key -= MODKEY_SHIFT;
        shift = true;
    }

    int count = file.ReadInt16LE();
    auto macro = new CMacro(key, alt, ctrl, shift);
    for (int i = 0; i < count; i++)
    {
        auto type = file.ReadUInt8();
        MACRO_CODE code = (MACRO_CODE)file.ReadUInt16LE();
        MACRO_SUB_CODE subCode = (MACRO_SUB_CODE)file.ReadUInt16LE();
        CMacroObject *obj = nullptr;
        switch (type)
        {
            case 0: //original
            {
                obj = new CMacroObject(code, subCode);
                break;
            }
            case 2: //with string
            {
                short len = file.ReadUInt16LE();
                string str = file.ReadString(len);
                obj = new CMacroObjectString(code, subCode, str);
                break;
            }
            default:
                break;
        }
        if (obj != nullptr)
        {
            macro->Add(obj);
        }
    }
    file.Ptr = next;
    return macro;
}

void CMacro::Save(Wisp::CBinaryFileWriter &writer)
{
    DEBUG_TRACE_FUNCTION;
    short size = 12;
    short count = 0;
    for (auto obj = (CMacroObject *)m_Items; obj != nullptr; obj = (CMacroObject *)obj->m_Next)
    {
        size += 5;
        count++;
        if (obj->HaveString()) //with string
        {
            string str = ((CMacroObjectString *)obj)->m_String;
            size += (short)str.length() + 3;
        }
    }

    writer.WriteUInt16LE(size);
    auto key = Key;
    if (Alt)
    {
        key += MODKEY_ALT;
    }

    if (Ctrl)
    {
        key += MODKEY_CTRL;
    }

    if (Shift)
    {
        key += MODKEY_SHIFT;
    }

    writer.WriteInt32LE(key);
    writer.WriteUInt16LE(count);
    for (auto obj = (CMacroObject *)m_Items; obj != nullptr; obj = (CMacroObject *)obj->m_Next)
    {
        uint8_t type = 0;
        if (obj->HaveString())
        {
            type = 2;
        }

        writer.WriteUInt8(type);
        writer.WriteUInt16LE(obj->Code);
        writer.WriteUInt16LE(obj->SubCode);
        if (type == 2) //with string
        {
            string str = ((CMacroObjectString *)obj)->m_String;
            int len = (int)str.length();
            writer.WriteInt16LE(len + 1);
            writer.WriteString(str);
        }
        writer.WriteBuffer();
    }
    writer.WriteUInt32LE(0); //EOM
    writer.WriteBuffer();
}

CMacro *CMacro::GetCopy()
{
    DEBUG_TRACE_FUNCTION;
    CMacro *macro = new CMacro(Key, Alt, Ctrl, Shift);
    MACRO_CODE oldCode = MC_NONE;
    for (auto obj = (CMacroObject *)m_Items; obj != nullptr; obj = (CMacroObject *)obj->m_Next)
    {
        if (obj->HaveString())
        {
            macro->Add(new CMacroObjectString(
                obj->Code, obj->SubCode, ((CMacroObjectString *)obj)->m_String));
        }
        else
        {
            macro->Add(new CMacroObject(obj->Code, obj->SubCode));
        }
        oldCode = obj->Code;
    }

    if (oldCode != MC_NONE)
    {
        macro->Add(new CMacroObject(MC_NONE, MSC_NONE));
    }
    return macro;
}

void CMacro::GetBoundByCode(const MACRO_CODE &code, int &count, int &offset)
{
    DEBUG_TRACE_FUNCTION;
    switch (code)
    {
        case MC_WALK:
        {
            offset = MSC_G1_NW;
            count = MSC_G2_CONFIGURATION - MSC_G1_NW;
            break;
        }
        case MC_OPEN:
        case MC_CLOSE:
        case MC_MINIMIZE:
        case MC_MAXIMIZE:
        {
            offset = MSC_G2_CONFIGURATION;
            count = MSC_G3_ANATOMY - MSC_G2_CONFIGURATION;
            break;
        }
        case MC_USE_SKILL:
        {
            offset = MSC_G3_ANATOMY;
            count = MSC_G4_LEFT_HAND - MSC_G3_ANATOMY;
            break;
        }
        case MC_ARM_DISARM:
        {
            offset = MSC_G4_LEFT_HAND;
            count = MSC_G5_HONOR - MSC_G4_LEFT_HAND;
            break;
        }
        case MC_INVOKE_VIRTUE:
        {
            offset = MSC_G5_HONOR;
            count = MSC_G6_CLUMSY - MSC_G5_HONOR;
            break;
        }
        case MC_CAST_SPELL:
        {
            offset = MSC_G6_CLUMSY;
            count = MSC_G7_HOSTILE - MSC_G6_CLUMSY;
            break;
        }
        case MC_SELECT_NEXT:
        case MC_SELECT_PREVIOUS:
        case MC_SELECT_NEAREST:
        {
            offset = MSC_G7_HOSTILE;
            count = MSC_TOTAL_COUNT - MSC_G7_HOSTILE;
            break;
        }
        default:
            break;
    }
}

const char *CMacro::m_MacroActionName[MACRO_ACTION_NAME_COUNT] = { "(NONE)",
                                                                   "Say",
                                                                   "Emote",
                                                                   "Whisper",
                                                                   "Yell",
                                                                   "Walk",
                                                                   "War/Peace",
                                                                   "Paste",
                                                                   "Open",
                                                                   "Close",
                                                                   "Minimize",
                                                                   "Maximize",
                                                                   "OpenDoor",
                                                                   "UseSkill",
                                                                   "LastSkill",
                                                                   "CastSpell",
                                                                   "LastSpell",
                                                                   "LastObject",
                                                                   "Bow",
                                                                   "Salute",
                                                                   "QuitGame",
                                                                   "AllNames",
                                                                   "LastTarget",
                                                                   "TargetSelf",
                                                                   "Arm/Disarm",
                                                                   "WaitForTarget",
                                                                   "TargetNext",
                                                                   "AttackLast",
                                                                   "Delay",
                                                                   "CircleTrans",
                                                                   "CloseGumps",
                                                                   "AlwaysRun",
                                                                   "SaveDesktop",
                                                                   "KillGumpOpen",
                                                                   "PrimaryAbility",
                                                                   "SecondaryAbility",
                                                                   "EquipLastWeapon",
                                                                   "SetUpdateRange",
                                                                   "ModifyUpdateRange",
                                                                   "IncreaseUpdateRange",
                                                                   "DecreaseUpdateRange",
                                                                   "MaxUpdateRange",
                                                                   "MinUpdateRange",
                                                                   "DefaultUpdateRange",
                                                                   "UpdateRangeInfo",
                                                                   "EnableRangeColor",
                                                                   "DisableRangeColor",
                                                                   "ToggleRangeColor",
                                                                   "InvokeVirtue",
                                                                   "SelectNext",
                                                                   "SelectPrevious",
                                                                   "SelectNearest",
                                                                   "AttackSelectedTarget",
                                                                   "UseSelectedTarget",
                                                                   "CurrentTarget",
                                                                   "TargetSystemOn/Off",
                                                                   "ToggleBuficonWindow",
                                                                   "BandageSelf",
                                                                   "BandageTarget",
                                                                   "ToggleGargoyleFlying" };

const char *CMacro::m_MacroAction[MACRO_ACTION_COUNT] = {
    "?",
    "NW (top)", //Walk group
    "N (topright)",
    "NE (right)",
    "E (bottonright)",
    "SE (bottom)",
    "S (bottomleft)",
    "SW (left)",
    "W (topleft)",
    "Configuration", //Open/Close/Minimize/Maximize group
    "Paperdoll",
    "Status",
    "Journal",
    "Skills",
    "Mage Spellbook",
    "Chat",
    "Backpack",
    "Overview",
    "World Map",
    "Mail",
    "Party Manifest",
    "Party Chat",
    "Necro Spellbook",
    "Paladin Spellbook",
    "Combat Book",
    "Bushido Spellbook",
    "Ninjitsu Spellbook",
    "Guild",
    "Spell Weaving Spellbook",
    "Quest Log",
    "Mysticism Spellbook",
    "Racial Abilities Book",
    "Bard Spellbook",
    "Anatomy", //Skills group
    "Animal Lore",
    "Animal Taming",
    "Arms Lore",
    "Begging",
    "Cartograpy",
    "Detecting Hidden",
    "Enticement",
    "Evaluating Intelligence",
    "Forensic Evaluation",
    "Hiding",
    "Imbuing",
    "Inscription",
    "Item Identification",
    "Meditation",
    "Peacemaking",
    "Poisoning",
    "Provocation",
    "Remove Trap",
    "Spirit Speak",
    "Stealing",
    "Stealth",
    "Taste Identification",
    "Tracking",
    "Left Hand", ///Arm/Disarm group
    "Right Hand",
    "Honor", //Invoke Virtue group
    "Sacrifice",
    "Valor",
    "Clumsy", //Cast Spell group
    "Create Food",
    "Feeblemind",
    "Heal",
    "Magic Arrow",
    "Night Sight",
    "Reactive Armor",
    "Weaken",
    "Agility",
    "Cunning",
    "Cure",
    "Harm",
    "Magic Trap",
    "Magic Untrap",
    "Protection",
    "Strength",
    "Bless",
    "Fireball",
    "Magic Lock",
    "Poison",
    "Telekinesis",
    "Teleport",
    "Unlock",
    "Wall Of Stone",
    "Arch Cure",
    "Arch Protection",
    "Curse",
    "Fire Field",
    "Greater Heal",
    "Lightning",
    "Mana Drain",
    "Recall",
    "Blade Spirits",
    "Dispell Field",
    "Incognito",
    "Magic Reflection",
    "Mind Blast",
    "Paralyze",
    "Poison Field",
    "Summon Creature",
    "Dispel",
    "Energy Bolt",
    "Explosion",
    "Invisibility",
    "Mark",
    "Mass Curse",
    "Paralyze Field",
    "Reveal",
    "Chain Lightning",
    "Energy Field",
    "Flame Strike",
    "Gate Travel",
    "Mana Vampire",
    "Mass Dispel",
    "Meteor Swarm",
    "Polymorph",
    "Earthquake",
    "Energy Vortex",
    "Resurrection",
    "Air Elemental",
    "Summon Daemon",
    "Earth Elemental",
    "Fire Elemental",
    "Water Elemental",
    "Animate Dead",
    "Blood Oath",
    "Corpse Skin",
    "Curse Weapon",
    "Evil Omen",
    "Horrific Beast",
    "Lich Form",
    "Mind Rot",
    "Pain Spike",
    "Poison Strike",
    "Strangle",
    "Summon Familar",
    "Vampiric Embrace",
    "Vengeful Spirit",
    "Wither",
    "Wraith Form",
    "Exorcism",
    "Cleanse By Fire",
    "Close Wounds",
    "Concentrate Weapon",
    "Dispel Evil",
    "Divine Fury",
    "Enemy Of One",
    "Holy Light",
    "Noble Sacrifice",
    "Remove Curse",
    "Sacred Journey",
    "Honorable Execution",
    "Confidence",
    "Evasion",
    "Counter Attack",
    "Lightning Strike",
    "Momentum Strike",
    "Focus Attack",
    "Death Strike",
    "Animal Form",
    "Ki Attack",
    "Surprice Attack",
    "Backstab",
    "Shadowjump",
    "Mirror Image",
    "Arcane Circle",
    "Gift Of Reneval",
    "Immolating Weapon",
    "Attunement",
    "Thunderstorm",
    "Natures Fury",
    "Summon Fey",
    "Summon Fiend",
    "Reaper Form",
    "Wildfire",
    "Essence Of Wind",
    "Dryad Allure",
    "Ethereal Voyage",
    "Word Of Death",
    "Gift Of Life",
    "Arcane Empowerment",
    "Nether Bolt",
    "Healing Stone",
    "Purge Magic",
    "Enchant",
    "Sleep",
    "Eagle Strike",
    "Animated Weapon",
    "Stone Form",
    "Spell Trigger",
    "Mass Sleep",
    "Cleansing Winds",
    "Bombard",
    "Spell Plague",
    "Hail Storm",
    "Nether Cyclone",
    "Rising Colossus",
    "Inspire",
    "Invigorate",
    "Resilience",
    "Perseverance",
    "Tribulation",
    "Despair",
    "Hostile", //Select Next/Previous/Nearest group
    "Party",
    "Follower",
    "Object",
    "Mobile"
};
