// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// MacroToScript: one JS statement per recorded action against the Client.
// bindings registered in script/apiscript.cpp. Serial references go through
// the GetSerial alias strings (core/textconv.cpp TextToSerial) or the
// FindFriend/FindEnemy scans; raw serials/graphic ids stay hex literals.

#define LOGGER_MODULE Assistant

#include "macroscriptgen.h"

#if defined(XUO_ASSISTANT)

#include <stdio.h>

#include "../../Managers/SkillsManager.h" // g_SkillsManager (skill name lookup)

namespace xuoassist {

namespace {

// a JS double-quoted string literal with quotes, backslashes and newlines
// escaped (control characters as \xNN)
//
// bytes >= 0x80 pass through verbatim: macro text is UTF-8 (it flows
// through the client's UTF-8 text engine), so multi-byte sequences must
// not be split; invalid UTF-8 would make the QuickJS parser reject the
// string literal
astr_t js_string(const astr_t &text)
{
    astr_t result = "\"";
    for (char c : text)
    {
        switch (c)
        {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if ((unsigned char)c < 0x20)
                {
                    char buf[8] = { 0 };
                    snprintf(buf, sizeof(buf), "\\x%02X", (unsigned char)c);
                    result += buf;
                }
                else
                {
                    result += c;
                }
                break;
        }
    }
    result += "\"";
    return result;
}

astr_t hex32(uint32_t value)
{
    char buf[16] = { 0 };
    snprintf(buf, sizeof(buf), "0x%08X", value);
    return buf;
}

astr_t hex16(uint16_t value)
{
    char buf[8] = { 0 };
    snprintf(buf, sizeof(buf), "0x%04X", value);
    return buf;
}

// serial reference: the alias string GetSerial/TextToSerial understands
// (rendered as a JS literal) or the FindFriend/FindEnemy scan; anything
// else is a raw serial literal
astr_t serial_expression(MACRO_CONVERT_TYPE convertType, uint32_t serial)
{
    switch (convertType)
    {
        case MCT_SELF:
            return "\"self\"";
        case MCT_LAST:
            return "\"lastobject\"";
        case MCT_LASTATTACK:
            return "\"lastattack\"";
        case MCT_LASTTARGET:
            return "\"lasttarget\"";
        case MCT_LASTCONTAINER:
            return "\"lastcontainer\"";
        case MCT_BACKPACK:
            return "\"backpack\"";
        case MCT_ENEMY:
            return "Client.FindEnemy()";
        case MCT_FRIEND:
            return "Client.FindFriend()";
        default:
            return hex32(serial);
    }
}

astr_t placeholder(const CMacro *action)
{
    return "// " + action->GetText() + " (not convertible)";
}

const CMacroWithObject *object_of(const CMacro *action)
{
    return action->GetClass() == CMacro::MCLASS_OBJECT ? (const CMacroWithObject *)action : nullptr;
}

const CMacroWithIntValue *int_of(const CMacro *action)
{
    return action->GetClass() == CMacro::MCLASS_INT_VALUE ? (const CMacroWithIntValue *)action : nullptr;
}

const CMacroWithStringValue *string_of(const CMacro *action)
{
    return action->GetClass() == CMacro::MCLASS_STRING_VALUE ? (const CMacroWithStringValue *)action : nullptr;
}

astr_t action_statement(const CMacro *action)
{
    switch (action->Type)
    {
        case MT_WAIT:
            return "Client.Wait(" + str_from(action->WaitingTime) + ");";
        case MT_SAY:
        {
            const CMacroWithStringValue *value = string_of(action);
            if (value == nullptr)
            {
                return placeholder(action);
            }
            const char *name = "Say";
            switch (action->ConvertType)
            {
                case MCT_SAY_YELL:
                    name = "SayYell";
                    break;
                case MCT_SAY_WHISPER:
                    name = "SayWhisper";
                    break;
                case MCT_SAY_EMOTE:
                    name = "SayEmote";
                    break;
                case MCT_SAY_BROADCAST:
                    name = "SayBroadcast";
                    break;
                case MCT_SAY_PARTY:
                    name = "SayParty";
                    break;
                case MCT_SAY_GUILD:
                    name = "SayGuild";
                    break;
                case MCT_SAY_ALLIANCE:
                    name = "SayAlliance";
                    break;
                default:
                    break;
            }
            return astr_t("Client.") + name + "(" + js_string(value->Value) + ");";
        }
        case MT_USE:
        {
            const CMacroWithObject *object = object_of(action);
            if (object == nullptr)
            {
                return placeholder(action);
            }
            switch (action->ConvertType)
            {
                case MCT_BY_TYPE:
                    return "Client.UseType(" + hex16(object->Graphic) + ");";
                case MCT_BY_TYPE_COLOR:
                    return "Client.UseType(" + hex16(object->Graphic) + ", " + hex16(object->Color) + ");";
                case MCT_BY_TYPE_GROUND:
                    return "Client.UseFromGround(" + hex16(object->Graphic) + ");";
                case MCT_BY_TYPE_COLOR_GROUND:
                    return "Client.UseFromGround(" + hex16(object->Graphic) + ", " + hex16(object->Color) + ");";
                case MCT_SELF:
                    return "Client.UseObject(\"self\");";
                default:
                    return "Client.UseObject(" + serial_expression(action->ConvertType, object->Serial) + ");";
            }
        }
        case MT_ATTACK:
        {
            const CMacroWithObject *object = object_of(action);
            if (object == nullptr)
            {
                return placeholder(action);
            }
            return "Client.Attack(" + serial_expression(action->ConvertType, object->Serial) + ");";
        }
        case MT_WAIT_FOR_TARGET_OBJECT:
        {
            const CMacroWithObject *object = object_of(action);
            if (object == nullptr)
            {
                return placeholder(action);
            }
            switch (action->ConvertType)
            {
                case MCT_BY_TYPE:
                    return "Client.WaitTargetType(" + hex16(object->Graphic) + ");";
                case MCT_BY_TYPE_COLOR:
                    return "Client.WaitTargetType(" + hex16(object->Graphic) + ", " + hex16(object->Color) + ");";
                case MCT_BY_TYPE_GROUND:
                    return "Client.WaitTargetGround(" + hex16(object->Graphic) + ");";
                case MCT_BY_TYPE_COLOR_GROUND:
                    return "Client.WaitTargetGround(" + hex16(object->Graphic) + ", " + hex16(object->Color) + ");";
                default:
                    return (
                        "Client.WaitTargetObject(" +
                        serial_expression(action->ConvertType, object->Serial) + ");");
            }
        }
        case MT_WAIT_FOR_TARGET_TILE:
        {
            if (action->GetClass() != CMacro::MCLASS_TARGET_TILE)
            {
                return placeholder(action);
            }
            const CMacroWithTargetTile *tile = (const CMacroWithTargetTile *)action;
            if (action->ConvertType == MCT_BY_TARGET_RELATIVE)
            {
                return (
                    "Client.WaitTargetTileRelative(" + str_from(tile->RelativeX) + ", " +
                    str_from(tile->RelativeY) + ", " + str_from(tile->RelativeZ) + ");");
            }
            if (action->ConvertType == MCT_LAST)
            {
                return placeholder(action);
            }
            return (
                "Client.WaitTargetTile(" + str_from(tile->X) + ", " + str_from(tile->Y) + ", " +
                str_from(tile->Z) + ");");
        }
        case MT_USE_SKILL:
        {
            // resolve the recorded index to the skill name: the named form
            // keeps the script readable and immune to skill-table reordering
            // (Find matches by name, partial and case-insensitive); MCT_LAST
            // carries the concrete index too, so it resolves the same way
            const CMacroWithIntValue *value = int_of(action);
            if (value == nullptr)
            {
                return placeholder(action);
            }
            const CSkill *skill = g_SkillsManager.Get((uint32_t)value->Value);
            if (skill == nullptr)
            {
                return "Client.UseSkill(" + str_from(value->Value) + ");";
            }
            return "Client.UseSkill(" + js_string(skill->Name) + ");";
        }
        case MT_CAST:
        case MT_CAST_NEW:
        case MT_CAST_FROM_BOOK:
        {
            const CMacroWithIntValue *value = int_of(action);
            if (value == nullptr || action->ConvertType == MCT_LAST)
            {
                return placeholder(action);
            }
            return "Client.Cast(" + str_from(value->Value) + ");";
        }
        case MT_WAIT_FOR_PROMPT:
        {
            const CMacroWithStringValue *value = string_of(action);
            if (value == nullptr)
            {
                return placeholder(action);
            }
            return "Client.WaitPrompt(" + js_string(value->Value) + ");";
        }
        case MT_WAIT_FOR_MENU:
        {
            if (action->GetClass() != CMacro::MCLASS_MENU || action->ConvertType == MCT_ANY)
            {
                return placeholder(action);
            }
            const CMacroWithMenu *menu = (const CMacroWithMenu *)action;
            return (
                "Client.WaitMenu(" + js_string(menu->Name) + ", " + js_string(menu->ItemName) +
                ");");
        }
        case MT_WAIT_FOR_GUMP:
            // the WaitGump binding arms a script-built GumpHook (button
            // indexes, not gump ids) - the recorded serial/id/button triple
            // has no single-call equivalent
            return placeholder(action);
        case MT_REQUEST_CONTEXT_MENU:
        {
            const CMacroWithObject *object = object_of(action);
            if (object == nullptr)
            {
                return placeholder(action);
            }
            return (
                "Client.RequestContextMenu(" +
                serial_expression(action->ConvertType, object->Serial) + ");");
        }
        case MT_WAIT_FOR_CONTEXT_MENU:
        {
            if (action->ConvertType == MCT_ANY)
            {
                return placeholder(action);
            }
            const CMacroWithObject *object = object_of(action);
            if (object == nullptr)
            {
                return placeholder(action);
            }
            return (
                "Client.WaitContextMenu(" +
                serial_expression(action->ConvertType, object->Serial) + ", " +
                str_from(action->WaitingTime) + ");");
        }
        case MT_WAR_MODE:
        {
            if (action->ConvertType == MCT_TOGGLE)
            {
                return "Client.WarMode();";
            }
            const CMacroWithIntValue *value = int_of(action);
            if (value == nullptr)
            {
                return placeholder(action);
            }
            return value->Value != 0 ? "Client.WarMode(\"on\");" : "Client.WarMode(\"off\");";
        }
        case MT_WALK:
        {
            const CMacroWithIntValue *value = int_of(action);
            if (value == nullptr)
            {
                return placeholder(action);
            }
            int dir = value->Value;
            if (dir < 0)
            {
                // the recorded action turns in place; Step always moves a tile
                return placeholder(action);
            }
            if ((dir & 0x80) != 0)
            {
                return "Client.Step(" + str_from(dir & 7) + ", true);";
            }
            return "Client.Step(" + str_from(dir & 7) + ");";
        }
        case MT_HELP_MENU_REQUEST:
            return "Client.HelpGump();";
        case MT_QUEST_MENU_REQUEST:
        case MT_GUILD_MENU_REQUEST:
            return placeholder(action);
        case MT_WRESTLING_STUN:
            return "Client.UseWrestlingStun();";
        case MT_WRESTLING_DISARM:
            return "Client.UseWrestlingDisarm();";
        case MT_COMBAT_ABILITY:
            switch (action->ConvertType)
            {
                case MCT_PRIMARY:
                    return "Client.UseAbility(\"primary\");";
                case MCT_SECONDARY:
                    return "Client.UseAbility(\"secondary\");";
                default:
                    return placeholder(action);
            }
        case MT_TOGGLE_GARGOYLE_FLYING:
            return placeholder(action);
        case MT_PICK_UP_ITEM:
        case MT_DROP_ITEM:
        case MT_EQUIP_ITEM:
        case MT_SELECT_TRADE:
            return placeholder(action);
        case MT_SET_ARM:
            return "Client.SetArm();";
        case MT_DISARM:
            return "Client.Disarm();";
        case MT_ARM:
            return "Client.Arm();";
        case MT_SET_DRESS:
            return "Client.SetDress();";
        case MT_UNDRESS:
            return "Client.Undress();";
        case MT_DRESS:
            return "Client.Dress();";
        default:
            return placeholder(action);
    }
}

} // namespace

astr_t MacroToScript(const vector<CMacro *> &actions, const astr_t &name)
{
    // wrapped in a function: the script engine's function list (and the
    // editor/tab run controls) operate on declared functions, and the tab's
    // Start invokes the picked function by name
    astr_t script = "// Macro: " + name + "\n";
    script += "// Recorded actions converted to assistant script commands.\n";
    script += "function main()\n{\n";
    for (const CMacro *action : actions)
    {
        script += "    " + action_statement(action) + "\n";
    }
    script += "}\n";
    return script;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
