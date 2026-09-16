// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// the info-command table, the 26 default items, the display-key parser and
// the title-data compiler are here; the win32 GDI Redraw/icon bodies are
// Phase 2/4 (see displaymanager.h).

#include "displaymanager.h"
#include "ServerList.h" // g_ServerList.LastServerName (the fused server name)
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../config/assistantconfig.h"
#include "../core/textconv.h"
#include "../core/world.h" // g_Player / g_World
#include "../core/worldutil.h"
#include <common/logging/logging.h>
#include <map>
#include <stdlib.h>

#include <xuocore/uodata.h>        // g_Index / MAX_STATIC_DATA_INDEX_COUNT
#include "CrossUO.h"               // ::g_Game (tiledata flags)
#include "Managers/ColorManager.h" // ::g_ColorManager (hue)
#include "Managers/UOFileReader.h" // ::g_UOFileReader (static art decode)

namespace xuoassist {

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
    // The original icon cache: keyed by (graphic << 16) |
    // hue, fed by LoadDisplayIcon from the assistant filemanager's raw
    // static-art access. In-process the pixels come from the client's own
    // art reader (g_UOFileReader.GetArtPixels) and the client hue manager.
    if (item.Graphic == 0u)
    {
        return nullptr;
    }

    const uint32_t serial = (uint32_t)((item.Graphic << 16) | item.Hue);
    auto i = m_Icons.find(serial);
    if (i == m_Icons.end() || i->second == nullptr)
    {
        CDisplayIcon *icon = LoadDisplayIcon(item.Graphic, item.Hue);
        m_Icons[serial] = icon;
        return icon;
    }

    return i->second;
}

