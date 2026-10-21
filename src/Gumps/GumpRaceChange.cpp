// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpRaceChange.h"
#include "../GUI/GUIComboBox.h"
#include "../GUI/GUIComboboxText.h"
#include "../Managers/ColorManager.h"
#include "../Managers/CreateCharacterManager.h"
#include "../Network/Packets.h"
#include "../Renderer/RenderShader.h" // g_ColorizerShader
#include "../SelectedObject.h"        // g_SelectedObject
#include <xuocore/uodata.h>           // g_Data, uo_get_equipconv

CGumpRaceChange *g_GumpRaceChange = nullptr;

enum
{
    ID_GRC_NONE = 0,

    ID_GRC_HAIR_STYLE = 10,
    ID_GRC_BEARD_STYLE = 40,
    ID_GRC_CONFIRM,

    ID_GRC_SKIN_COLOR = 100,
    ID_GRC_HAIR_COLOR = 200,
    ID_GRC_BEARD_COLOR = 300,

    ID_GRC_COUNT,
};

//resolves the raw palette hue into the actual 16-bit color of the hues
//ramp entry - the same lookup the creation screen uses for its color bars
static uint32_t GetHueBarColor(uint16_t rawHue)
{
    uint8_t *huesData = (uint8_t *)g_ColorManager.GetHuesRangePointer() + 32 + 4;
    int colorOffsetDivider = sizeof(HUES_GROUP) - 4;
    int colorIndex = (rawHue + ((rawHue + (rawHue << 2)) << 1)) << 3;
    colorIndex += (colorIndex / colorOffsetDivider) << 2;
    return g_ColorManager.Color16To32(*(uint16_t *)(huesData + colorIndex));
}

//one color row of the right panel: a read-only caption that opens the
//picker, and a bar filled with the currently selected color. The caption
//sits 15px from the panel top/left and the bar 14px below it, the next row
//starts 45px lower - the exact layout of the creation screen's color rows
void CGumpRaceChange::AddColorRow(int x, int y, int id, const char *label, uint16_t selectedHue)
{
    CGUITextEntry *entry =
        (CGUITextEntry *)Add(new CGUITextEntry(id, 0x0481, 0x0021, 0x0021, x, y, 0, false, 9));
    entry->m_Entry.SetTextA(label);
    entry->CheckOnSerial = true;
    entry->ReadOnly = true;

    Add(new CGUIColoredPolygone(
        id, (uint16_t)(selectedHue - 1), x, y + 14, 120, 25, GetHueBarColor(selectedHue - 1)));
}

CGumpRaceChange::CGumpRaceChange(bool isFemale, uint8_t race)
    : CGump(GT_RACE_CHANGE, 0, 50, 50)
{
    g_GumpRaceChange = this;

    if (race < RT_HUMAN || race > RT_GARGOYLE)
    {
        //invalid race byte in the offer, nothing to display
        RemoveMark = true;
        m_Race = RT_HUMAN;
        m_Female = isFemale;
        return;
    }

    m_Race = (RACE_TYPE)race;
    m_Female = isFemale;

    //the dialog is a fixed layout, it must not be dragged around
    NoMove = true;

    //borrow the character creation tables for the gump lifetime
    CCreateCharacterManager &mgr = g_CreateCharacterManager;
    m_SavedRace = mgr.GetRace();
    m_SavedFemale = mgr.GetFemale();
    m_SavedHairStyle = mgr.HairStyle;
    m_SavedBeardStyle = mgr.BeardStyle;
    m_SavedSkinTone = mgr.SkinTone;
    m_SavedShirtColor = mgr.ShirtColor;
    m_SavedPantsColor = mgr.PantsColor;
    m_SavedHairColor = mgr.HairColor;
    m_SavedBeardColor = mgr.BeardColor;

    mgr.SetRace(m_Race);
    mgr.SetFemale(m_Female);
    m_Borrowed = true;

    //palette-based defaults, first entry of each table; stored hues use the
    //creation manager convention (raw table value + 1)
    m_SkinTone = (uint16_t)(*mgr.GetSkinTonePtr() + 1);
    m_HairColor = (uint16_t)(*mgr.GetHairColorPtr() + 1);
    m_BeardColor = m_HairColor;

    m_HairStyle = 1;
    if (m_HairStyle >= mgr.GetCurrentHairCount())
    {
        m_HairStyle = 0;
    }
    m_BeardStyle = 0;
}

