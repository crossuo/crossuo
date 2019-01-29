// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../TextEngine/EntryText.h"

class CGUITextEntry : public CBaseGUI
{
public:
    //The color of the text in the selected state
    uint16_t ColorSelected = 0;

    //Text color in focus state
    uint16_t ColorFocused = 0;

    //The text is in Unicode
    bool Unicode = false;

    //The typeface
    uint8_t Font = 0;

    //Text orientation
    TEXT_ALIGN_TYPE Align = TS_LEFT;

    //Flags of text
    uint16_t TextFlags = 0;

    //To select an object by serial number can be selected by background, hit-box, etc.)
    bool CheckOnSerial = false;

    //Read only
    bool ReadOnly = false;

    //Flag the focus
    bool Focused = false;

    //Flag of global color use
    bool UseGlobalColor = false;

    //R-component of global OGL color
    uint8_t GlobalColorR = 0;

    //G-component of global OGL color
    uint8_t GlobalColorG = 0;

    //B-component of global OGL color
    uint8_t GlobalColorB = 0;

    //Alpha-channel global OGL color
    uint8_t GlobalColorA = 0;

    //R-component of global OGL color when the component is selected
    uint8_t GlobalColorSelectedR = 0;

    //G-component of global OGL color when the component is selected
    uint8_t GlobalColorSelectedG = 0;

    //B-component of global OGL color when the component is selected
    uint8_t GlobalColorSelectedB = 0;

    //Alpha-channel global OGL colors when a component is selected
    uint8_t GlobalColorSelectedA = 0;

    //R-component of global OGL color when the component is in focus
    uint8_t GlobalColorFocusedR = 0;

    //G-component of global OGL color when the component is in focus
    uint8_t GlobalColorFocusedG = 0;

    //B-component of global OGL color when the component is in focus
    uint8_t GlobalColorFocusedB = 0;

    //Alpha channel global OGL colors when the component is in focus
    uint8_t GlobalColorFocusedA = 0;

    //Text offset when component is in focus
    char FocusedOffsetY = 0;

    CGUITextEntry(
        int serial,
        uint16_t color,
        uint16_t colorSelected,
        uint16_t colorFocused,
        int x,
        int y,
        int maxWidth = 0,
        bool unicode = true,
        uint8_t font = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0,
        int maxLength = 0);
    virtual ~CGUITextEntry();

    //The class object for text input
    class CEntryText m_Entry;

    virtual void PrepareTextures();

    virtual bool EntryPointerHere();

    virtual CSize GetSize();

    //Set global OGL color before drawing text
    void SetGlobalColor(bool use, int color, int selected, int focused);

    //Pressing the component
    void OnClick(CGump *gump, int x, int y);

    virtual void OnMouseEnter();
    virtual void OnMouseExit();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};
