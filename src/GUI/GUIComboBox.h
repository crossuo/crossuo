// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIComboBox : public CBaseGUI
{
    //! Picture ID in the expanded state
    uint16_t OpenGraphic = 0;

    //! Compound-background flag: when set, five repeated m_OpenGraphic tiles are drawn; when clear, a Resizepic built from m_OpenGraphic
    bool CompositeBackground = false;

    //! Number of visible rows in the open list
protected:
    int m_ShowItemsCount = 0;

public:
    int GetShowItemsCount() { return m_ShowItemsCount; };
    void SetShowItemsCount(int val);

    //! Width in the collapsed state (used only for the Resizepic)
    int Width = 0;

    //! Width in the expanded state (used only for the Resizepic)
    int OpenedWidth = 0;

    //! Start index to display in the expanded state
    int StartIndex = 0;

    //! Currently selected index
    int SelectedIndex = -1;

    //! Scrolling direction (computed in Select())
    int ListingDirection = 0;

    //! Scrolling timer
    uint32_t ListingTimer = 0;

    //! Center the expanded state on the Y axis
    bool ShowMaximizedCenter = false;

    //! Text offset for display
    int TextOffsetY = 0;

private:
    //! Get a pointer to the first display object
    CBaseGUI *SkipToStart();

    //! Down-arrow position (collapsed state)
    int m_MinimizedArrowX{ 0 };

    //! Down-arrow position
    int m_ArrowX{ 0 };

    //! Y offset
    int m_OffsetY{ 0 };

    //! Y step
    int m_StepY{ 0 };

    //! Working width
    int m_WorkWidth{ 0 };

    //! Working height
    int m_WorkHeight{ 0 };

    //! Text shown in the collapsed state
    class CGUIText *Text{ nullptr };

public:
    CGUIComboBox(
        int serial,
        uint16_t graphic,
        bool compositeBackground,
        uint16_t openGraphic,
        int x,
        int y,
        int width,
        int showItemsCount,
        bool showMaximizedCenter);
    virtual ~CGUIComboBox() override;

    //! Set the text
    void SetText(class CGUIText *text) { Text = text; }

    //! Recalculate the width
    void RecalculateWidth();

    virtual CSize GetSize() override;

    virtual void PrepareTextures() override;

    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;

    //! Get a reference to the selected list item (or to this object)
    CBaseGUI *SelectedItem();

    //! Get the selected item's index, -1 if nothing is selected
    int IsSelectedItem();
};
