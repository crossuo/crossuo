// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The macro model: GetText display strings and the XML attribute set; the
// spell-name lookup uses the spell manager.

#include "macro.h"
#include "../core/assist.h"
#include "../managers/spellmanager.h"
#include "Managers/SkillsManager.h" // ::g_SkillsManager
#include <algorithm>

namespace xuoassist {

astr_t CMacro::GetText() const
{
    switch (Type)
    {
        case MT_USE:
        {
            CMacroWithObject *withObject = (GetClass() == MCLASS_OBJECT ? (CMacroWithObject *)this : nullptr);

            if (ConvertType == MCT_BY_TYPE)
            {
                return "UseType: " + CAssistant::GraphicToText(withObject->Graphic);
            }
            if (ConvertType == MCT_BY_TYPE_COLOR)
            {
                return "UseTypeColor: " + CAssistant::GraphicToText(withObject->Graphic) +
                       ", " + CAssistant::GraphicToText(withObject->Color);
            }
            if (ConvertType == MCT_BY_TYPE_GROUND)
            {
                return "UseTypeGround: " + CAssistant::GraphicToText(withObject->Graphic);
            }
            if (ConvertType == MCT_BY_TYPE_COLOR_GROUND)
            {
                return "UseTypeColorGround: " + CAssistant::GraphicToText(withObject->Graphic) +
                       ", " + CAssistant::GraphicToText(withObject->Color);
            }
            if (ConvertType == MCT_LAST)
            {
                return "UseObject: last object";
            }
            if (ConvertType == MCT_SELF)
            {
                return "UseObject: self";
            }

            return "UseObject: " + CAssistant::SerialToText(withObject->Serial);
        }
        case MT_ATTACK:
        {
            if (ConvertType == MCT_LAST || ConvertType == MCT_LASTATTACK)
            {
                return "Attack: last attack";
            }
            if (ConvertType == MCT_LASTTARGET)
            {
                return "Attack: last target";
            }
            //if (ConvertType == MCT_FRIEND)
            //	return "Attack: friend";
            if (ConvertType == MCT_ENEMY)
            {
                return "Attack: enemy";
            }

            return "Attack: " +
                   CAssistant::SerialToText((GetClass() == MCLASS_OBJECT ? (CMacroWithObject *)this : nullptr)->Serial);
        }
        case MT_WAIT_FOR_TARGET_OBJECT:
        {
            char text[256] = { 0 };
            CMacroWithObject *withObject = (GetClass() == MCLASS_OBJECT ? (CMacroWithObject *)this : nullptr);

            if (ConvertType == MCT_BY_TYPE)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTarget: type 0x%04X (delay=%i ms)",
                    withObject->Graphic,
                    WaitingTime);
            }
            else if (ConvertType == MCT_BY_TYPE_COLOR)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTarget: type color 0x%04X, 0x%04X (delay=%i ms)",
                    withObject->Graphic,
                    withObject->Color,
                    WaitingTime);
            }
            else if (ConvertType == MCT_BY_TYPE_GROUND)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTargetGround: type 0x%04X (delay=%i ms)",
                    withObject->Graphic,
                    WaitingTime);
            }
            else if (ConvertType == MCT_BY_TYPE_COLOR_GROUND)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTargetGround: type color 0x%04X, 0x%04X (delay=%i ms)",
                    withObject->Graphic,
                    withObject->Color,
                    WaitingTime);
            }
            else if (ConvertType == MCT_LASTATTACK)
            {
                snprintf(text, sizeof(text), "WaitForTarget: last attack (delay=%i ms)", WaitingTime);
            }
            else if (ConvertType == MCT_LASTTARGET)
            {
                snprintf(text, sizeof(text), "WaitForTarget: last target (delay=%i ms)", WaitingTime);
            }
            else if (ConvertType == MCT_SELF)
            {
                snprintf(text, sizeof(text), "WaitForTarget: self (delay=%i ms)", WaitingTime);
            }
            else if (ConvertType == MCT_ENEMY)
            {
                snprintf(text, sizeof(text), "WaitForTarget: enemy (delay=%i ms)", WaitingTime);
            }
            else if (ConvertType == MCT_FRIEND)
            {
                snprintf(text, sizeof(text), "WaitForTarget: friend (delay=%i ms)", WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTarget: object 0x%08X (delay=%i ms)",
                    withObject->Serial,
                    WaitingTime);
            }

            return text;
        }
        case MT_WAIT_FOR_TARGET_TILE:
        {
            char text[128] = { 0 };

            if (ConvertType == MCT_LAST)
            {
                snprintf(text, sizeof(text), "WaitForTarget: last tile (delay=%i ms)", WaitingTime);
            }
            else if (ConvertType == MCT_BY_TARGET_RELATIVE)
            {
                snprintf(text, sizeof(text), "WaitForTarget: relative tile (delay=%i ms)", WaitingTime);
            }
            else
            {
                snprintf(text, sizeof(text), "WaitForTarget: tile (delay=%i ms)", WaitingTime);
            }

            return text;
        }
        case MT_USE_SKILL:
        {
            if (ConvertType == MCT_LAST)
            {
                return "UseSkill: last";
            }

            const CSkill *skill = g_SkillsManager.Get(
                (GetClass() == MCLASS_INT_VALUE ? (CMacroWithIntValue *)this : nullptr)->Value);

            return "UseSkill: " + (skill != nullptr ? skill->Name : astr_t());
        }
        case MT_CAST:
        case MT_CAST_NEW:
        case MT_CAST_FROM_BOOK:
        {
            if (ConvertType == MCT_LAST)
            {
                return "CastSpell: last";
            }

            int spell = (GetClass() == MCLASS_INT_VALUE ? (CMacroWithIntValue *)this : nullptr)->Value;
            int spellbook = spell / 100;
            int spellbooksCount = (int)std::min(g_SpellManager.m_SpellbookSpellNames.size(), (size_t)7);

            if (spellbook < spellbooksCount)
            {
                vector<astr_t> &list = g_SpellManager.m_SpellbookSpellNames[spellbook];
                spell %= 100;

                if (spell > 0 && spell <= (int)list.size())
                {
                    return "CastSpell: " + list[spell - 1];
                }
            }

            break;
        }
        case MT_SAY:
        {
            CMacroWithStringValue *withValue = (GetClass() == MCLASS_STRING_VALUE ? (CMacroWithStringValue *)this : nullptr);

            if (ConvertType == MCT_SAY_YELL)
            {
                return "Yell: " + withValue->Value;
            }
            if (ConvertType == MCT_SAY_WHISPER)
            {
                return "Whisper: " + withValue->Value;
            }
            if (ConvertType == MCT_SAY_EMOTE)
            {
                return "Emote: " + withValue->Value;
            }
            if (ConvertType == MCT_SAY_BROADCAST)
            {
                return "Broadcast: " + withValue->Value;
            }
            if (ConvertType == MCT_SAY_PARTY)
            {
                return "Party chat: " + withValue->Value;
            }
            if (ConvertType == MCT_SAY_GUILD)
            {
                return "Guild chat: " + withValue->Value;
            }
            if (ConvertType == MCT_SAY_ALLIANCE)
            {
                return "Alliance chat: " + withValue->Value;
            }

            return "Say: " + withValue->Value;
        }
        case MT_PICK_UP_ITEM:
        {
            return "PickUp: ";
        }
        case MT_DROP_ITEM:
        {
            return "Drop: ";
        }
        case MT_EQUIP_ITEM:
        {
            return "Equip: ";
        }
        case MT_WAIT_FOR_PROMPT:
            return "WaitForPrompt: " + ((CMacroWithStringValue *)this)->Value;
        case MT_WAIT_FOR_MENU:
        {
            char text[256] = { 0 };

            if (ConvertType == MCT_ANY)
            {
                snprintf(text, sizeof(text), "WaitForMenu: any (delay=%i ms)", WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForMenu: %s (delay=%i ms)",
                    ((CMacroWithMenu *)this)->Name.c_str(),
                    WaitingTime);
            }

            return text;
        }
        case MT_WAIT_FOR_GUMP:
        {
            char text[256] = { 0 };
            uint32_t button = ((CMacroWithGump *)this)->Button;

            if (ConvertType == MCT_ANY)
            {
                snprintf(text, sizeof(text), "WaitForGump: any, button:%i (delay=%i ms)", button, WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForGump: 0x%08X, button:%i (delay=%i ms)",
                    ((CMacroWithGump *)this)->ID,
                    button,
                    WaitingTime);
            }

            return text;
        }
        case MT_REQUEST_CONTEXT_MENU:
        {
            if (ConvertType == MCT_SELF)
            {
                return "RequestContextMenu: self";
            }
            if (ConvertType == MCT_LASTATTACK)
            {
                return "RequestContextMenu: last attack";
            }
            if (ConvertType == MCT_LASTTARGET)
            {
                return "RequestContextMenu: last target";
            }
            if (ConvertType == MCT_ENEMY)
            {
                return "RequestContextMenu: enemy";
            }
            if (ConvertType == MCT_FRIEND)
            {
                return "RequestContextMenu: friend";
            }

            return "RequestContextMenu: object " +
                   CAssistant::SerialToText(((CMacroWithObject *)this)->Serial);
        }
        case MT_WAIT_FOR_CONTEXT_MENU:
        {
            char text[256] = { 0 };

            if (ConvertType == MCT_ANY)
            {
                snprintf(text, sizeof(text), "WaitForContext menu: any (delay=%i ms)", WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForContext menu: object 0x%08X (delay=%i ms)",
                    ((CMacroWithObject *)this)->Serial,
                    WaitingTime);
            }

            return text;
        }
        case MT_SELECT_TRADE:
        {
            return "WaitForTrade: ";
        }
        case MT_WAR_MODE:
        {
            if (ConvertType == MCT_TOGGLE)
            {
                return "WarMode: toggle";
            }

            return astr_t("War mode: ") +
                   (((CMacroWithIntValue *)this)->Value != 0 ? "on" : "off");
        }
        case MT_WALK:
        {
            int val = ((CMacroWithIntValue *)this)->Value;

            astr_t text;

            const char *directionName[8] = { "North",    "North East", "East",  "South East",
                                             "South",    "South West", "West",  "North West" };

            if (val < 0)
            {
                val = -val;
                text = "Turn: " + astr_t(directionName[val & 7]);
            }
            else if ((val & 0x80) != 0)
            {
                text = "Run: " + astr_t(directionName[val & 7]);
            }
            else
            {
                text = "Walk: " + astr_t(directionName[val & 7]);
            }

            return text;
        }
        case MT_HELP_MENU_REQUEST:
            return "Help menu request";
        case MT_QUEST_MENU_REQUEST:
            return "Quest menu request";
        case MT_GUILD_MENU_REQUEST:
            return "Guild menu request";
        case MT_WRESTLING_STUN:
            return "Wrestling: stun";
        case MT_WRESTLING_DISARM:
            return "Wrestling: disarm";
        case MT_COMBAT_ABILITY:
        {
            if (ConvertType == MCT_PRIMARY)
            {
                return "Use ability: Primary";
            }
            return "Use ability: Secondary";
        }
        case MT_TOGGLE_GARGOYLE_FLYING:
            return "Toggle gargoyle flying";
        case MT_WAIT:
            return "Wait: " + str_from(WaitingTime) + " ms";
        case MT_SET_ARM:
            return "SetArm";
        case MT_DISARM:
            return "Disarm, delay: " + str_from(WaitingTime) + " ms";
        case MT_ARM:
            return "Arm, delay: " + str_from(WaitingTime) + " ms";
        case MT_SET_DRESS:
            return "SetDress";
        case MT_UNDRESS:
            return "Undress, delay: " + str_from(WaitingTime) + " ms";
        case MT_DRESS:
            return "Dress, delay: " + str_from(WaitingTime) + " ms";
        default:
            break;
    }

    return "unknown action";
}

