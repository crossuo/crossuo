// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (macros/macro.cpp); GetText display strings and the
// XML attribute set are identical. The spell-name lookup uses the ported
// CSpellManager.

#include "macro.h"
#include "../core/assist.h"
#include "../managers/spellmanager.h"
#include "Managers/SkillsManager.h" // ::g_SkillsManager
#include <algorithm>

namespace xuoassist {

string CMacro::GetText()
{
    switch (m_Type)
    {
        case MT_USE:
        {
            CMacroWithObject *withObject = (GetClass() == MCLASS_OBJECT ? (CMacroWithObject *)this : nullptr);

            if (m_ConvertType == MCT_BY_TYPE)
            {
                return "UseType: " + CAssistant::GraphicToText(withObject->GetGraphic());
            }
            if (m_ConvertType == MCT_BY_TYPE_COLOR)
            {
                return "UseTypeColor: " + CAssistant::GraphicToText(withObject->GetGraphic()) +
                       ", " + CAssistant::GraphicToText(withObject->GetColor());
            }
            if (m_ConvertType == MCT_BY_TYPE_GROUND)
            {
                return "UseTypeGround: " + CAssistant::GraphicToText(withObject->GetGraphic());
            }
            if (m_ConvertType == MCT_BY_TYPE_COLOR_GROUND)
            {
                return "UseTypeColorGround: " + CAssistant::GraphicToText(withObject->GetGraphic()) +
                       ", " + CAssistant::GraphicToText(withObject->GetColor());
            }
            if (m_ConvertType == MCT_LAST)
            {
                return "UseObject: last object";
            }
            if (m_ConvertType == MCT_SELF)
            {
                return "UseObject: self";
            }

            return "UseObject: " + CAssistant::SerialToText(withObject->GetSerial());
        }
        case MT_ATTACK:
        {
            if (m_ConvertType == MCT_LAST || m_ConvertType == MCT_LASTATTACK)
            {
                return "Attack: last attack";
            }
            if (m_ConvertType == MCT_LASTTARGET)
            {
                return "Attack: last target";
            }
            //if (m_ConvertType == MCT_FRIEND)
            //	return "Attack: friend";
            if (m_ConvertType == MCT_ENEMY)
            {
                return "Attack: enemy";
            }

            return "Attack: " +
                   CAssistant::SerialToText((GetClass() == MCLASS_OBJECT ? (CMacroWithObject *)this : nullptr)->GetSerial());
        }
        case MT_WAIT_FOR_TARGET_OBJECT:
        {
            char text[256] = { 0 };
            CMacroWithObject *withObject = (GetClass() == MCLASS_OBJECT ? (CMacroWithObject *)this : nullptr);

            if (m_ConvertType == MCT_BY_TYPE)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTarget: type 0x%04X (delay=%i ms)",
                    withObject->GetGraphic(),
                    m_WaitingTime);
            }
            else if (m_ConvertType == MCT_BY_TYPE_COLOR)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTarget: type color 0x%04X, 0x%04X (delay=%i ms)",
                    withObject->GetGraphic(),
                    withObject->GetColor(),
                    m_WaitingTime);
            }
            else if (m_ConvertType == MCT_BY_TYPE_GROUND)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTargetGround: type 0x%04X (delay=%i ms)",
                    withObject->GetGraphic(),
                    m_WaitingTime);
            }
            else if (m_ConvertType == MCT_BY_TYPE_COLOR_GROUND)
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTargetGround: type color 0x%04X, 0x%04X (delay=%i ms)",
                    withObject->GetGraphic(),
                    withObject->GetColor(),
                    m_WaitingTime);
            }
            else if (m_ConvertType == MCT_LASTATTACK)
            {
                snprintf(text, sizeof(text), "WaitForTarget: last attack (delay=%i ms)", m_WaitingTime);
            }
            else if (m_ConvertType == MCT_LASTTARGET)
            {
                snprintf(text, sizeof(text), "WaitForTarget: last target (delay=%i ms)", m_WaitingTime);
            }
            else if (m_ConvertType == MCT_SELF)
            {
                snprintf(text, sizeof(text), "WaitForTarget: self (delay=%i ms)", m_WaitingTime);
            }
            else if (m_ConvertType == MCT_ENEMY)
            {
                snprintf(text, sizeof(text), "WaitForTarget: enemy (delay=%i ms)", m_WaitingTime);
            }
            else if (m_ConvertType == MCT_FRIEND)
            {
                snprintf(text, sizeof(text), "WaitForTarget: friend (delay=%i ms)", m_WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForTarget: object 0x%08X (delay=%i ms)",
                    withObject->GetSerial(),
                    m_WaitingTime);
            }

            return text;
        }
        case MT_WAIT_FOR_TARGET_TILE:
        {
            char text[128] = { 0 };

            if (m_ConvertType == MCT_LAST)
            {
                snprintf(text, sizeof(text), "WaitForTarget: last tile (delay=%i ms)", m_WaitingTime);
            }
            else if (m_ConvertType == MCT_BY_TARGET_RELATIVE)
            {
                snprintf(text, sizeof(text), "WaitForTarget: relative tile (delay=%i ms)", m_WaitingTime);
            }
            else
            {
                snprintf(text, sizeof(text), "WaitForTarget: tile (delay=%i ms)", m_WaitingTime);
            }

            return text;
        }
        case MT_USE_SKILL:
        {
            if (m_ConvertType == MCT_LAST)
            {
                return "UseSkill: last";
            }

            const CSkill *skill = g_SkillsManager.Get(
                (GetClass() == MCLASS_INT_VALUE ? (CMacroWithIntValue *)this : nullptr)->GetValue());

            return "UseSkill: " + (skill != nullptr ? skill->Name : string());
        }
        case MT_CAST:
        case MT_CAST_NEW:
        case MT_CAST_FROM_BOOK:
        {
            if (m_ConvertType == MCT_LAST)
            {
                return "CastSpell: last";
            }

            int spell = (GetClass() == MCLASS_INT_VALUE ? (CMacroWithIntValue *)this : nullptr)->GetValue();
            int spellbook = spell / 100;
            int spellbooksCount = (int)std::min(g_SpellManager.m_SpellbookSpellNames.size(), (size_t)7);

            if (spellbook < spellbooksCount)
            {
                vector<string> &list = g_SpellManager.m_SpellbookSpellNames[spellbook];
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

            if (m_ConvertType == MCT_SAY_YELL)
            {
                return "Yell: " + withValue->GetValue();
            }
            if (m_ConvertType == MCT_SAY_WHISPER)
            {
                return "Whisper: " + withValue->GetValue();
            }
            if (m_ConvertType == MCT_SAY_EMOTE)
            {
                return "Emote: " + withValue->GetValue();
            }
            if (m_ConvertType == MCT_SAY_BROADCAST)
            {
                return "Broadcast: " + withValue->GetValue();
            }
            if (m_ConvertType == MCT_SAY_PARTY)
            {
                return "Party chat: " + withValue->GetValue();
            }
            if (m_ConvertType == MCT_SAY_GUILD)
            {
                return "Guild chat: " + withValue->GetValue();
            }
            if (m_ConvertType == MCT_SAY_ALLIANCE)
            {
                return "Alliance chat: " + withValue->GetValue();
            }

            return "Say: " + withValue->GetValue();
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
            return "WaitForPrompt: " + ((CMacroWithStringValue *)this)->GetValue();
        case MT_WAIT_FOR_MENU:
        {
            char text[256] = { 0 };

            if (m_ConvertType == MCT_ANY)
            {
                snprintf(text, sizeof(text), "WaitForMenu: any (delay=%i ms)", m_WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForMenu: %s (delay=%i ms)",
                    ((CMacroWithMenu *)this)->GetName().c_str(),
                    m_WaitingTime);
            }

            return text;
        }
        case MT_WAIT_FOR_GUMP:
        {
            char text[256] = { 0 };
            uint button = ((CMacroWithGump *)this)->GetButton();

            if (m_ConvertType == MCT_ANY)
            {
                snprintf(text, sizeof(text), "WaitForGump: any, button:%i (delay=%i ms)", button, m_WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForGump: 0x%08X, button:%i (delay=%i ms)",
                    ((CMacroWithGump *)this)->GetID(),
                    button,
                    m_WaitingTime);
            }

            return text;
        }
        case MT_REQUEST_CONTEXT_MENU:
        {
            if (m_ConvertType == MCT_SELF)
            {
                return "RequestContextMenu: self";
            }
            if (m_ConvertType == MCT_LASTATTACK)
            {
                return "RequestContextMenu: last attack";
            }
            if (m_ConvertType == MCT_LASTTARGET)
            {
                return "RequestContextMenu: last target";
            }
            if (m_ConvertType == MCT_ENEMY)
            {
                return "RequestContextMenu: enemy";
            }
            if (m_ConvertType == MCT_FRIEND)
            {
                return "RequestContextMenu: friend";
            }

            return "RequestContextMenu: object " +
                   CAssistant::SerialToText(((CMacroWithObject *)this)->GetSerial());
        }
        case MT_WAIT_FOR_CONTEXT_MENU:
        {
            char text[256] = { 0 };

            if (m_ConvertType == MCT_ANY)
            {
                snprintf(text, sizeof(text), "WaitForContext menu: any (delay=%i ms)", m_WaitingTime);
            }
            else
            {
                snprintf(
                    text,
                    sizeof(text),
                    "WaitForContext menu: object 0x%08X (delay=%i ms)",
                    ((CMacroWithObject *)this)->GetSerial(),
                    m_WaitingTime);
            }

            return text;
        }
        case MT_SELECT_TRADE:
        {
            return "WaitForTrade: ";
        }
        case MT_WAR_MODE:
        {
            if (m_ConvertType == MCT_TOGGLE)
            {
                return "WarMode: toggle";
            }

            return string("War mode: ") +
                   (((CMacroWithIntValue *)this)->GetValue() != 0 ? "on" : "off");
        }
        case MT_WALK:
        {
            int val = ((CMacroWithIntValue *)this)->GetValue();

            string text;

            const char *directionName[8] = { "North",    "North East", "East",  "South East",
                                             "South",    "South West", "West",  "North West" };

            if (val < 0)
            {
                val = -val;
                text = "Turn: " + string(directionName[val & 7]);
            }
            else if ((val & 0x80) != 0)
            {
                text = "Run: " + string(directionName[val & 7]);
            }
            else
            {
                text = "Walk: " + string(directionName[val & 7]);
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
            if (m_ConvertType == MCT_PRIMARY)
            {
                return "Use ability: Primary";
            }
            return "Use ability: Secondary";
        }
        case MT_TOGGLE_GARGOYLE_FLYING:
            return "Toggle gargoyle flying";
        case MT_WAIT:
            return "Wait: " + std::to_string(m_WaitingTime) + " ms";
        case MT_SET_ARM:
            return "SetArm";
        case MT_DISARM:
            return "Disarm, delay: " + std::to_string(m_WaitingTime) + " ms";
        case MT_ARM:
            return "Arm, delay: " + std::to_string(m_WaitingTime) + " ms";
        case MT_SET_DRESS:
            return "SetDress";
        case MT_UNDRESS:
            return "Undress, delay: " + std::to_string(m_WaitingTime) + " ms";
        case MT_DRESS:
            return "Dress, delay: " + std::to_string(m_WaitingTime) + " ms";
        default:
            break;
    }

    return "unknown action";
}

bool CMacro::HaveConvertTable()
{
    switch (m_Type)
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

CMacro *CMacro::Load(const XMLElement &element)
{
    // Was CMacro::Load from the XML stream attributes - same attributes.
    CMacro *result = nullptr;

    if (element.HasAttribute("type") && element.HasAttribute("convert_type"))
    {
        MACRO_TYPE type = (MACRO_TYPE)element.AttributeInt("type");

        switch (type)
        {
            case MT_USE:
            case MT_ATTACK:
            case MT_WAIT_FOR_TARGET_OBJECT:
            case MT_REQUEST_CONTEXT_MENU:
            case MT_WAIT_FOR_CONTEXT_MENU:
            {
                if (element.HasAttribute("serial") && element.HasAttribute("graphic") &&
                    element.HasAttribute("color"))
                {
                    result = new CMacroWithObject(
                        type,
                        CAssistant::TextToSerial(element.Attribute("serial")),
                        CAssistant::TextToGraphic(element.Attribute("graphic")),
                        CAssistant::TextToGraphic(element.Attribute("color")));
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
                if (element.HasAttribute("value"))
                {
                    result = new CMacroWithIntValue(type, element.AttributeInt("value"));
                }

                break;
            }
            case MT_SAY:
            case MT_WAIT_FOR_PROMPT:
            {
                if (element.HasAttribute("value"))
                {
                    result = new CMacroWithStringValue(type, element.Attribute("value"));
                }

                break;
            }
            case MT_WAIT_FOR_GUMP:
            {
                if (element.HasAttribute("serial") && element.HasAttribute("id") &&
                    element.HasAttribute("button"))
                {
                    result = new CMacroWithGump(
                        type,
                        CAssistant::TextToSerial(element.Attribute("serial")),
                        CAssistant::TextToSerial(element.Attribute("id")),
                        element.AttributeInt("button"));
                }

                break;
            }
            case MT_WAIT_FOR_MENU:
            {
                if (element.HasAttribute("name") && element.HasAttribute("item_name") &&
                    element.HasAttribute("graphic") && element.HasAttribute("color"))
                {
                    result = new CMacroWithMenu(
                        type,
                        element.Attribute("name"),
                        element.Attribute("item_name"),
                        CAssistant::TextToGraphic(element.Attribute("graphic")),
                        CAssistant::TextToGraphic(element.Attribute("color")));
                }

                break;
            }
            case MT_WAIT_FOR_TARGET_TILE:
            {
                if (element.HasAttribute("graphic") && element.HasAttribute("x") &&
                    element.HasAttribute("y") && element.HasAttribute("z") &&
                    element.HasAttribute("relative_x") && element.HasAttribute("relative_y") &&
                    element.HasAttribute("relative_z"))
                {
                    result = new CMacroWithTargetTile(
                        type,
                        CAssistant::TextToGraphic(element.Attribute("graphic")),
                        element.AttributeInt("x"),
                        element.AttributeInt("y"),
                        element.AttributeInt("z"),
                        element.AttributeInt("relative_x"),
                        element.AttributeInt("relative_y"),
                        element.AttributeInt("relative_z"));
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
            result->SetConvertType((MACRO_CONVERT_TYPE)element.AttributeInt("convert_type"));

            if (element.HasAttribute("waiting_time"))
            {
                result->SetWaitingTime(element.AttributeInt("waiting_time"));
            }
        }
    }

    return result;
}

void CMacro::Save(XMLElement &element)
{
    element.SetAttribute("type", std::to_string((int)m_Type));
    element.SetAttribute("convert_type", std::to_string((int)m_ConvertType));
    element.SetAttribute("waiting_time", std::to_string(m_WaitingTime));
}

void CMacroWithIntValue::Save(XMLElement &element)
{
    CMacro::Save(element);

    element.SetAttribute("value", std::to_string(m_Value));
}

void CMacroWithStringValue::Save(XMLElement &element)
{
    CMacro::Save(element);

    element.SetAttribute("value", m_Value);
}

void CMacroWithObject::Save(XMLElement &element)
{
    CMacro::Save(element);

    element.SetAttribute("serial", CAssistant::SerialToText(m_Serial));
    element.SetAttribute("graphic", CAssistant::GraphicToText(m_Graphic));
    element.SetAttribute("color", CAssistant::GraphicToText(m_Color));
}

void CMacroWithGump::Save(XMLElement &element)
{
    CMacro::Save(element);

    element.SetAttribute("serial", CAssistant::SerialToText(m_Serial));
    element.SetAttribute("id", CAssistant::SerialToText(m_ID));
    element.SetAttribute("button", std::to_string(m_Button));
}

void CMacroWithMenu::Save(XMLElement &element)
{
    CMacro::Save(element);

    element.SetAttribute("name", m_Name);
    element.SetAttribute("item_name", m_ItemName);
    element.SetAttribute("graphic", CAssistant::GraphicToText(m_Graphic));
    element.SetAttribute("color", CAssistant::GraphicToText(m_Color));
}

void CMacroWithTargetTile::Save(XMLElement &element)
{
    CMacro::Save(element);

    element.SetAttribute("graphic", CAssistant::GraphicToText(m_Graphic));
    element.SetAttribute("x", std::to_string(m_X));
    element.SetAttribute("y", std::to_string(m_Y));
    element.SetAttribute("z", std::to_string(m_Z));
}

} // namespace xuoassist
