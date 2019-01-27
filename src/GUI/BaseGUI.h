// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../RenderObject.h"
#include "../Point.h"
#include "../api/enumlist.h"

class CTextData;

class CBaseGUI : public CRenderObject
{
public:
    // Component Type
    GUMP_OBJECT_TYPE Type = GOT_NONE;

    // The ability to move the gump, if the component is clamped
    bool MoveOnDrag = false;

    // Flag for using PartialHue
    bool PartialHue = false;

    // Method of using the selection function
    // true - checks if the mouse cursor is within the polygon (for CGUIPolygonal and a component with a width and height, or takes the dimensions of the original Graphic state image)
    // false - standard pixel check function
    bool CheckPolygone = false;

    // Turning off the processing of the component (the component is displayed)
    bool Enabled = true;

    // Show / hide component and its processing
    bool Visible = true;

    // Flag, responsible only for the selection of components, without its display in the gump
    bool SelectOnly = false;

    // Flag that is responsible only for drawing the component, without selecting it
    bool DrawOnly = false;
    uint32_t ClilocID = 0;

    CTextData *TextData = nullptr;

public:
    CBaseGUI(GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y);
    virtual ~CBaseGUI();

    // Identify the g_EntryPointer field in this component
    virtual bool EntryPointerHere() { return false; }

    // Set data for shader
    virtual void SetShaderMode() {}

    // Get the ID of the image to display
    virtual uint16_t GetDrawGraphic() { return Graphic; }

    // Draw component
    // checktrans - use stencil + transparency
    virtual void Draw(bool checktrans = false) {}

    // Check component for selection
    virtual bool Select() { return false; }

    // Get the final dimensions of the components
    virtual CSize GetSize() { return CSize(); }

    // This is a user interface component.
    virtual bool IsGUI() { return true; }

    // This is the HTMLGump component
    virtual bool IsHTMLGump() { return false; }

    // This is the component of HTMLGump (scroll buttons, scroller, background, output area)
    virtual bool IsControlHTML() { return false; }

    // Ability to handle component events if it is clamped, but the mouse is somewhere else
    virtual bool IsPressedOuthit() { return false; }
};