CGumpRaceChange::~CGumpRaceChange()
{
    if (m_Borrowed)
    {
        RestoreCharacterCreationState();
    }
    g_GumpRaceChange = nullptr;
}

void CGumpRaceChange::RestoreCharacterCreationState()
{
    CCreateCharacterManager &mgr = g_CreateCharacterManager;
    mgr.SetRace(m_SavedRace);
    mgr.SetFemale(m_SavedFemale);
    mgr.HairStyle = m_SavedHairStyle;
    mgr.BeardStyle = m_SavedBeardStyle;
    mgr.SkinTone = m_SavedSkinTone;
    mgr.ShirtColor = m_SavedShirtColor;
    mgr.PantsColor = m_SavedPantsColor;
    mgr.HairColor = m_SavedHairColor;
    mgr.BeardColor = m_SavedBeardColor;
}

//inline palette picker for one color row, drawn over the right panel. The
//grid geometry (cell size, row/column count per race) and the dot marker on
//each swatch match the creation screen's color picker exactly; only the root
//position is shifted to this dialog's panel origin (419, 45)
void CGumpRaceChange::AddColorPicker(int idBase, const uint16_t *palette, int gridYOffset)
{
    int maxX = 8;
    int polygoneWidth = 15;
    int maxY = 8;
    int polygoneHeight = 35;

    if (m_Race == RT_ELF)
    {
        maxX = 4;
        polygoneWidth = 30;
    }
    else if (m_Race == RT_GARGOYLE)
    {
        maxX = 4;
        polygoneWidth = 30;
        maxY = 7;
        polygoneHeight = 40;
    }

    if (gridYOffset >= 0)
    { //hair palettes: fixed 6 columns, row count/height depends on the race
        maxX = 6;
        polygoneWidth = 20;
        polygoneHeight = 35;
        maxY = 8;
        if (m_Race == RT_ELF)
        {
            maxY = 9;
            polygoneHeight = 31;
        }
        else if (m_Race == RT_GARGOYLE)
        {
            maxY = 3;
            polygoneHeight = 93;
        }
    }

    for (int y = 0; y < maxY; y++)
    {
        for (int x = 0; x < maxX; x++)
        {
            const uint16_t rawHue = *palette++;
            CGUIColoredPolygone *polygone = (CGUIColoredPolygone *)Add(new CGUIColoredPolygone(
                idBase + ((int)x * maxY + (int)y),
                rawHue,
                435 + ((int)x * polygoneWidth),
                58 + gridYOffset + ((int)y * polygoneHeight),
                polygoneWidth,
                polygoneHeight,
                GetHueBarColor(rawHue)));
            polygone->DrawDot = true;
        }
    }
}

//semi-naked body artwork shown inside the paperdoll frame - the same
//per-race/gender creation gumps the creation screen draws (and the ones the
//hair/facial hair overlay gumps are anchored to), index 1 = female
uint16_t CGumpRaceChange::GetPreviewBodyGump() const
{
    //CCreateCharacterManager::GetBodyGump()
    static const uint16_t gump[3][2] = { { 0x0761, 0x0760 },   //human
                                         { 0x0766, 0x0765 },   //elf
                                         { 0x076B, 0x076A } }; //gargoyle
    return gump[m_Race - 1][m_Female];
}

