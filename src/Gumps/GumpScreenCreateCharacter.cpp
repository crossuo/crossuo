// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenCreateCharacter.h"
#include "Gump1998ScrollChoice.h"
#include "../GUI/GUIShader.h"
#include "../GUI/GUIButton.h"
#include "../Renderer/RenderShader.h" // g_ColorizerShader and friends
#include "../Config.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../GameWindow.h"
#include "../Managers/CreateCharacterManager.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ColorManager.h"
#include "../Managers/SkillsManager.h"
#include "../Managers/ProfessionManager.h"
#include "../Profession.h"
#include "../ScreenStages/CreateCharacterScreen.h"
#include <common/str.h>

enum
{
    ID_CCS_NONE,

    ID_CCS_QUIT,
    ID_CCS_ARROW_PREV,
    ID_CCS_ARROW_NEXT,
    ID_CCS_NAME_FIELD,
    ID_CCS_MALE_BUTTON,
    ID_CCS_FEMALE_BUTTON,
    ID_CCS_HUMAN_RACE_BUTTON,
    ID_CCS_ELF_RACE_BUTTON,
    ID_CCS_GARGOYLE_RACE_BUTTON,

    ID_CCS_SKIN_TONE,
    ID_CCS_SHIRT_COLOR,
    ID_CCS_SKIRT_OR_PANTS_COLOR,
    ID_CCS_HAIR_COLOR,
    ID_CCS_FACIAL_HAIR_COLOR,

    ID_CCS_COUNT,

    ID_CCS_HAIR_STYLE = 20,
    ID_CCS_FACIAL_HAIR_STYLE = 40,
    ID_CCS_COLOR_RANGE = 100,

    //1.25.32 (1998) CharCreation controls: disjoint id blocks per role
    // (nav / hue sliders / popup ids) - a row or arrow id must never
    //straddle another role, and the row branch is bounded by the row count
    ID_CCS_1998_BACK = 1600,
    ID_CCS_1998_NEXT,
    ID_CCS_1998_HUE_SLIDER,       //+0..+5: the six hue sliders
    ID_CCS_1998_SEX,
    ID_CCS_1998_HAIR,
    ID_CCS_1998_BEARD,
    ID_CCS_1998_HAIR_POPUP_UP = 1615,
    ID_CCS_1998_HAIR_POPUP_DOWN,
    ID_CCS_1998_HAIR_POPUP_THUMB,
    ID_CCS_1998_BEARD_POPUP_UP = 1618,
    ID_CCS_1998_BEARD_POPUP_DOWN,
    ID_CCS_1998_BEARD_POPUP_THUMB,
    ID_CCS_1998_HAIR_POPUP_ROW = 1620,  //+0..+10
    ID_CCS_1998_BEARD_POPUP_ROW = 1720, //+0..+7
};

CGumpScreenCreateCharacter::CGumpScreenCreateCharacter()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;

    for (int i = 0; i < 6; i++)
    {
        m_1998HueSliders[i] = nullptr;
    }
    m_1998StyleName[0] = nullptr;
    m_1998StyleName[1] = nullptr;
    m_1998PreviewBody = nullptr;
    m_1998PreviewHair = nullptr;
    m_1998PreviewBeard = nullptr;
}

CGumpScreenCreateCharacter::~CGumpScreenCreateCharacter()
{
}

void CGumpScreenCreateCharacter::UpdateContent()
{
    Clear();

    if (g_Config.ClientVersion < CV_200)
    {
        UpdateContent1998();
        return;
    }

    UpdateContentModern();
}

