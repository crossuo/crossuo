// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/displaymanager.cpp) - data-only port:
// the info-command table, the 26 default items, the display-key parser and
// the title-data compiler are here; the win32 GDI Redraw/icon bodies are
// Phase 2/4 (see displaymanager.h).

#include "displaymanager.h"
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../config/assistantconfig.h"
#include "../core/textparser.h"
#include "../model/gameobjects/gameplayer.h"
#include <common/logging/logging.h>
#include <map>
#include <stdlib.h>

namespace xuoassist {

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CDisplayManager g_DisplayManager;

// clang-format off
const CDisplayItem CDisplayManager::m_DefaultItems[26] = {
    //ABGR colors, A=00
    CDisplayItem(), //empty
    //                                 text         min         mid         norm        bg     bg min      mid        norm        min/max  percents
    CDisplayItem(DIT_NAME,                "", 0, 0, 0x00000000, 0x00000000, 0x00000001, false, 0x0000007F, 0x007F7F7F, 0x007F0000,  0,  0, false), //Player name
    CDisplayItem(DIT_SERVER,              "", 0, 0, 0x00000000, 0x00000000, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000,  0,  0, false), //Server name
    CDisplayItem(DIT_STATUSBAR_SMALL,     "", 0, 0, 0x00007F00, 0x00007F00, 0x00007F00, false, 0x00000000, 0x00000000, 0x00000000, 33, 66, false), //Statusbar small
    CDisplayItem(DIT_STATUSBAR_MIDDLE,    "", 0, 0, 0x00007F00, 0x00007F00, 0x00007F00, false, 0x00000000, 0x00000000, 0x00000000, 33, 66, false), //Statusbar middle
    CDisplayItem(DIT_STATUSBAR_LARGE,     "", 0, 0, 0x00007F00, 0x00007F00, 0x00007F00, false, 0x00000000, 0x00000000, 0x00000000, 33, 66, false), //Statusbar large
    CDisplayItem(DIT_HITS,             "hp:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 33, 66,  true), //Hitpoints
    CDisplayItem(DIT_MAX_HITS,            "", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000,  0,  0, false), //Max hitpoints
    CDisplayItem(DIT_HITS_MAX_HITS,    "hp:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 33, 66,  true), //Hitpoints / [Max hitpoints]
    CDisplayItem(DIT_MANA,             "mp:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 33, 66,  true), //Mana
    CDisplayItem(DIT_MAX_MANA,            "", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000,  0,  0, false), //Max mana
    CDisplayItem(DIT_MANA_MAX_MANA,    "mp:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 33, 66,  true), //Mana / [Max mana]
    CDisplayItem(DIT_STAM,             "sp:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 33, 66,  true), //Stamina
    CDisplayItem(DIT_MAX_STAM,            "", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000,  0,  0, false), //Max stamina
    CDisplayItem(DIT_STAM_MAX_STAM,    "sp:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 33, 66,  true), //Stamina / [Max stamina]
    CDisplayItem(DIT_STR,             "str:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000, 10, 30, false), //Stringth
    CDisplayItem(DIT_INT,             "int:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000, 10, 30, false), //Intelligence
    CDisplayItem(DIT_DEX,             "dex:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000, 10, 30, false), //Dexterity
    CDisplayItem(DIT_WEIGHT,            "w:", 0, 0, 0x00000001, 0x00000001, 0x00FFFFFF,  true, 0x00000000, 0x0000FFFF, 0x0000007F, 33, 66,  true), //Weight
    CDisplayItem(DIT_MAX_WEIGHT,          "", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000,  0,  0, false), //Max weight
    CDisplayItem(DIT_WEIGHT_MAX_WEIGHT, "w:", 0, 0, 0x00000001, 0x00000001, 0x00FFFFFF,  true, 0x00000000, 0x0000FFFF, 0x0000007F, 33, 66,  true), //Weight / [Max weight]
    CDisplayItem(DIT_ARMOR,            "ar:", 0, 0, 0x00FFFFFF, 0x00000001, 0x00000001,  true, 0x0000007F, 0x0000FFFF, 0x00000000, 20, 40, false), //Armor rating
    CDisplayItem(DIT_GOLD,              "g:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000,  0,  0, false), //Gold
    CDisplayItem(DIT_X,                 "x:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000,  0,  0, false), //X
    CDisplayItem(DIT_Y,                 "y:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000,  0,  0, false), //Y
    CDisplayItem(DIT_Z,                 "z:", 0, 0, 0x00000001, 0x00000001, 0x00000001, false, 0x00000000, 0x00000000, 0x00000000,  0,  0, false), //Z
};
// clang-format on

const DISPLAY_KEY_ITEM CDisplayManager::m_KeysList[DKI_COUNT] = {
    { "wtf key?", DKI_NONE },
    { "text", DKI_TEXT },
    { "id", DKI_ID },
    { "hue", DKI_HUE },
    { "mincolor", DKI_COLOR_MIN },
    { "midcolor", DKI_COLOR_MID },
    { "color", DKI_COLOR },
    { "background", DKI_BACKGROUND },
    { "bgmincolor", DKI_BG_COLOR_MIN },
    { "bgmidcolor", DKI_BG_COLOR_MID },
    { "bgcolor", DKI_BG_COLOR },
    { "minval", DKI_MIN_VAULE },
    { "midval", DKI_MID_VALUE },
    { "%", DKI_PERCENTS },
};

CDisplayManager::CDisplayManager()
{
    m_InfoCommands.push_back(CDisplayInfo("", "", DIT_NONE));
    m_InfoCommands.push_back(CDisplayInfo("Player name", "name", DIT_NAME));
    m_InfoCommands.push_back(CDisplayInfo("Server name", "server", DIT_SERVER));
    m_InfoCommands.push_back(CDisplayInfo("Statusbar small", "barsmall", DIT_STATUSBAR_SMALL));
    m_InfoCommands.push_back(CDisplayInfo("Statusbar middle", "barmiddle", DIT_STATUSBAR_MIDDLE));
    m_InfoCommands.push_back(CDisplayInfo("Statusbar large", "barlarge", DIT_STATUSBAR_LARGE));
    m_InfoCommands.push_back(CDisplayInfo("Hitpoints", "hp", DIT_HITS));
    m_InfoCommands.push_back(CDisplayInfo("Max hitpoints", "maxhp", DIT_MAX_HITS));
    m_InfoCommands.push_back(CDisplayInfo("Hitpoints / [Max hitpoints]", "hp_maxhp", DIT_HITS_MAX_HITS));
    m_InfoCommands.push_back(CDisplayInfo("Mana", "mp", DIT_MANA));
    m_InfoCommands.push_back(CDisplayInfo("Max mana", "maxmp", DIT_MAX_MANA));
    m_InfoCommands.push_back(CDisplayInfo("Mana / [Max mana]", "mp_maxmp", DIT_MANA_MAX_MANA));
    m_InfoCommands.push_back(CDisplayInfo("Stamina", "sp", DIT_STAM));
    m_InfoCommands.push_back(CDisplayInfo("Max stamina", "maxsp", DIT_MAX_STAM));
    m_InfoCommands.push_back(CDisplayInfo("Stamina / [Max stamina]", "sp_maxsp", DIT_STAM_MAX_STAM));
    m_InfoCommands.push_back(CDisplayInfo("Strength", "str", DIT_STR));
    m_InfoCommands.push_back(CDisplayInfo("Intelligence", "int", DIT_INT));
    m_InfoCommands.push_back(CDisplayInfo("Dexterity", "dex", DIT_DEX));
    m_InfoCommands.push_back(CDisplayInfo("Weight", "weight", DIT_WEIGHT));
    m_InfoCommands.push_back(CDisplayInfo("Max weight", "maxweight", DIT_MAX_WEIGHT));
    m_InfoCommands.push_back(CDisplayInfo("Weight / [Max weight]", "weight_maxweight", DIT_WEIGHT_MAX_WEIGHT));
    m_InfoCommands.push_back(CDisplayInfo("Armor rating", "armor", DIT_ARMOR));
    m_InfoCommands.push_back(CDisplayInfo("Gold", "gold", DIT_GOLD));
    m_InfoCommands.push_back(CDisplayInfo("X", "x", DIT_X));
    m_InfoCommands.push_back(CDisplayInfo("Y", "y", DIT_Y));
    m_InfoCommands.push_back(CDisplayInfo("Z", "z", DIT_Z));
    m_InfoCommands.push_back(CDisplayInfo("Any item counter", "item", DIT_OBJECT));
}

CDisplayManager::~CDisplayManager() {}

void CDisplayManager::ClearIcons()
{
    for (auto &icon : m_Icons)
    {
        delete icon.second;
    }
    m_Icons.clear();
}

CDisplayIcon *CDisplayManager::GetIcon(const CDisplayItem &item)
{
    // Was the win32 CreateBitmap icon cache fed from the assistant
    // filemanager's static art (LoadDisplayIcon).
    // PORT-TODO(phase-2/4): decode static art into the CDisplayIcon payload
    // once the filemanager dedup + rendering rounds land; the raw-bytes
    // struct is in place.
    (void)item;
    return nullptr;
}

void CDisplayManager::GetValues(
    const CDisplayItem &item,
    string &value,
    string &maxValue,
    uint textColor,
    uint backgroundColor,
    bool useNotoriety)
{
    int iVal = 0;
    int iMaxVal = 0;
    bool inPercents = false;

    switch (item.GetType())
    {
        case DIT_NAME:
        {
            value = g_CharacterName;
            backgroundColor = 0;

            if (useNotoriety)
            {
                if (g_Player->GetNotoriety() == 3 || g_Player->GetNotoriety() == 4)
                {
                    textColor = item.GetBackgroundMidColor(); //gray
                }
                else if (g_Player->GetNotoriety() == 5 || g_Player->GetNotoriety() == 6)
                {
                    textColor = item.GetBackgroundMinColor(); //red
                }
                else
                {
                    textColor = item.GetBackgroundColor(); //blue
                }
            }
            else
            {
                textColor = item.GetColor();
            }

            return;
        }
        case DIT_SERVER:
        {
            value = g_ServerName;
            textColor = item.GetColor();
            backgroundColor = item.GetBackgroundColor();
            return;
        }
        case DIT_HITS:
        case DIT_MAX_HITS:
        case DIT_HITS_MAX_HITS:
        {
            iVal = g_Player->GetHits();
            iMaxVal = g_Player->GetMaxHits();
            inPercents = (item.GetPercents() && item.GetType() != DIT_MAX_HITS);
            break;
        }
        case DIT_MANA:
        case DIT_MAX_MANA:
        case DIT_MANA_MAX_MANA:
        {
            iVal = g_Player->GetMana();
            iMaxVal = g_Player->GetMaxMana();
            inPercents = (item.GetPercents() && item.GetType() != DIT_MAX_MANA);
            break;
        }
        case DIT_STAM:
        case DIT_MAX_STAM:
        case DIT_STAM_MAX_STAM:
        {
            iVal = g_Player->GetStam();
            iMaxVal = g_Player->GetMaxStam();
            inPercents = (item.GetPercents() && item.GetType() != DIT_MAX_STAM);
            break;
        }
        case DIT_STR:
        {
            iVal = g_Player->GetStr();
            break;
        }
        case DIT_INT:
        {
            iVal = g_Player->GetInt();
            break;
        }
        case DIT_DEX:
        {
            iVal = g_Player->GetDex();
            break;
        }
        case DIT_WEIGHT:
        case DIT_MAX_WEIGHT:
        case DIT_WEIGHT_MAX_WEIGHT:
        {
            iVal = g_Player->GetWeight();
            iMaxVal = g_Player->GetMaxWeight();
            inPercents = (item.GetPercents() && item.GetType() != DIT_MAX_WEIGHT);
            break;
        }
        case DIT_ARMOR:
        {
            iVal = g_Player->GetArmor();
            break;
        }
        case DIT_GOLD:
        {
            iVal = g_Player->GetGold();
            break;
        }
        case DIT_X:
        {
            value = std::to_string(g_Player->GetX());
            textColor = item.GetColor();
            backgroundColor = item.GetBackgroundColor();
            return;
        }
        case DIT_Y:
        {
            value = std::to_string(g_Player->GetY());
            textColor = item.GetColor();
            backgroundColor = item.GetBackgroundColor();
            return;
        }
        case DIT_Z:
        {
            value = std::to_string(g_Player->GetZ());
            textColor = item.GetColor();
            backgroundColor = item.GetBackgroundColor();
            return;
        }
        case DIT_OBJECT:
        {
            iVal = g_Player->CountItems(item.GetGraphic(), item.GetHue(), true);
            break;
        }
        case DIT_NONE:
        {
            textColor = item.GetColor();
            break;
        }
        default:
            return;
    }

    value = std::to_string(iVal);
    maxValue = std::to_string(iMaxVal);

    if (inPercents && (iMaxVal != 0))
    {
        iVal = int((iVal * 100) / float(iMaxVal));
    }

    if (iVal <= item.GetMinValue())
    {
        textColor = item.GetMinColor();
        backgroundColor = item.GetBackgroundMinColor();
    }
    else if (iVal <= item.GetMidValue())
    {
        textColor = item.GetMidColor();
        backgroundColor = item.GetBackgroundMidColor();
    }
    else
    {
        textColor = item.GetColor();
        backgroundColor = item.GetBackgroundColor();
    }

    // The colors land in the drawn text/bg (Phase 2/4); the value logic above
    // is the part this round keeps.
    (void)textColor;
    (void)backgroundColor;
}

bool CDisplayManager::CanRedraw(const ushort &graphic, const ushort &color)
{
    for (size_t i = 0; i < m_Commands.size(); i++)
    {
        CDisplayItem &obj = m_Commands[i];

        if (obj.GetType() == DIT_OBJECT && obj.GetGraphic() == graphic &&
            (obj.GetHue() == 0xFFFF || obj.GetHue() == color))
        {
            return true;
        }
    }

    return false;
}

void CDisplayManager::Redraw(
    REDRAW_DISPLAY_MODE mode,
    bool useNotoriety,
    bool hightFont,
    bool highlightReagents,
    bool displayAppIcon,
    uint backgroundColor)
{
    // Was the win32 GDI title-bar painter (HBITMAPs, GetValues + blits).
    // PORT-TODO(phase-2/4): title rendering (ImGui overlay / client caption)
    // restores the body; the data half (GetValues/CompilleData) is live.
    (void)mode;
    (void)useNotoriety;
    (void)hightFont;
    (void)highlightReagents;
    (void)displayAppIcon;
    (void)backgroundColor;
}

CDisplayItem CDisplayManager::GetDisplayItemByName(const string &name)
{
    CDisplayItem item = g_AssistantConfig.GetCustomDisplayItemByName(name);

    if (item.GetType() == DIT_NONE)
    {
        for (size_t i = 0; i < m_InfoCommands.size(); i++)
        {
            if (name == m_InfoCommands[i].Command)
            {
                if (m_InfoCommands[i].Type != DIT_OBJECT)
                {
                    return m_DefaultItems[m_InfoCommands[i].Type];
                }
                //return CDisplayItem(m_DefaultItems[m_InfoCommands[i].Type]);

                break;
            }
        }
    }

    return item;
}

void CDisplayManager::ApplyAttributeToDisplayItem(
    CDisplayItem &obj, const string &name, const string &value)
{
    for (int i = 0; i < DKI_COUNT; i++)
    {
        if (name == m_KeysList[i].Name)
        {
            switch (m_KeysList[i].Identifier)
            {
                case DKI_TEXT:
                {
                    obj.SetText(value);
                    break;
                }
                case DKI_ID:
                {
                    obj.SetGraphic((ushort)CAssistant::TextToGraphic(to_lower_copy(value)));
                    break;
                }
                case DKI_HUE:
                {
                    obj.SetHue((ushort)CAssistant::TextToGraphic(to_lower_copy(value)));
                    break;
                }
                case DKI_COLOR_MIN:
                {
                    obj.SetMinColor(CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_COLOR_MID:
                {
                    obj.SetMidColor(CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_COLOR:
                {
                    obj.SetColor(CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_BACKGROUND:
                {
                    obj.SetBackground(CAssistant::RawStringToBool(to_lower_copy(value)));
                    break;
                }
                case DKI_BG_COLOR_MIN:
                {
                    obj.SetBackgroundMinColor(CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_BG_COLOR_MID:
                {
                    obj.SetBackgroundMidColor(CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_BG_COLOR:
                {
                    obj.SetBackgroundColor(CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_MIN_VAULE:
                {
                    obj.SetMinValue((int)CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_MID_VALUE:
                {
                    obj.SetMidValue((int)CAssistant::RawStringToUInt(to_lower_copy(value)));
                    break;
                }
                case DKI_PERCENTS:
                {
                    obj.SetPercents(CAssistant::RawStringToBool(to_lower_copy(value)));
                    break;
                }
                default:
                    break;
            }

            break;
        }
    }
}

void CDisplayManager::RecompilleData(const string &text, uint textColor)
{
    g_UpdateDisplayOnStep = false;
    m_Commands.clear();

    CompilleData(text, textColor);
}

void CDisplayManager::CompilleData(const string &text, uint textColor)
{
    CTextParser textParset(text, "", "", "{}", true);

    while (!textParset.IsEOF())
    {
        vector<string> list = textParset.ReadTokens(false);

        for (size_t li = 0; li < list.size(); li++)
        {
            if (list[li].length() == 0)
            {
                continue;
            }

            string str = list[li];

            if (str.at(0) == '{')
            {
                CTextParser tagParset(str, " \t{}=", "", "\"\"''", false);

                vector<string> tags = tagParset.ReadTokens();

                if (!tags.empty())
                {
                    string lowerName = to_lower_copy(*tags.begin());

                    if (lowerName == "group")
                    {
                        if (tags.size() > 1)
                        {
                            CompilleData(
                                g_AssistantConfig.GetGroupContent(to_lower_copy(*(tags.begin() + 1))),
                                textColor);
                        }

                        continue;
                    }

                    CDisplayItem tagData = GetDisplayItemByName(lowerName);

                    for (size_t t = 1; t < tags.size(); t++)
                    {
                        string tagName = tags[t];

                        ++t;

                        if (t != tags.size())
                        {
                            ApplyAttributeToDisplayItem(tagData, to_lower_copy(tagName), tags[t]);
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (tagData.GetType() >= DIT_X && tagData.GetType() <= DIT_Z)
                    {
                        g_UpdateDisplayOnStep = true;
                    }

                    m_Commands.push_back(tagData);
                }
            }
            else
            {
                m_Commands.push_back(CDisplayItem(
                    DIT_NONE, str, 0, 0, 0, 0, textColor, false, 0, 0, 0, 0, 0, false));
            }
        }
    }
}

} // namespace xuoassist
