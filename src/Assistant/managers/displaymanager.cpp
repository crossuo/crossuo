// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The info-command table, the 26 default items, the display-key parser and
// the plain-text title-data compiler (the caption of the client window).

#include "displaymanager.h"
#include "ServerList.h" // g_ServerList.LastServerName (the shared server name)
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../config/assistantconfig.h"
#include "../core/textconv.h"
#include "../core/world.h" // g_Player / g_World
#include "../core/worldutil.h"
#include <common/logging/logging.h>

namespace xuoassist {

CDisplayManager g_DisplayManager;

// The info-command catalog: the rows of the type combobox, the title-data tag
// lookup and the per-type defaults a tag renders with before the per-tag
// text/id/hue/min/mid overrides apply.
// clang-format off
CDisplayManager::CDisplayManager()
{
    m_InfoCommands.push_back({ "", "", DIT_NONE, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Player name", "name", DIT_NAME, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Server name", "server", DIT_SERVER, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Hitpoints", "hp", DIT_HITS, "hp:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Max hitpoints", "maxhp", DIT_MAX_HITS, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Hitpoints / [Max hitpoints]", "hp_maxhp", DIT_HITS_MAX_HITS, "hp:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Mana", "mp", DIT_MANA, "mp:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Max mana", "maxmp", DIT_MAX_MANA, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Mana / [Max mana]", "mp_maxmp", DIT_MANA_MAX_MANA, "mp:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Stamina", "sp", DIT_STAM, "sp:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Max stamina", "maxsp", DIT_MAX_STAM, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Stamina / [Max stamina]", "sp_maxsp", DIT_STAM_MAX_STAM, "sp:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Strength", "str", DIT_STR, "str:", 0, 0, 10, 30 });
    m_InfoCommands.push_back({ "Intelligence", "int", DIT_INT, "int:", 0, 0, 10, 30 });
    m_InfoCommands.push_back({ "Dexterity", "dex", DIT_DEX, "dex:", 0, 0, 10, 30 });
    m_InfoCommands.push_back({ "Weight", "weight", DIT_WEIGHT, "w:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Max weight", "maxweight", DIT_MAX_WEIGHT, "", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Weight / [Max weight]", "weight_maxweight", DIT_WEIGHT_MAX_WEIGHT, "w:", 0, 0, 33, 66 });
    m_InfoCommands.push_back({ "Armor rating", "armor", DIT_ARMOR, "ar:", 0, 0, 20, 40 });
    m_InfoCommands.push_back({ "Gold", "gold", DIT_GOLD, "g:", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "X", "x", DIT_X, "x:", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Y", "y", DIT_Y, "y:", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Z", "z", DIT_Z, "z:", 0, 0, 0, 0 });
    m_InfoCommands.push_back({ "Any item counter", "item", DIT_OBJECT, "", 0, 0, 0, 0 });
}
// clang-format on

const DISPLAY_KEY_ITEM CDisplayManager::m_KeysList[DKI_COUNT] = {
    { "", DKI_NONE },
    { "text", DKI_TEXT },
    { "id", DKI_ID },
    { "hue", DKI_HUE },
    { "minval", DKI_MIN_VAULE },
    { "midval", DKI_MID_VALUE },
};

void CDisplayManager::GetValues(const DisplayInfo &item, astr_t &value, astr_t &maxValue)
{
    int iVal = 0;
    int iMaxVal = 0;

    switch (item.Type)
    {
        case DIT_NAME:
        {
            value = astr_t(g_SelectedCharName);
            return;
        }
        case DIT_SERVER:
        {
            value = g_ServerList.LastServerName;
            return;
        }
        case DIT_HITS:
        case DIT_MAX_HITS:
        case DIT_HITS_MAX_HITS:
        {
            iVal = g_Player->Hits;
            iMaxVal = g_Player->MaxHits;
            break;
        }
        case DIT_MANA:
        case DIT_MAX_MANA:
        case DIT_MANA_MAX_MANA:
        {
            iVal = g_Player->Mana;
            iMaxVal = g_Player->MaxMana;
            break;
        }
        case DIT_STAM:
        case DIT_MAX_STAM:
        case DIT_STAM_MAX_STAM:
        {
            iVal = g_Player->Stam;
            iMaxVal = g_Player->MaxStam;
            break;
        }
        case DIT_STR:
        {
            iVal = g_Player->Str;
            break;
        }
        case DIT_INT:
        {
            iVal = g_Player->Int;
            break;
        }
        case DIT_DEX:
        {
            iVal = g_Player->Dex;
            break;
        }
        case DIT_WEIGHT:
        case DIT_MAX_WEIGHT:
        case DIT_WEIGHT_MAX_WEIGHT:
        {
            iVal = g_Player->Weight;
            iMaxVal = g_Player->MaxWeight;
            break;
        }
        case DIT_ARMOR:
        {
            iVal = g_Player->Armor;
            break;
        }
        case DIT_GOLD:
        {
            iVal = g_Player->Gold;
            break;
        }
        case DIT_X:
        {
            value = str_from(g_Player->GetX());
            return;
        }
        case DIT_Y:
        {
            value = str_from(g_Player->GetY());
            return;
        }
        case DIT_Z:
        {
            value = str_from(g_Player->GetZ());
            return;
        }
        case DIT_OBJECT:
        {
            iVal = CountItems(g_Player, item.Graphic, item.Hue, true);
            break;
        }
        case DIT_NONE:
        {
            break; // plain text token: obj.Text already carries the content
        }
        default:
            return;
    }

    value = str_from(iVal);
    maxValue = str_from(iMaxVal);
}

bool CDisplayManager::CanRedraw(const uint16_t &graphic, const uint16_t &color) const
{
    for (const DisplayInfo &obj : m_Commands)
    {
        if (obj.Type == DIT_OBJECT && obj.Graphic == graphic &&
            (obj.Hue == 0xFFFF || obj.Hue == color))
        {
            return true;
        }
    }

    return false;
}

void CDisplayManager::Redraw(bool enabled)
{
    // Composes the caption text; the caller (the assistant UI tick) applies it
    // to the client window on change.
    astr_t title;

    if (!enabled || g_World == nullptr || g_Player == nullptr)
    {
        title = "Ultima Online";

        if (g_World == nullptr)
        {
            if (g_ServerList.LastServerName.length() != 0)
            {
                title += " - " + g_ServerList.LastServerName;
            }
        }
        else
        {
            title += " - " + astr_t(g_SelectedCharName) + " (" + g_ServerList.LastServerName + ")";
        }
    }
    else
    {
        for (const DisplayInfo &obj : m_Commands)
        {
            astr_t value = "";
            astr_t maxValue = "";

            GetValues(obj, value, maxValue);

            title += obj.Text;

            switch (obj.Type)
            {
                case DIT_MAX_HITS:
                case DIT_MAX_MANA:
                case DIT_MAX_STAM:
                case DIT_MAX_WEIGHT:
                {
                    title += maxValue;
                    break;
                }
                case DIT_HITS_MAX_HITS:
                case DIT_MANA_MAX_MANA:
                case DIT_STAM_MAX_STAM:
                case DIT_WEIGHT_MAX_WEIGHT:
                {
                    if (value != maxValue)
                    {
                        title += value + "/" + maxValue;
                    }
                    else
                    {
                        title += value;
                    }

                    break;
                }
                default:
                {
                    title += value;
                    break;
                }
            }
        }

        if (title.length() == 0)
        {
            title = "Ultima Online - " + astr_t(g_SelectedCharName) + " (" + g_ServerList.LastServerName + ")";
        }
    }

    m_TitleText = title;
}

DisplayInfo CDisplayManager::GetDisplayItemByName(const astr_t &name) const
{
    DisplayInfo item = g_AssistantConfig.GetCustomDisplayItemByName(name);

    if (item.Type == DIT_NONE)
    {
        for (const DisplayInfo &info : m_InfoCommands)
        {
            if (name == info.Command)
            {
                if (info.Type != DIT_OBJECT)
                {
                    return info;
                }

                break;
            }
        }
    }

    return item;
}

void CDisplayManager::ApplyAttributeToDisplayItem(
    DisplayInfo &obj, const astr_t &name, const astr_t &value)
{
    for (const DISPLAY_KEY_ITEM &key : m_KeysList)
    {
        if (name == key.Name)
        {
            switch (key.Identifier)
            {
                case DKI_TEXT:
                {
                    obj.Text = value;
                    break;
                }
                case DKI_ID:
                {
                    obj.Graphic = (uint16_t)CAssistant::TextToGraphic(str_lower(value));
                    break;
                }
                case DKI_HUE:
                {
                    obj.Hue = (uint16_t)CAssistant::TextToGraphic(str_lower(value));
                    break;
                }
                case DKI_MIN_VAULE:
                {
                    obj.MinValue = (int)CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                case DKI_MID_VALUE:
                {
                    obj.MidValue = (int)CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                default:
                    break;
            }

            break;
        }
    }
}

void CDisplayManager::RecompileData(const astr_t &text)
{
    g_UpdateDisplayOnStep = false;
    m_Commands.clear();

    CompileData(text);
}

void CDisplayManager::CompileData(const astr_t &text)
{
    // Tokenized line by line, one token list per line; the "{}" quote pair keeps the braces on tag
    // fragments so the tag branch below can recognize them.
    size_t pos = 0;

    while (pos < text.length())
    {
        const size_t eol = text.find('\n', pos);
        const astr_t line = text.substr(pos, (eol == astr_t::npos ? text.length() : eol) - pos);
        pos = (eol == astr_t::npos) ? text.length() : eol + 1;

        vector<astr_t> list = ParseTextTokens(line, "", "", "{}", false, true);

        for (const astr_t &token : list)
        {
            if (token.length() == 0)
            {
                continue;
            }

            astr_t str = token;

            if (str.at(0) == '{')
            {
                vector<astr_t> tags = ParseTextTokens(str, " \t{}=", "", "\"\"''");

                if (!tags.empty())
                {
                    astr_t lowerName = str_lower(*tags.begin());

                    if (lowerName == "group")
                    {
                        if (tags.size() > 1)
                        {
                            CompileData(g_AssistantConfig.GetGroupContent(str_lower(*(tags.begin() + 1))));
                        }

                        continue;
                    }

                    DisplayInfo tagData = GetDisplayItemByName(lowerName);

                    for (size_t t = 1; t < tags.size(); t++)
                    {
                        astr_t tagName = tags[t];

                        ++t;

                        if (t != tags.size())
                        {
                            ApplyAttributeToDisplayItem(tagData, str_lower(tagName), tags[t]);
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (tagData.Type >= DIT_X && tagData.Type <= DIT_Z)
                    {
                        g_UpdateDisplayOnStep = true;
                    }

                    m_Commands.push_back(tagData);
                }
            }
            else
            {
                DisplayInfo tag; // a DIT_NONE tag renders its Text as-is
                tag.Text = str;
                m_Commands.push_back(tag);
            }
        }
    }
}

} // namespace xuoassist