//the layout every >= 2.0 client expects
void CGumpScreenCreateCharacter::UpdateContentModern()
{
    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));
    Add(new CGUIGumppic(0x157C, 0, 0));
    Add(new CGUIGumppic(0x15A0, 0, 4));
    Add(new CGUIButton(ID_CCS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));
    Add(new CGUIButton(ID_CCS_ARROW_PREV, 0x15A1, 0x15A2, 0x15A3, 586, 445));
    Add(new CGUIButton(ID_CCS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));
    Add(new CGUIResizepic(0, 0x0E10, 82, 125, 151, 310));
    Add(new CGUIGumppic(0x0709, 280, 53));
    Add(new CGUIGumppic(0x070A, 240, 73));
    Add(new CGUIGumppicTiled(0x070B, 248, 73, 215, 0));
    Add(new CGUIGumppic(0x070C, 463, 73));

    CGUITextEntry *entry = (CGUITextEntry *)Add(
        new CGUITextEntry(ID_CCS_NAME_FIELD, 1, 1, 1, 257, 65, 300, false, 5, TS_LEFT, 0, 32));
    entry->CheckOnSerial = true;
    //the name field owns keyboard focus until a screen without entries takes it
    g_EntryPointer = &entry->m_Entry;
    g_EntryPointer->SetTextA(g_CreateCharacterScreen.Name);

    Add(new CGUIGumppic(0x0708, 238, 98));
    Add(new CGUIResizepic(0, 0x0E10, 475, 125, 151, 310));

    const uint16_t toneTextColorRange[2] = { 0x0481, 0x0021 };

    int facialHairCount = g_CreateCharacterManager.GetCurrentFacialHairCount();

    CGUIComboBox *combo = (CGUIComboBox *)Add(new CGUIComboBox(
        ID_CCS_FACIAL_HAIR_STYLE, 0x0BB8, false, 0x0BB8, 97, 199, 120, facialHairCount, false));
    combo->SelectedIndex = g_CreateCharacterManager.BeardStyle;
    combo->Visible =
        (!g_CreateCharacterManager.GetFemale() && g_CreateCharacterManager.GetRace() != RT_ELF);

    CGUIText *text = new CGUIText(0x0481, 3, -12);
    text->CreateTextureA(9, "Facial Hair Style");
    combo->SetText(text);

    for (int i = 0; i < facialHairCount; i++)
    {
        combo->Add(
            new CGUIComboboxText(0x0381, 9, g_CreateCharacterManager.GetBeard((uint8_t)i).Name));
    }

    combo->RecalculateWidth();

    int hairCount = g_CreateCharacterManager.GetCurrentHairCount();

    combo = (CGUIComboBox *)Add(
        new CGUIComboBox(ID_CCS_HAIR_STYLE, 0x0BB8, false, 0x0BB8, 97, 154, 120, hairCount, false));
    combo->SelectedIndex = g_CreateCharacterManager.HairStyle;

    text = new CGUIText(0x0481, 3, -12);
    text->CreateTextureA(9, "Hair Style");
    combo->SetText(text);

    for (int i = 0; i < hairCount; i++)
    {
        combo->Add(
            new CGUIComboboxText(0x0381, 9, g_CreateCharacterManager.GetHair((uint8_t)i).Name));
    }

    combo->RecalculateWidth();

    uint8_t *huesData = (uint8_t *)g_ColorManager.GetHuesRangePointer() + 32 + 4;
    int colorOffsetDivider = sizeof(HUES_GROUP) - 4;

    if (g_CreateCharacterScreen.GetColorSelection() == 0)
    {
        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_CCS_SKIN_TONE,
            toneTextColorRange[0],
            toneTextColorRange[1],
            toneTextColorRange[1],
            490,
            140,
            0,
            false,
            9));
        entry->m_Entry.SetTextA("Skin Tone");
        entry->CheckOnSerial = true;
        entry->ReadOnly = true;

        uint16_t color = g_CreateCharacterManager.SkinTone - 1;

        int colorIndex = (color + ((color + (color << 2)) << 1)) << 3;
        colorIndex += (colorIndex / colorOffsetDivider) << 2;
        color = *(uint16_t *)(huesData + colorIndex);

        uint32_t clr = g_ColorManager.Color16To32(color);

        Add(new CGUIColoredPolygone(ID_CCS_SKIN_TONE, color, 490, 154, 120, 25, clr));

        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_CCS_SHIRT_COLOR,
            toneTextColorRange[0],
            toneTextColorRange[1],
            toneTextColorRange[1],
            490,
            185,
            0,
            false,
            9));

        if (g_CreateCharacterManager.GetRace() == RT_GARGOYLE)
        {
            entry->m_Entry.SetTextA("Robe Color");
        }
        else
        {
            entry->m_Entry.SetTextA("Shirt Color");
        }

        entry->CheckOnSerial = true;
        entry->ReadOnly = true;

        color = g_CreateCharacterManager.ShirtColor - 1;

        colorIndex = (color + ((color + (color << 2)) << 1)) << 3;
        colorIndex += (colorIndex / colorOffsetDivider) << 2;
        color = *(uint16_t *)(huesData + colorIndex);

        clr = g_ColorManager.Color16To32(color);

        Add(new CGUIColoredPolygone(ID_CCS_SHIRT_COLOR, color, 490, 199, 120, 25, clr));

        if (g_CreateCharacterManager.GetRace() != RT_GARGOYLE)
        {
            entry = (CGUITextEntry *)Add(new CGUITextEntry(
                ID_CCS_SKIRT_OR_PANTS_COLOR,
                toneTextColorRange[0],
                toneTextColorRange[1],
                toneTextColorRange[1],
                490,
                230,
                0,
                false,
                9));
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;

            if (g_CreateCharacterManager.GetFemale())
            {
                entry->m_Entry.SetTextA("Skirt Color");
            }
            else
            {
                entry->m_Entry.SetTextA("Pants Color");
            }

            color = g_CreateCharacterManager.PantsColor - 1;

            colorIndex = (color + ((color + (color << 2)) << 1)) << 3;
            colorIndex += (colorIndex / colorOffsetDivider) << 2;
            color = *(uint16_t *)(huesData + colorIndex);

            clr = g_ColorManager.Color16To32(color);

            Add(new CGUIColoredPolygone(
                ID_CCS_SKIRT_OR_PANTS_COLOR, color, 490, 244, 120, 25, clr));
        }

        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_CCS_HAIR_COLOR,
            toneTextColorRange[0],
            toneTextColorRange[1],
            toneTextColorRange[1],
            490,
            275,
            0,
            false,
            9));
        entry->m_Entry.SetTextA("Hair Color");
        entry->CheckOnSerial = true;
        entry->ReadOnly = true;

        color = g_CreateCharacterManager.HairColor - 1;

        colorIndex = (color + ((color + (color << 2)) << 1)) << 3;
        colorIndex += (colorIndex / colorOffsetDivider) << 2;
        color = *(uint16_t *)(huesData + colorIndex);

        clr = g_ColorManager.Color16To32(color);

        Add(new CGUIColoredPolygone(ID_CCS_HAIR_COLOR, color, 490, 289, 120, 25, clr));

        if (!g_CreateCharacterManager.GetFemale() && g_CreateCharacterManager.GetRace() != RT_ELF)
        {
            entry = (CGUITextEntry *)Add(new CGUITextEntry(
                ID_CCS_FACIAL_HAIR_COLOR,
                toneTextColorRange[0],
                toneTextColorRange[1],
                toneTextColorRange[1],
                490,
                320,
                0,
                false,
                9));
            entry->m_Entry.SetTextA("Facial Hair Color");
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;

            color = g_CreateCharacterManager.BeardColor - 1;

            colorIndex = (color + ((color + (color << 2)) << 1)) << 3;
            colorIndex += (colorIndex / colorOffsetDivider) << 2;
            color = *(uint16_t *)(huesData + colorIndex);

            clr = g_ColorManager.Color16To32(color);

            Add(new CGUIColoredPolygone(ID_CCS_FACIAL_HAIR_COLOR, color, 490, 333, 120, 25, clr));
        }
    }
    else
    {
        if (g_CreateCharacterScreen.GetColorSelection() == CCSID_SKIN_TONE)
        {
            const uint16_t *colorPtr = g_CreateCharacterManager.GetSkinTonePtr();
            int maxX = 8;
            int polygoneWidth = 15;
            int maxY = 8;
            int polygoneHeight = 35;

            if (g_CreateCharacterManager.GetRace() == RT_ELF)
            {
                maxX = 4;
                polygoneWidth = 30;
            }
            else if (g_CreateCharacterManager.GetRace() == RT_GARGOYLE)
            {
                maxX = 4;
                polygoneWidth = 30;
                maxY = 7;
                polygoneHeight = 40;
            }

            for (int y = 0; y < maxY; y++)
            {
                for (int x = 0; x < maxX; x++)
                {
                    uint16_t startColor = *colorPtr++;

                    int colorIndex = (startColor + ((startColor + (startColor << 2)) << 1)) << 3;
                    colorIndex += (colorIndex / colorOffsetDivider) << 2;
                    uint16_t color = *(uint16_t *)(huesData + colorIndex);

                    uint32_t clr = g_ColorManager.Color16To32(color);

                    CGUIColoredPolygone *polygone =
                        (CGUIColoredPolygone *)Add(new CGUIColoredPolygone(
                            ID_CCS_COLOR_RANGE + ((int)x * maxY + (int)y),
                            startColor,
                            491 + ((int)x * polygoneWidth),
                            138 + ((int)y * polygoneHeight),
                            polygoneWidth,
                            polygoneHeight,
                            clr));
                    polygone->DrawDot = true;
                }
            }
        }
        else if (
            g_CreateCharacterScreen.GetColorSelection() == CCSID_SHIRT_COLOR ||
            g_CreateCharacterScreen.GetColorSelection() == CCSID_SKIRT_OR_PANTS_COLOR)
        {
            for (int y = 1; y < 48; y++)
            {
                uint16_t startColor = (1 << (y % 4)) * 100 + 1 + ((uint16_t)y / 4);

                for (int x = 0; x < 20; x++)
                {
                    int colorIndex = (startColor + ((startColor + (startColor << 2)) << 1)) << 3;
                    colorIndex += (colorIndex / colorOffsetDivider) << 2;
                    uint16_t color = *(uint16_t *)(huesData + colorIndex);

                    uint32_t clr = g_ColorManager.Color16To32(color);

                    CGUIColoredPolygone *polygone =
                        (CGUIColoredPolygone *)Add(new CGUIColoredPolygone(
                            ID_CCS_COLOR_RANGE + ((int)y * 20 + (int)x),
                            startColor,
                            492 + ((int)x * 6),
                            148 + ((int)y * 5),
                            6,
                            5,
                            clr));
                    polygone->DrawDot = true;

                    startColor += 5;
                }
            }
        }
        else if (
            g_CreateCharacterScreen.GetColorSelection() == CCSID_HAIR_COLOR ||
            g_CreateCharacterScreen.GetColorSelection() == CCSID_FACIAL_HAIR_COLOR)
        {
            const uint16_t *colorPtr = g_CreateCharacterManager.GetHairColorPtr();
            int maxY = 8;
            int polygoneHeight = 35;

            if (g_CreateCharacterManager.GetRace() == RT_ELF)
            {
                maxY = 9;
                polygoneHeight = 31;
            }
            else if (g_CreateCharacterManager.GetRace() == RT_GARGOYLE)
            {
                maxY = 3;
                polygoneHeight = 93;
            }

            for (int y = 0; y < maxY; y++)
            {
                for (int x = 0; x < 6; x++)
                {
                    uint16_t startColor = *colorPtr++;

                    int colorIndex = (startColor + ((startColor + (startColor << 2)) << 1)) << 3;
                    colorIndex += (colorIndex / colorOffsetDivider) << 2;
                    uint16_t color = *(uint16_t *)(huesData + colorIndex);

                    uint32_t clr = g_ColorManager.Color16To32(color);

                    CGUIColoredPolygone *polygone =
                        (CGUIColoredPolygone *)Add(new CGUIColoredPolygone(
                            ID_CCS_COLOR_RANGE + ((int)x * maxY + (int)y),
                            startColor,
                            490 + ((int)x * 20),
                            140 + ((int)y * polygoneHeight),
                            20,
                            polygoneHeight,
                            clr));
                    polygone->DrawDot = true;
                }
            }
        }
    }

    //the colorizer shader wraps the paperdoll layers - without it the hue
    //fields on the gumps never apply
    Add(new CGUIShader(&g_ColorizerShader, true));

    uint16_t gumpID = g_CreateCharacterManager.GetBodyGump();

    if (gumpID != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppic(gumpID, 238, 98));
        gumppic->Color = g_CreateCharacterManager.SkinTone;
    }

    gumpID = g_CreateCharacterManager.GetBootsGump();

    if (gumpID != 0u)
    {
        Add(new CGUIGumppic(gumpID, 238, 98));
    }

    gumpID = g_CreateCharacterManager.GetPantsGump();

    if (gumpID != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppic(gumpID, 238, 98));
        gumppic->Color = g_CreateCharacterManager.PantsColor;
    }

    gumpID = g_CreateCharacterManager.GetShirtGump();

    if (gumpID != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppic(gumpID, 238, 98));
        gumppic->Color = g_CreateCharacterManager.ShirtColor;
    }

    if (g_CreateCharacterManager.HairStyle != 0u)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppic(
            g_CreateCharacterManager.GetHair(g_CreateCharacterManager.HairStyle).GumpID, 238, 98));
        gumppic->Color = g_CreateCharacterManager.HairColor;
    }

    if (!g_CreateCharacterManager.GetFemale() && (g_CreateCharacterManager.BeardStyle != 0u) &&
        g_CreateCharacterManager.GetRace() != RT_ELF)
    {
        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppic(
            g_CreateCharacterManager.GetBeard(g_CreateCharacterManager.BeardStyle).GumpID,
            238,
            98));
        gumppic->Color = g_CreateCharacterManager.BeardColor;
    }

    Add(new CGUIShader(nullptr, false));

    if (g_Config.ClientVersion < CV_4011D)
    {
        if (g_CreateCharacterManager.GetFemale())
        {
            Add(new CGUIButton(ID_CCS_MALE_BUTTON, 0x070D, 0x070E, 0x070F, 310, 408));
        }
        else
        {
            Add(new CGUIButton(ID_CCS_FEMALE_BUTTON, 0x0710, 0x0711, 0x0712, 310, 408));
        }
    }
    else
    {
        Add(new CGUIGroup(1));

        CGUIRadio *radio =
            (CGUIRadio *)Add(new CGUIRadio(ID_CCS_MALE_BUTTON, 0x0768, 0x0767, 0x0767, 425, 435));
        radio->Checked = !g_CreateCharacterManager.GetFemale();
        radio =
            (CGUIRadio *)Add(new CGUIRadio(ID_CCS_FEMALE_BUTTON, 0x0768, 0x0767, 0x0767, 425, 455));
        radio->Checked = g_CreateCharacterManager.GetFemale();

        Add(new CGUIGroup(0));

        Add(new CGUIButton(ID_CCS_MALE_BUTTON, 0x0710, 0x0711, 0x0712, 445, 435));
        Add(new CGUIButton(ID_CCS_FEMALE_BUTTON, 0x070D, 0x070E, 0x070F, 445, 455));

        Add(new CGUIGroup(2));

        radio = (CGUIRadio *)Add(
            new CGUIRadio(ID_CCS_HUMAN_RACE_BUTTON, 0x0768, 0x0767, 0x0767, 180, 435));
        radio->Checked = (g_CreateCharacterManager.GetRace() == RT_HUMAN);
        radio = (CGUIRadio *)Add(
            new CGUIRadio(ID_CCS_ELF_RACE_BUTTON, 0x0768, 0x0767, 0x0767, 180, 455));
        radio->Checked = (g_CreateCharacterManager.GetRace() == RT_ELF);

        Add(new CGUIButton(ID_CCS_HUMAN_RACE_BUTTON, 0x0702, 0x0703, 0x0704, 200, 435));
        Add(new CGUIButton(ID_CCS_ELF_RACE_BUTTON, 0x0705, 0x0706, 0x0707, 200, 455));

        if (g_Config.ClientVersion >= CV_60144)
        {
            radio = (CGUIRadio *)Add(
                new CGUIRadio(ID_CCS_GARGOYLE_RACE_BUTTON, 0x0768, 0x0767, 0x0767, 60, 435));
            radio->Checked = (g_CreateCharacterManager.GetRace() == RT_GARGOYLE);

            Add(new CGUIButton(ID_CCS_GARGOYLE_RACE_BUTTON, 0x07D3, 0x07D4, 0x07D5, 80, 435));
        }

        Add(new CGUIGroup(0));
    }
}