CDisplayIcon *CDisplayManager::LoadDisplayIcon(uint16_t graphic, uint16_t color)
{
    // The original LoadDisplayIcon verbatim, with the raw
    // art decode delegated to the client's g_UOFileReader.GetArtPixels (the
    // original code walked the file bytes itself through the plugin file access).
    // Pixels are IM_COL32-layout RGBA (the client hue manager emits
    // 0x00BBGGRR; nonzero pixels get full alpha for the ImGui upload).
    if (graphic == 0x0EED || graphic == 0x0EEE)
    {
        graphic = 0x0EEF; // Fix gold display
    }

    if ((size_t)graphic >= MAX_STATIC_DATA_INDEX_COUNT)
    {
        return new CDisplayIcon();
    }

    short dataWidth = 0;
    short dataHeight = 0;
    vector<uint16_t> art =
        g_UOFileReader.GetArtPixels(graphic, g_Index.m_Static[graphic], true, dataWidth, dataHeight);
    if (art.empty() || dataWidth <= 0 || dataHeight <= 0)
    {
        return new CDisplayIcon();
    }

    const uint64_t tileFlags = g_Game.GetStaticFlags(graphic);
    const bool partialHue = (tileFlags & 0x00040000) != 0;
    const int stackOffset =
        (((tileFlags & 0x00000800) != 0u) && graphic != 0x0EEF && g_AssistantConfig.DoubleImage())
            ? 5
            : 0;

    const int imageWidth = dataWidth + stackOffset;
    const int imageHeight = dataHeight + stackOffset;
    vector<uint32_t> pix((size_t)imageWidth * imageHeight, 0);

    for (int y = 0; y < dataHeight; y++)
    {
        for (int x = 0; x < dataWidth; x++)
        {
            const uint16_t raw = art[(y * dataWidth) + x];
            if (raw == 0u)
            {
                continue; // transparent (the original run decode skipped 0 pixels)
            }

            uint32_t col = 0;
            uint16_t c = raw;
            if (color != 0u)
            {
                col = (partialHue ? ::g_ColorManager.GetPartialHueColor(c, color)
                                  : ::g_ColorManager.GetColor(c, color));
            }
            else
            {
                col = ::g_ColorManager.Color16To32(c);
            }

            pix[(y * imageWidth) + x] = col;
        }
    }

    // the stacked (+5 down-right) copy of stackable items (original verbatim)
    if (stackOffset != 0)
    {
        for (int j = dataHeight - 1; j >= 0; j--)
        {
            const int srcPosY = j * imageWidth;
            const int dstPosY = (j + stackOffset) * imageWidth;
            for (int i = dataWidth - 1; i >= 0; i--)
            {
                const uint32_t pixel = pix[srcPosY + i];
                if (pixel != 0u)
                {
                    pix[dstPosY + i + stackOffset] = pixel;
                }
            }
        }
    }

    // bounding box over the non-transparent pixels
    int minX = 999;
    int minY = 999;
    int maxX = 0;
    int maxY = 0;
    for (int y = 0; y < imageHeight; y++)
    {
        for (int x = 0; x < imageWidth; x++)
        {
            if (pix[(y * imageWidth) + x] != 0u)
            {
                if (x < minX)
                    minX = x;
                if (x > maxX)
                    maxX = x;
                if (y < minY)
                    minY = y;
                if (y > maxY)
                    maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY)
    {
        return new CDisplayIcon(); // fully transparent decode
    }

    int realWidth = (maxX - minX) + stackOffset;
    if (maxX < imageWidth)
    {
        realWidth++;
    }

    int realHeight = (maxY - minY) + stackOffset;
    if (maxY < imageHeight)
    {
        realHeight++;
    }

    CDisplayIcon *icon = new CDisplayIcon();
    icon->Width = realWidth;
    icon->Height = realHeight;
    icon->Pixels.assign((size_t)realWidth * realHeight, 0);

    for (int y = 0; y < realHeight; y++)
    {
        const size_t srcY = (size_t)(y + minY) * imageWidth;
        const size_t dstY = (size_t)y * realWidth;

        for (int x = 0; x < realWidth; x++)
        {
            const uint32_t col = pix[srcY + x + minX];
            // 0x00BBGGRR -> opaque RGBA32 (0 stays fully transparent)
            icon->Pixels[dstY + x] = col != 0u ? (col | 0xFF000000u) : 0u;
        }
    }

    return icon;
}

void CDisplayManager::GetValues(
    const CDisplayItem &item,
    astr_t &value,
    astr_t &maxValue,
    uint32_t textColor,
    uint32_t backgroundColor,
    bool useNotoriety)
{
    int iVal = 0;
    int iMaxVal = 0;
    bool inPercents = false;

    switch (item.Type)
    {
        case DIT_NAME:
        {
            value = astr_t(g_SelectedCharName);
            backgroundColor = 0;

            if (useNotoriety)
            {
                if (g_Player->Notoriety == 3 || g_Player->Notoriety == 4)
                {
                    textColor = item.BackgroundMidColor; //gray
                }
                else if (g_Player->Notoriety == 5 || g_Player->Notoriety == 6)
                {
                    textColor = item.BackgroundMinColor; //red
                }
                else
                {
                    textColor = item.BackgroundColor; //blue
                }
            }
            else
            {
                textColor = item.Color;
            }

            return;
        }
        case DIT_SERVER:
        {
            value = g_ServerList.LastServerName;
            textColor = item.Color;
            backgroundColor = item.BackgroundColor;
            return;
        }
        case DIT_HITS:
        case DIT_MAX_HITS:
        case DIT_HITS_MAX_HITS:
        {
            iVal = g_Player->Hits;
            iMaxVal = g_Player->MaxHits;
            inPercents = (item.Percents && item.Type != DIT_MAX_HITS);
            break;
        }
        case DIT_MANA:
        case DIT_MAX_MANA:
        case DIT_MANA_MAX_MANA:
        {
            iVal = g_Player->Mana;
            iMaxVal = g_Player->MaxMana;
            inPercents = (item.Percents && item.Type != DIT_MAX_MANA);
            break;
        }
        case DIT_STAM:
        case DIT_MAX_STAM:
        case DIT_STAM_MAX_STAM:
        {
            iVal = g_Player->Stam;
            iMaxVal = g_Player->MaxStam;
            inPercents = (item.Percents && item.Type != DIT_MAX_STAM);
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
            inPercents = (item.Percents && item.Type != DIT_MAX_WEIGHT);
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
            textColor = item.Color;
            backgroundColor = item.BackgroundColor;
            return;
        }
        case DIT_Y:
        {
            value = str_from(g_Player->GetY());
            textColor = item.Color;
            backgroundColor = item.BackgroundColor;
            return;
        }
        case DIT_Z:
        {
            value = str_from(g_Player->GetZ());
            textColor = item.Color;
            backgroundColor = item.BackgroundColor;
            return;
        }
        case DIT_OBJECT:
        {
            iVal = CountItems(g_Player, item.Graphic, item.Hue, true);
            break;
        }
        case DIT_NONE:
        {
            textColor = item.Color;
            break;
        }
        default:
            return;
    }

    value = str_from(iVal);
    maxValue = str_from(iMaxVal);

    if (inPercents && (iMaxVal != 0))
    {
        iVal = int((iVal * 100) / float(iMaxVal));
    }

    if (iVal <= item.MinValue)
    {
        textColor = item.MinColor;
        backgroundColor = item.BackgroundMinColor;
    }
    else if (iVal <= item.MidValue)
    {
        textColor = item.MidColor;
        backgroundColor = item.BackgroundMidColor;
    }
    else
    {
        textColor = item.Color;
        backgroundColor = item.BackgroundColor;
    }

    // The colors land in the drawn text/bg (Phase 2/4); the value logic above
    // is the part this round keeps.
    (void)textColor;
    (void)backgroundColor;
}

bool CDisplayManager::CanRedraw(const uint16_t &graphic, const uint16_t &color) const
{
    for (const CDisplayItem &obj : m_Commands)
    {
        if (obj.Type == DIT_OBJECT && obj.Graphic == graphic &&
            (obj.Hue == 0xFFFF || obj.Hue == color))
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
    uint32_t backgroundColor)
{
    // Was the win32 GDI title-bar painter (displaymanager.cpp:500-...). The
    // caption text is composed here (the original RDM_TEXTUAL branch, verbatim);
    // the colored/graphical painter has no OS-caption equivalent - the two
    // colored modes produce the same plain string. The caller (the assistant
    // UI tick) applies the text to the client window on change.
    (void)hightFont;
    (void)highlightReagents;
    (void)displayAppIcon;
    (void)backgroundColor;

    astr_t title;

    if (mode == RDM_DISABLED || g_World == nullptr || g_Player == nullptr)
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
        for (const CDisplayItem &obj : m_Commands)
        {
            astr_t value = "";
            astr_t maxValue = "";
            uint32_t textColor = 0;
            uint32_t compiledBgColor = 0;

            GetValues(obj, value, maxValue, textColor, compiledBgColor, useNotoriety);

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

CDisplayItem CDisplayManager::GetDisplayItemByName(const astr_t &name) const
{
    CDisplayItem item = g_AssistantConfig.GetCustomDisplayItemByName(name);

    if (item.Type == DIT_NONE)
    {
        for (const CDisplayInfo &info : m_InfoCommands)
        {
            if (name == info.Command)
            {
                if (info.Type != DIT_OBJECT)
                {
                    return m_DefaultItems[info.Type];
                }
                //return CDisplayItem(m_DefaultItems[m_InfoCommands[i].Type]);

                break;
            }
        }
    }

    return item;
}

void CDisplayManager::ApplyAttributeToDisplayItem(
    CDisplayItem &obj, const astr_t &name, const astr_t &value)
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
                case DKI_COLOR_MIN:
                {
                    obj.MinColor = CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                case DKI_COLOR_MID:
                {
                    obj.MidColor = CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                case DKI_COLOR:
                {
                    obj.Color = CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                case DKI_BACKGROUND:
                {
                    obj.Background = CAssistant::RawStringToBool(str_lower(value));
                    break;
                }
                case DKI_BG_COLOR_MIN:
                {
                    obj.BackgroundMinColor = CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                case DKI_BG_COLOR_MID:
                {
                    obj.BackgroundMidColor = CAssistant::RawStringToUInt(str_lower(value));
                    break;
                }
                case DKI_BG_COLOR:
                {
                    obj.BackgroundColor = CAssistant::RawStringToUInt(str_lower(value));
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
                case DKI_PERCENTS:
                {
                    obj.Percents = CAssistant::RawStringToBool(str_lower(value));
                    break;
                }
                default:
                    break;
            }

            break;
        }
    }
}

void CDisplayManager::RecompilleData(const astr_t &text, uint32_t textColor)
{
    g_UpdateDisplayOnStep = false;
    m_Commands.clear();

    CompilleData(text, textColor);
}

void CDisplayManager::CompilleData(const astr_t &text, uint32_t textColor)
{
    // Tokenized line by line, one token list per line, like the original
    // X:UO Assist text parser; the "{}" quote pair keeps the braces on tag
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
                            CompilleData(
                                g_AssistantConfig.GetGroupContent(str_lower(*(tags.begin() + 1))),
                                textColor);
                        }

                        continue;
                    }

                    CDisplayItem tagData = GetDisplayItemByName(lowerName);

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
                m_Commands.push_back(CDisplayItem(
                    DIT_NONE, str, 0, 0, 0, 0, textColor, false, 0, 0, 0, 0, 0, false));
            }
        }
    }
}

} // namespace xuoassist