bool CMacro::HaveConvertTable() const
{
    switch (Type)
    {
        case MT_USE:
        case MT_ATTACK:
        case MT_WAIT_FOR_TARGET_OBJECT:
        case MT_WAIT_FOR_TARGET_TILE:
        case MT_USE_SKILL:
        case MT_CAST:
        case MT_CAST_NEW:
        case MT_CAST_FROM_BOOK:
        case MT_SAY:
        case MT_PICK_UP_ITEM:
        case MT_DROP_ITEM:
        case MT_WAIT_FOR_MENU:
        case MT_WAIT_FOR_GUMP:
        case MT_REQUEST_CONTEXT_MENU:
        case MT_WAIT_FOR_CONTEXT_MENU:
        //case MT_SELECT_TRADE:
        case MT_WAR_MODE:
        case MT_WALK:
        case MT_DISARM:
        case MT_ARM:
        case MT_UNDRESS:
        case MT_DRESS:
        case MT_WAIT:
        case MT_COMBAT_ABILITY:
            return true;
        default:
            break;
    }

    return false;
}

CMacro *CMacro::Copy()
{
    return new CMacro(this);
}

CMacro *CMacroWithIntValue::Copy()
{
    return new CMacroWithIntValue(this);
}

CMacro *CMacroWithStringValue::Copy()
{
    return new CMacroWithStringValue(this);
}