void CGumpRaceChange::UpdateContent()
{
    Clear();

    CCreateCharacterManager &mgr = g_CreateCharacterManager;

    //main background and side panels
    Add(new CGUIResizepic(0, 0x0E10, 0, 0, 595, 400));
    Add(new CGUIResizepic(0, 0x0E10, 25, 45, 151, 310));
    Add(new CGUIResizepic(0, 0x0E10, 419, 45, 151, 310));

    //paperdoll frame, then the character preview layers, then the title on top
    Add(new CGUIGumppic(0x0708, 185, 25));

    //race caption (display only, the race is chosen by the server offer)
    uint16_t raceText = 0;
    int raceTextWidth = 0;
    switch (m_Race)
    {
        case RT_ELF:
        {
            raceText = 0x0705;
            raceTextWidth = 79;
            break;
        }
        case RT_GARGOYLE:
        {
            raceText = 0x07D4;
            raceTextWidth = 99;
            break;
        }
        default:
        {
            raceText = 0x0702;
            raceTextWidth = 79;
            break;
        }
    }
    Add(new CGUIGumppic(raceText, 176 - raceTextWidth, 360));

    //gender caption (display only, matches the offer)
    Add(new CGUIGumppic(m_Female ? 0x070D : 0x0710, 419, 360));

    //confirm button
    Add(new CGUIButton(ID_GRC_CONFIRM, 0x15A4, 0x15A6, 0x15A5, 560, 360));

    //paperdoll preview: the body artwork is placed inside the paperdoll frame
    //at a fixed offset; as on the creation screen, the hair and facial hair
    //overlay gumps share the body's origin (they are anchored for these
    //creation body gumps, so no extra offset is applied)
    Add(new CGUIShader(&g_ColorizerShader, true));

    //the frame and every preview layer share one origin: the arch art in
    //0x0708 carries the placement offset (same scheme as the creation screen)
    const int previewX = 185;
    const int previewY = 25;

    const uint16_t bodyGump = GetPreviewBodyGump();
    if (bodyGump != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppic(bodyGump, previewX, previewY));
        gumppic->Color = m_SkinTone;
    }

    if (m_HairStyle != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(
            new CGUIGumppic(mgr.GetHair((uint8_t)m_HairStyle).GumpID, previewX, previewY));
        gumppic->Color = m_HairColor;
    }

    const bool hasBeard = (!m_Female && m_Race != RT_ELF);
    if (hasBeard && m_BeardStyle != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(
            new CGUIGumppic(mgr.GetBeard((uint8_t)m_BeardStyle).GumpID, previewX, previewY));
        gumppic->Color = m_BeardColor;
    }

    Add(new CGUIShader(nullptr, false));

    //"Character Race Changer" title, drawn above the paperdoll frame
    Add(new CGUIGumppic(0x0769, 211, 15));

    //hair style picker: caption above, combobox below (caption hue as on the
    //creation screen - the dark list hue is unreadable on the stone panel)
    CGUIText *text = (CGUIText *)Add(new CGUIText(0x0481, 41, 60));
    text->CreateTextureA(9, "Hair Style");

    const int hairCount = mgr.GetCurrentHairCount();
    CGUIComboBox *combo = (CGUIComboBox *)Add(
        new CGUIComboBox(ID_GRC_HAIR_STYLE, 0x0BB8, false, 0x0BB8, 40, 75, 120, hairCount, false));
    combo->SelectedIndex = m_HairStyle;
    for (int i = 0; i < hairCount; i++)
    {
        combo->Add(new CGUIComboboxText(0x0381, 9, mgr.GetHair((uint8_t)i).Name));
    }
    combo->RecalculateWidth();

    //facial hair picker (males, non-elves only)
    if (hasBeard)
    {
        text = (CGUIText *)Add(new CGUIText(0x0481, 41, 105));
        text->CreateTextureA(9, "Facial Hair Style");

        const int beardCount = mgr.GetCurrentFacialHairCount();
        combo = (CGUIComboBox *)Add(new CGUIComboBox(
            ID_GRC_BEARD_STYLE, 0x0BB8, false, 0x0BB8, 40, 120, 120, beardCount, false));
        combo->SelectedIndex = m_BeardStyle;
        for (int i = 0; i < beardCount; i++)
        {
            combo->Add(new CGUIComboboxText(0x0381, 9, mgr.GetBeard((uint8_t)i).Name));
        }
        combo->RecalculateWidth();
    }

    //color rows: caption at y, bar 14px below, 45px row pitch (fixed slots,
    //matching the creation screen's panel layout)
    if (m_ColorSelection == 0)
    {
        AddColorRow(434, 60, ID_GRC_SKIN_COLOR, "Skin Tone", m_SkinTone);
        AddColorRow(434, 105, ID_GRC_HAIR_COLOR, "Hair Color", m_HairColor);
        if (hasBeard)
        {
            AddColorRow(434, 150, ID_GRC_BEARD_COLOR, "Facial Hair Color", m_BeardColor);
        }
    }
    else if (m_ColorSelection == ID_GRC_SKIN_COLOR)
    {
        AddColorPicker(ID_GRC_SKIN_COLOR, mgr.GetSkinTonePtr(), 0);
    }
    else
    {
        const uint16_t *palette = mgr.GetHairColorPtr();
        AddColorPicker(
            m_ColorSelection == ID_GRC_HAIR_COLOR ? ID_GRC_HAIR_COLOR : ID_GRC_BEARD_COLOR,
            palette,
            2);
    }
}