//popup rows in the original's order (0x411C7F/0x411DEE), mapped onto the
//manager tables (whose Topknot/Buns positions differ)
static const uint8_t g_1998HairOrderMale[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 6 };
static const uint8_t g_1998HairOrderFemale[] = { 0, 1, 2, 3, 4, 5, 10, 7, 8, 9, 6 };

//the preview hair/beard overlay canvases (260x237, aligned to the body
//draw rect), indexed by popup position - 1. Male: Short..Receding are
//0xC60C..0xC612, then the run breaks (2 Tails/Topknot sit at 0xC619/
//0xC61B - confirmed on the dump sheets); female and beard are clean runs
static const uint16_t g_1998HairArtMale[] = {
    0xC60C, 0xC60D, 0xC60E, 0xC60F, 0xC612, 0xC616, 0xC61A, 0xC619, 0xC61B,
};
static const uint16_t g_1998HairArtFemale[] = {
    0xED1C, 0xED1D, 0xED1E, 0xED1F, 0xED20, 0xED21, 0xED22, 0xED23, 0xED24, 0xED25,
};
static const uint16_t g_1998BeardArt[] = { 0xC670, 0xC671, 0xC672, 0xC673, 0xC674, 0xC675,
    0xC676 };

//1.25.32 (1998) creation wizard step 3: CharCreation - sex toggle, the
//hair/beard pickers with their scrollchoice lists, the six hue sliders
//and the live character preview; PREV returns to the skill picks, NEXT
//opens the town/location step
void CGumpScreenCreateCharacter::UpdateContent1998()
{
    //hardening: drop the scrollchoices' cached child pointers (Clear()
    //frees them) and the slider/label/preview pointers with them
    m_1998HairPopup.Reset();
    m_1998BeardPopup.Reset();
    for (int i = 0; i < 6; i++)
    {
        m_1998HueSliders[i] = nullptr;
    }
    m_1998StyleName[0] = nullptr;
    m_1998StyleName[1] = nullptr;
    m_1998PreviewBody = nullptr;
    m_1998PreviewHair = nullptr;
    m_1998PreviewBeard = nullptr;

    //the wood backdrop (640x480) stays behind every 1998 login-flow screen
    Add(new CGUIGumppic(0x03E8, 0, 0));

    //no text entries on this step; drop any earlier entry focus
    g_EntryPointer = nullptr;

    CCreateCharacterManager &mgr = g_CreateCharacterManager;
    const bool female = mgr.GetFemale();
    const uint8_t *hairOrder = female ? g_1998HairOrderFemale : g_1998HairOrderMale;
    const int hairRows = female ? 11 : 10;

    Add(new CGUIPage(1)); //the CharCreation panel

    //CharCreation backdrop (413x370) at the gump origin (the ctor draws it
    //into the prerendered surface at 0,0 and stores the canvas origin 0x6C)
    Add(new CGUIGumppic(0x04EC, 108, 0));

    //side paper scrolls (sub-gump ctors 0x41954E/0x4195D5: left (80,131),
    //right (505,129)). They are the COLLAPSED selection lists: the left
    //one is replaced by the hair popup, the right one by the beard popup
    if (m_1998OpenPopup != 1)
    {
        Add(new CGUIGumppic(0x04ED, 80, 131));
    }
    if (m_1998OpenPopup != 2)
    {
        Add(new CGUIGumppic(0x04EE, 505, 129));
    }

    //sex toggle: flips male/female, resetting the hair style to Short and
    //forcing beard NONE on female
    Add(new CGUIButton(ID_CCS_1998_SEX, 0x04F7, 0x04F8, 0x04F9, 284, 200));

    //hair style picker -> scrollchoice + the current style name
    Add(new CGUIButton(ID_CCS_1998_HAIR, 0x04FA, 0x04FB, 0x04FC, 160, 204));
    m_1998StyleName[0] = (CGUIText *)Add(new CGUIText(0x0386, 154, 230));
    m_1998StyleName[0]->CreateTextureA(1, mgr.GetHair(mgr.HairStyle).Name);

    //the beard picker exists for males only; on female it stays visible
    //but disabled (the original greys it out, [btn+0xB0]=1 at 0x419B95)
    CGUIButton *beardButton = (CGUIButton *)Add(new CGUIButton(
        ID_CCS_1998_BEARD, 0x04FD, 0x04FE, 0x04FF, 374, 204));
    m_1998StyleName[1] = (CGUIText *)Add(new CGUIText(0x0386, 368, 230));
    m_1998StyleName[1]->CreateTextureA(1, mgr.GetBeard(mgr.BeardStyle).Name);
    if (female)
    {
        beardButton->Enabled = false;
        beardButton->Color = 0x0386; //the dark grey ramp
        beardButton->PartialHue = true;
    }

    //the character preview, recomposed per the original's 0x419CB0 layer
    //order: clothing back layer, body (skin hue), clothes overlay, hair
    //and beard (color hues), then the fixed detail overlays. The doll's
    //top edge intentionally rises above the panel (canvas y -40). The
    //colorizer shader wraps the layers - without it the hues never
    //apply (the modern creation screen does the same)
    Add(new CGUIShader(&g_ColorizerShader, true));
    Add(new CGUIGumppic(female ? 0x04F0 : 0x04EF, 285, 221));
    m_1998PreviewBody =
        (CGUIGumppic *)Add(new CGUIGumppic(female ? 0x000D : 0x000C, 232, -40));
    m_1998PreviewBody->Color = mgr.SkinTone;
    m_1998PreviewBody->PartialHue = true;
    Add(new CGUIGumppic(female ? 0xEC12 : 0xC502, 232, -40));
    if (mgr.HairStyle != 0)
    {
        //the overlay canvas of the picked style (popup position - 1)
        int hairPos = 0;
        for (int i = 0; i < hairRows; i++)
        {
            if (hairOrder[i] == mgr.HairStyle)
            {
                hairPos = i;
                break;
            }
        }
        const uint16_t hairArt =
            female ? g_1998HairArtFemale[hairPos - 1] : g_1998HairArtMale[hairPos - 1];
        m_1998PreviewHair = (CGUIGumppic *)Add(new CGUIGumppic(hairArt, 232, -40));
        m_1998PreviewHair->Color = mgr.HairColor;
        m_1998PreviewHair->PartialHue = true;
    }
    if (!female && mgr.BeardStyle != 0)
    {
        m_1998PreviewBeard = (CGUIGumppic *)Add(
            new CGUIGumppic(g_1998BeardArt[mgr.BeardStyle - 1], 232, -40));
        m_1998PreviewBeard->Color = mgr.BeardColor;
        m_1998PreviewBeard->PartialHue = true;
    }
    Add(new CGUIGumppic(0xC5BE, 232, -40));
    Add(new CGUIGumppic(female ? 0xEC21 : 0xC4FF, 232, -40));
    Add(new CGUIGumppic(female ? 0xEC40 : 0xC530, 232, -40));

    //the six hue sliders (slider sub-gumps 0x413740, knob 0x4B9/0x4BA, no
    //embedded background). Each part color is ONE hue id driven by a
    //coarse+fine slider pair: hair/beard = 1102 + fine(0..7) +
    //coarse(0..5)*8 (the wire-aligned range 1102..1149; the original's
    //gump-side 1101 maps to wire 1102 via its +1 packet encoding, and
    //1101 itself renders uncolored in crossuo); skin = 1002 + group*8 +
    //shade (the original walks skinhue.txt runs, 0x441060; the file is
    //absent from the 1998 install, so the 8-shade grid stands in).
    //Canvas positions from ctor 0x418D60. 0=hair fine, 1=hair coarse,
    //2=beard fine, 3=beard coarse, 4=skin coarse, 5=skin fine; a pair
    //hides while its style is NONE
    static const int sliderPos[6][2] = {
        { 159, 271 }, { 154, 251 }, { 376, 271 }, { 370, 251 }, { 267, 270 }, { 262, 290 },
    };
    static const int sliderLen[6] = { 92, 102, 91, 103, 92, 102 };
    static const int sliderMax[6] = { 7, 5, 7, 5, 6, 7 };
    const int sliderValue[6] = { (mgr.HairColor - 1102) % 8, (mgr.HairColor - 1102) / 8,
        (mgr.BeardColor - 1102) % 8, (mgr.BeardColor - 1102) / 8, (mgr.SkinTone - 1002) / 8,
        (mgr.SkinTone - 1002) % 8 };
    for (int i = 0; i < 6; i++)
    {
        if (i < 2 && mgr.HairStyle == 0)
        {
            continue;
        }
        if ((i == 2 || i == 3) && (female || mgr.BeardStyle == 0))
        {
            continue;
        }

        m_1998HueSliders[i] = (CGUISlider *)Add(new CGUISlider(ID_CCS_1998_HUE_SLIDER + i,
            0x04B9, 0x04BA, 0x04BA, 0, false, false, sliderPos[i][0], sliderPos[i][1],
            sliderLen[i], 0, sliderMax[i], sliderValue[i]));
    }

    //PREV -> the skill picks; NEXT -> the town/location step
    Add(new CGUIButton(ID_CCS_1998_BACK, 0x04F1, 0x04F2, 0x04F3, 168, 151));
    Add(new CGUIButton(ID_CCS_1998_NEXT, 0x04F4, 0x04F5, 0x04F6, 405, 150));

    //the hair/beard scrollchoices draw OVER the panel (the original keeps
    //the gump visible and replaces the matching side scroll art)
    if (m_1998OpenPopup == 1)
    {
        //hair styles (the original's row order; Buns is female-only)
        std::vector<astr_t> rows;
        for (int i = 0; i < hairRows; i++)
        {
            rows.push_back(mgr.GetHair(hairOrder[i]).Name);
        }
        int hairSelected = 0;
        for (int i = 0; i < hairRows; i++)
        {
            if (hairOrder[i] == mgr.HairStyle)
            {
                hairSelected = i;
                break;
            }
        }
        m_1998HairPopup.Create(this, ID_CCS_1998_HAIR_POPUP_ROW, ID_CCS_1998_HAIR_POPUP_UP,
            ID_CCS_1998_HAIR_POPUP_DOWN, ID_CCS_1998_HAIR_POPUP_THUMB, false, rows,
            0, hairSelected);
    }
    if (m_1998OpenPopup == 2 && !female)
    {
        //beard styles (males only)
        std::vector<astr_t> rows;
        for (int i = 0; i < mgr.GetCurrentFacialHairCount(); i++)
        {
            rows.push_back(mgr.GetBeard((uint8_t)i).Name);
        }
        m_1998BeardPopup.Create(this, ID_CCS_1998_BEARD_POPUP_ROW,
            ID_CCS_1998_BEARD_POPUP_UP, ID_CCS_1998_BEARD_POPUP_DOWN,
            ID_CCS_1998_BEARD_POPUP_THUMB, true, rows, 0, mgr.BeardStyle);
    }
}