CMacro *CMacroWithObject::Copy()
{
    return new CMacroWithObject(this);
}

CMacro *CMacroWithGump::Copy()
{
    return new CMacroWithGump(this);
}

CMacro *CMacroWithMenu::Copy()
{
    return new CMacroWithMenu(this);
}

CMacro *CMacroWithTargetTile::Copy()
{
    return new CMacroWithTargetTile(this);
}

CMacro *CMacro::Load(const CIniFile &ini, const astr_t &section)
{
    // Was CMacro::Load from the XML stream attributes - same keys.
    CMacro *result = nullptr;

    if (ini.Has(section, "type") && ini.Has(section, "convert_type"))
    {
        MACRO_TYPE type = (MACRO_TYPE)ini.GetInt(section, "type", 0);

        switch (type)
        {
            case MT_USE:
            case MT_ATTACK:
            case MT_WAIT_FOR_TARGET_OBJECT:
            case MT_REQUEST_CONTEXT_MENU:
            case MT_WAIT_FOR_CONTEXT_MENU:
            {
                if (ini.Has(section, "serial") && ini.Has(section, "graphic") &&
                    ini.Has(section, "color"))
                {
                    result = new CMacroWithObject(
                        type,
                        CAssistant::TextToSerial(ini.Get(section, "serial")),
                        CAssistant::TextToGraphic(ini.Get(section, "graphic")),
                        CAssistant::TextToGraphic(ini.Get(section, "color")));
                }

                break;
            }
            case MT_USE_SKILL:
            case MT_CAST:
            case MT_CAST_FROM_BOOK:
            case MT_CAST_NEW:
            case MT_WAR_MODE:
            case MT_WALK:
            {
                if (ini.Has(section, "value"))
                {
                    result = new CMacroWithIntValue(type, ini.GetInt(section, "value", 0));
                }

                break;
            }
            case MT_SAY:
            case MT_WAIT_FOR_PROMPT:
            {
                if (ini.Has(section, "value"))
                {
                    result = new CMacroWithStringValue(type, ini.Get(section, "value"));
                }

                break;
            }
            case MT_WAIT_FOR_GUMP:
            {
                if (ini.Has(section, "serial") && ini.Has(section, "id") &&
                    ini.Has(section, "button"))
                {
                    result = new CMacroWithGump(
                        type,
                        CAssistant::TextToSerial(ini.Get(section, "serial")),
                        CAssistant::TextToSerial(ini.Get(section, "id")),
                        ini.GetInt(section, "button", 0));
                }

                break;
            }
            case MT_WAIT_FOR_MENU:
            {
                if (ini.Has(section, "name") && ini.Has(section, "item_name") &&
                    ini.Has(section, "graphic") && ini.Has(section, "color"))
                {
                    result = new CMacroWithMenu(
                        type,
                        ini.Get(section, "name"),
                        ini.Get(section, "item_name"),
                        CAssistant::TextToGraphic(ini.Get(section, "graphic")),
                        CAssistant::TextToGraphic(ini.Get(section, "color")));
                }

                break;
            }
            case MT_WAIT_FOR_TARGET_TILE:
            {
                if (ini.Has(section, "graphic") && ini.Has(section, "x") &&
                    ini.Has(section, "y") && ini.Has(section, "z") &&
                    ini.Has(section, "relative_x") && ini.Has(section, "relative_y") &&
                    ini.Has(section, "relative_z"))
                {
                    result = new CMacroWithTargetTile(
                        type,
                        CAssistant::TextToGraphic(ini.Get(section, "graphic")),
                        ini.GetInt(section, "x", 0),
                        ini.GetInt(section, "y", 0),
                        ini.GetInt(section, "z", 0),
                        ini.GetInt(section, "relative_x", 0),
                        ini.GetInt(section, "relative_y", 0),
                        ini.GetInt(section, "relative_z", 0));
                }

                break;
            }
            case MT_WAIT:
            case MT_HELP_MENU_REQUEST:
            case MT_QUEST_MENU_REQUEST:
            case MT_GUILD_MENU_REQUEST:
            case MT_SET_ARM:
            case MT_DISARM:
            case MT_ARM:
            case MT_SET_DRESS:
            case MT_UNDRESS:
            case MT_DRESS:
            case MT_WRESTLING_STUN:
            case MT_WRESTLING_DISARM:
            case MT_COMBAT_ABILITY:
            case MT_TOGGLE_GARGOYLE_FLYING:
            {
                result = new CMacro(type);
                break;
            }
            default:
                break;
        }

        if (result != nullptr)
        {
            result->ConvertType = (MACRO_CONVERT_TYPE)ini.GetInt(section, "convert_type", 0);

            if (ini.Has(section, "waiting_time"))
            {
                result->WaitingTime = ini.GetInt(section, "waiting_time", 0);
            }
        }
    }

    return result;
}