void CGumpRaceChange::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GRC_CONFIRM)
    {
        CCreateCharacterManager &mgr = g_CreateCharacterManager;
        const bool hasBeard = (!m_Female && m_Race != RT_ELF);

        CPacketRaceChange(
            m_SkinTone,
            mgr.GetHair(m_HairStyle).GraphicID,
            m_HairColor,
            (hasBeard && m_BeardStyle != 0u) ? mgr.GetBeard(m_BeardStyle).GraphicID : 0,
            (hasBeard && m_BeardStyle != 0u) ? m_BeardColor : 0)
            .Send();

        RemoveMark = true;
        m_ColorSelection = 0;
    }
}

//the color rows are read-only text entries: clicking a caption opens that
//row's palette, clicking a swatch stores the picked hue (raw table value
//+ 1, the convention of the creation manager) and closes the picker
void CGumpRaceChange::GUMP_TEXT_ENTRY_EVENT_C
{
    if (m_ColorSelection == 0)
    {
        if (serial == ID_GRC_SKIN_COLOR)
        {
            m_ColorSelection = ID_GRC_SKIN_COLOR;
        }
        else if (serial == ID_GRC_HAIR_COLOR)
        {
            m_ColorSelection = ID_GRC_HAIR_COLOR;
        }
        else if (!m_Female && m_Race != RT_ELF && serial == ID_GRC_BEARD_COLOR)
        {
            m_ColorSelection = ID_GRC_BEARD_COLOR;
        }
    }
    else if (serial >= ID_GRC_SKIN_COLOR && serial < ID_GRC_BEARD_COLOR + 8 * 9)
    {
        const uint16_t color = (uint16_t)(g_SelectedObject.Object->Color + 1);

        if (m_ColorSelection == ID_GRC_SKIN_COLOR)
        {
            m_SkinTone = color;
        }
        else if (m_ColorSelection == ID_GRC_HAIR_COLOR)
        {
            m_HairColor = color;
        }
        else if (m_ColorSelection == ID_GRC_BEARD_COLOR)
        {
            m_BeardColor = color;
        }

        m_ColorSelection = 0;
    }

    WantUpdateContent = true;
}

void CGumpRaceChange::GUMP_COMBOBOX_SELECTION_EVENT_C
{
    if (serial >= ID_GRC_HAIR_STYLE && serial < ID_GRC_BEARD_STYLE)
    {
        const uint8_t index = (uint8_t)(serial - ID_GRC_HAIR_STYLE);
        if (index < g_CreateCharacterManager.GetCurrentHairCount())
        {
            m_HairStyle = index;
        }
        m_ColorSelection = 0;
        WantUpdateContent = true;
    }
    else if (serial >= ID_GRC_BEARD_STYLE && serial < ID_GRC_SKIN_COLOR)
    {
        const uint8_t index = (uint8_t)(serial - ID_GRC_BEARD_STYLE);
        if (index < g_CreateCharacterManager.GetCurrentFacialHairCount())
        {
            m_BeardStyle = index;
        }
        m_ColorSelection = 0;
        WantUpdateContent = true;
    }
}