void CGumpScreenCreateCharacter::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips || g_SelectedObject.Object == nullptr)
    {
        return;
    }

    uint32_t id = g_SelectedObject.Serial;

    switch (id)
    {
        case ID_CCS_QUIT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        case ID_CCS_ARROW_NEXT:
        {
            g_ToolTip.Set(L"Accept this character");
            break;
        }
        case ID_CCS_ARROW_PREV:
        {
            g_ToolTip.Set(L"Back to character generation screen", 150);
            break;
        }
        default:
            break;
    }
}

void CGumpScreenCreateCharacter::GUMP_BUTTON_EVENT_C
{
    //1.25.32 CharCreation controls (disjoint id blocks: hue sliders /
    //popup rows/arrows / panel buttons never share ids)
    if (g_Config.ClientVersion < CV_200)
    {
        CCreateCharacterManager &mgr = g_CreateCharacterManager;
        const bool female = mgr.GetFemale();

        if (serial == ID_CCS_1998_SEX)
        {
            //flipping resets the hair style to Short (0x419AB9) and forces
            //beard NONE on female (the beard controls hide and the packet
            //sends beard id 0)
            mgr.SetFemale(!mgr.GetFemale());
            mgr.HairStyle = 1;
            if (mgr.GetFemale())
            {
                mgr.BeardStyle = 0;
            }

            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (serial == ID_CCS_1998_HAIR)
        {
            //the picker toggles the list; it replaces the LEFT scroll art
            m_1998OpenPopup = m_1998OpenPopup == 1 ? 0 : 1;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (serial == ID_CCS_1998_BEARD)
        {
            //the picker toggles the list; it replaces the RIGHT scroll art
            m_1998OpenPopup = m_1998OpenPopup == 2 ? 0 : 2;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (m_1998HairPopup.IsRow(serial))
        {
            //hair style row: the row index is the original's popup order,
            //mapped back onto the manager table; the pick closes the list
            const int index = serial - ID_CCS_1998_HAIR_POPUP_ROW;
            if (index < (female ? 11 : 10))
            {
                mgr.HairStyle = female ? g_1998HairOrderFemale[index]
                                       : g_1998HairOrderMale[index];
            }

            m_1998OpenPopup = 0;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (m_1998BeardPopup.IsRow(serial))
        {
            //beard style row; the pick closes the list
            const int index = serial - ID_CCS_1998_BEARD_POPUP_ROW;
            if (index < mgr.GetCurrentFacialHairCount())
            {
                mgr.BeardStyle = (uint8_t)index;
            }

            m_1998OpenPopup = 0;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (
            serial == ID_CCS_1998_HAIR_POPUP_UP || serial == ID_CCS_1998_HAIR_POPUP_DOWN)
        {
            m_1998HairPopup.SetTop(
                m_1998HairPopup.Top() + (serial == ID_CCS_1998_HAIR_POPUP_UP ? -1 : 1));
            WantRedraw = true;
        }
        else if (
            serial == ID_CCS_1998_BEARD_POPUP_UP || serial == ID_CCS_1998_BEARD_POPUP_DOWN)
        {
            m_1998BeardPopup.SetTop(
                m_1998BeardPopup.Top() + (serial == ID_CCS_1998_BEARD_POPUP_UP ? -1 : 1));
            WantRedraw = true;
        }
        else if (serial == ID_CCS_1998_BACK)
        {
            //PREV: back to the skill picks
            g_CreateCharacterScreen.CreateSmoothAction(
                CCreateCharacterScreen::ID_SMOOTH_CCS_GO_SCREEN_PROFESSION);
        }
        else if (serial == ID_CCS_1998_NEXT)
        {
            //NEXT: on to the town/location step
            g_CreateCharacterScreen.CreateSmoothAction(
                CCreateCharacterScreen::ID_SMOOTH_CCS_GO_SCREEN_SELECT_TOWN);
        }

        return;
    }

    if (serial == ID_CCS_QUIT)
    { //x button
        g_CreateCharacterScreen.CreateSmoothAction(CCreateCharacterScreen::ID_SMOOTH_CCS_QUIT);
    }
    else if (serial == ID_CCS_ARROW_PREV)
    { //< button
        g_CreateCharacterScreen.CreateSmoothAction(
            CCreateCharacterScreen::ID_SMOOTH_CCS_GO_SCREEN_CHARACTER);
    }
    else if (serial == ID_CCS_ARROW_NEXT) //> button
    {
        if (g_EntryPointer->Length() < 2)
        {
            g_CreateCharacterScreen.CreateSmoothAction(
                CCreateCharacterScreen::ID_SMOOTH_CCS_GO_SCREEN_CONNECT);
        }
        else
        {
            g_CreateCharacterScreen.CreateSmoothAction(
                CCreateCharacterScreen::ID_SMOOTH_CCS_GO_SCREEN_SELECT_TOWN);
        }
    }
    else if (serial == ID_CCS_FEMALE_BUTTON)
    {
        g_CreateCharacterManager.SetFemale(true);
        WantUpdateContent = true;
        g_CreateCharacterScreen.SetColorSelection(0);
    }
    else if (serial == ID_CCS_MALE_BUTTON)
    {
        g_CreateCharacterManager.SetFemale(false);
        WantUpdateContent = true;
        g_CreateCharacterScreen.SetColorSelection(0);
    }
    else if (serial == ID_CCS_HUMAN_RACE_BUTTON)
    {
        g_CreateCharacterManager.SetRace(RT_HUMAN);
        WantUpdateContent = true;
        g_CreateCharacterScreen.SetColorSelection(0);
    }
    else if (serial == ID_CCS_ELF_RACE_BUTTON)
    {
        g_CreateCharacterManager.SetRace(RT_ELF);
        WantUpdateContent = true;
        g_CreateCharacterScreen.SetColorSelection(0);
    }
    else if (serial == ID_CCS_GARGOYLE_RACE_BUTTON)
    {
        g_CreateCharacterManager.SetRace(RT_GARGOYLE);
        WantUpdateContent = true;
        g_CreateCharacterScreen.SetColorSelection(0);
    }
}

void CGumpScreenCreateCharacter::GUMP_RADIO_EVENT_C
{
    if (state)
    {
        if (serial == ID_CCS_MALE_BUTTON)
        {
            g_CreateCharacterManager.SetFemale(false);
        }
        else if (serial == ID_CCS_FEMALE_BUTTON)
        {
            g_CreateCharacterManager.SetFemale(true);
        }
        else if (serial == ID_CCS_HUMAN_RACE_BUTTON)
        {
            g_CreateCharacterManager.SetRace(RT_HUMAN);
        }
        else if (serial == ID_CCS_ELF_RACE_BUTTON)
        {
            g_CreateCharacterManager.SetRace(RT_ELF);
        }
        else if (serial == ID_CCS_GARGOYLE_RACE_BUTTON)
        {
            g_CreateCharacterManager.SetRace(RT_GARGOYLE);
        }

        WantUpdateContent = true;
        g_CreateCharacterScreen.SetColorSelection(0);
    }
}

void CGumpScreenCreateCharacter::GUMP_TEXT_ENTRY_EVENT_C
{
    if (g_CreateCharacterScreen.GetColorSelection() == 0)
    {
        if (serial == ID_CCS_SKIN_TONE)
        {
            g_CreateCharacterScreen.SetColorSelection(CCSID_SKIN_TONE);
        }
        else if (serial == ID_CCS_SHIRT_COLOR)
        {
            g_CreateCharacterScreen.SetColorSelection(CCSID_SHIRT_COLOR);
        }
        else if (serial == ID_CCS_SKIRT_OR_PANTS_COLOR)
        {
            g_CreateCharacterScreen.SetColorSelection(CCSID_SKIRT_OR_PANTS_COLOR);
        }
        else if (serial == ID_CCS_HAIR_COLOR)
        {
            g_CreateCharacterScreen.SetColorSelection(CCSID_HAIR_COLOR);
        }
        else if (!g_CreateCharacterManager.GetFemale() && serial == ID_CCS_FACIAL_HAIR_COLOR)
        {
            g_CreateCharacterScreen.SetColorSelection(CCSID_FACIAL_HAIR_COLOR);
        }

        WantUpdateContent = true;
    }
    else
    {
        if (serial >= ID_CCS_COLOR_RANGE)
        {
            uint16_t color = g_SelectedObject.Object->Color + 1;

            if (g_CreateCharacterScreen.GetColorSelection() == CCSID_SKIN_TONE)
            {
                if (g_CreateCharacterManager.GetRace() == RT_HUMAN)
                {
                    if (color < 0x03EA)
                    {
                        color = 0x03EA;
                    }
                    else if (color > 0x0422)
                    {
                        color = 0x0422;
                    }
                }

                g_CreateCharacterManager.SkinTone = color;
            }
            else if (
                g_CreateCharacterScreen.GetColorSelection() == CCSID_SHIRT_COLOR ||
                g_CreateCharacterScreen.GetColorSelection() == CCSID_SKIRT_OR_PANTS_COLOR)
            {
                if (g_CreateCharacterScreen.GetColorSelection() == CCSID_SHIRT_COLOR)
                {
                    g_CreateCharacterManager.ShirtColor = color;
                }
                else
                {
                    g_CreateCharacterManager.PantsColor = color;
                }
            }
            else if (
                g_CreateCharacterScreen.GetColorSelection() == CCSID_HAIR_COLOR ||
                g_CreateCharacterScreen.GetColorSelection() == CCSID_FACIAL_HAIR_COLOR)
            {
                if (g_CreateCharacterManager.GetRace() == RT_HUMAN)
                {
                    if (color < 0x044E)
                    {
                        color = 0x044E;
                    }
                    else if (color > 0x047C)
                    {
                        color = 0x047C;
                    }
                }

                if (g_CreateCharacterScreen.GetColorSelection() == CCSID_HAIR_COLOR)
                {
                    g_CreateCharacterManager.HairColor = color;
                }
                else
                {
                    g_CreateCharacterManager.BeardColor = color;
                }
            }

            g_CreateCharacterScreen.SetColorSelection(0);
            WantUpdateContent = true;
        }
    }
}

void CGumpScreenCreateCharacter::GUMP_COMBOBOX_SELECTION_EVENT_C
{
    if (serial >= ID_CCS_HAIR_STYLE)
    {
        if (serial >= ID_CCS_FACIAL_HAIR_STYLE)
        {
            uint8_t index = serial - ID_CCS_FACIAL_HAIR_STYLE;

            if (index < g_CreateCharacterManager.GetCurrentFacialHairCount())
            {
                g_CreateCharacterManager.BeardStyle = (uint8_t)index;
            }

            WantUpdateContent = true;
        }
        else
        {
            uint8_t index = serial - ID_CCS_HAIR_STYLE;

            if (index < g_CreateCharacterManager.GetCurrentHairCount())
            {
                g_CreateCharacterManager.HairStyle = index;
            }

            WantUpdateContent = true;
        }
    }
}

void CGumpScreenCreateCharacter::GUMP_SLIDER_CLICK_EVENT_C
{
    OnSliderMove(serial);
}

void CGumpScreenCreateCharacter::GUMP_SLIDER_MOVE_EVENT_C
{
    //1.25.32 CharCreation hue sliders (0x4198A0): each part color is one
    //hue id driven by a coarse+fine slider pair - hair/beard =
    //1102 + fine(0..7) + coarse(0..5)*8, skin = 1002 + coarse(0..6)*8 +
    //fine(0..7). The preview layers re-hue live (Color is read every
    //draw); no rebuild (a slider is being dragged)
    if (g_Config.ClientVersion < CV_200)
    {
        //scrollchoice thumb drags: read the slider value back into the
        //matching row window (the row textures refresh in place)
        if (m_1998HairPopup.IsThumb(serial))
        {
            m_1998HairPopup.SyncThumb();
            WantRedraw = true;
            return;
        }
        if (m_1998BeardPopup.IsThumb(serial))
        {
            m_1998BeardPopup.SyncThumb();
            WantRedraw = true;
            return;
        }

        if (serial >= ID_CCS_1998_HUE_SLIDER && serial < ID_CCS_1998_HUE_SLIDER + 6)
        {
            CCreateCharacterManager &mgr = g_CreateCharacterManager;
            const int idx = serial - ID_CCS_1998_HUE_SLIDER;
            const int value = m_1998HueSliders[idx]->Value;

            switch (idx)
            {
                case 0: //hair fine
                {
                    mgr.HairColor =
                        (uint16_t)(1102 + value + ((mgr.HairColor - 1102) / 8) * 8);
                    if (m_1998PreviewHair != nullptr)
                    {
                        m_1998PreviewHair->Color = mgr.HairColor;
                    }
                    break;
                }
                case 1: //hair coarse
                {
                    mgr.HairColor =
                        (uint16_t)(1102 + (mgr.HairColor - 1102) % 8 + value * 8);
                    if (m_1998PreviewHair != nullptr)
                    {
                        m_1998PreviewHair->Color = mgr.HairColor;
                    }
                    break;
                }
                case 2: //beard fine
                {
                    mgr.BeardColor =
                        (uint16_t)(1102 + value + ((mgr.BeardColor - 1102) / 8) * 8);
                    if (m_1998PreviewBeard != nullptr)
                    {
                        m_1998PreviewBeard->Color = mgr.BeardColor;
                    }
                    break;
                }
                case 3: //beard coarse
                {
                    mgr.BeardColor =
                        (uint16_t)(1102 + (mgr.BeardColor - 1102) % 8 + value * 8);
                    if (m_1998PreviewBeard != nullptr)
                    {
                        m_1998PreviewBeard->Color = mgr.BeardColor;
                    }
                    break;
                }
                case 4: //skin coarse
                {
                    mgr.SkinTone =
                        (uint16_t)(1002 + value * 8 + (mgr.SkinTone - 1002) % 8);
                    if (m_1998PreviewBody != nullptr)
                    {
                        m_1998PreviewBody->Color = mgr.SkinTone;
                    }
                    break;
                }
                case 5: //skin fine
                {
                    mgr.SkinTone =
                        (uint16_t)(1002 + ((mgr.SkinTone - 1002) / 8) * 8 + value);
                    if (m_1998PreviewBody != nullptr)
                    {
                        m_1998PreviewBody->Color = mgr.SkinTone;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }

            WantRedraw = true;
            return;
        }
    }
}