void CMacro::Save(CIniFile &ini, const astr_t &section)
{
    ini.SetInt(section, "type", (int)Type);
    ini.SetInt(section, "convert_type", (int)ConvertType);
    ini.SetInt(section, "waiting_time", WaitingTime);
}

void CMacroWithIntValue::Save(CIniFile &ini, const astr_t &section)
{
    CMacro::Save(ini, section);

    ini.SetInt(section, "value", Value);
}

void CMacroWithStringValue::Save(CIniFile &ini, const astr_t &section)
{
    CMacro::Save(ini, section);

    ini.Set(section, "value", Value);
}

void CMacroWithObject::Save(CIniFile &ini, const astr_t &section)
{
    CMacro::Save(ini, section);

    ini.Set(section, "serial", CAssistant::SerialToText(Serial));
    ini.Set(section, "graphic", CAssistant::GraphicToText(Graphic));
    ini.Set(section, "color", CAssistant::GraphicToText(Color));
}

void CMacroWithGump::Save(CIniFile &ini, const astr_t &section)
{
    CMacro::Save(ini, section);

    ini.Set(section, "serial", CAssistant::SerialToText(Serial));
    ini.Set(section, "id", CAssistant::SerialToText(ID));
    ini.SetInt(section, "button", (int)Button);
}

void CMacroWithMenu::Save(CIniFile &ini, const astr_t &section)
{
    CMacro::Save(ini, section);

    ini.Set(section, "name", Name);
    ini.Set(section, "item_name", ItemName);
    ini.Set(section, "graphic", CAssistant::GraphicToText(Graphic));
    ini.Set(section, "color", CAssistant::GraphicToText(Color));
}

void CMacroWithTargetTile::Save(CIniFile &ini, const astr_t &section)
{
    CMacro::Save(ini, section);

    ini.Set(section, "graphic", CAssistant::GraphicToText(Graphic));
    ini.SetInt(section, "x", X);
    ini.SetInt(section, "y", Y);
    ini.SetInt(section, "z", Z);
    ini.SetInt(section, "relative_x", RelativeX);
    ini.SetInt(section, "relative_y", RelativeY);
    ini.SetInt(section, "relative_z", RelativeZ);
}

} // namespace xuoassist
