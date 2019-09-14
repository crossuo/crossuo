﻿// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Platform.h"
#include "../GUI/GUI.h"
#include "../RenderObject.h"

class CBaseGUI;

#define GUMP_BUTTON_EVENT_C OnButton(int serial)
#define GUMP_CHECKBOX_EVENT_C OnCheckbox(int serial, bool state)
#define GUMP_RADIO_EVENT_C OnRadio(int serial, bool state)
#define GUMP_TEXT_ENTRY_EVENT_C OnTextEntry(int serial)
#define GUMP_SLIDER_CLICK_EVENT_C OnSliderClick(int serial)
#define GUMP_SLIDER_MOVE_EVENT_C OnSliderMove(int serial)
#define GUMP_RESIZE_START_EVENT_C OnResizeStart(int serial)
#define GUMP_RESIZE_EVENT_C OnResize(int serial)
#define GUMP_RESIZE_END_EVENT_C OnResizeEnd(int serial)
#define GUMP_DIRECT_HTML_LINK_EVENT_C OnDirectHTMLLink(uint16_t link)
#define GUMP_COMBOBOX_SELECTION_EVENT_C OnComboboxSelection(int serial)
#define GUMP_SCROLL_BUTTON_EVENT_C OnScrollButton()

#define GUMP_BUTTON_EVENT_H virtual void GUMP_BUTTON_EVENT_C
#define GUMP_CHECKBOX_EVENT_H virtual void GUMP_CHECKBOX_EVENT_C
#define GUMP_RADIO_EVENT_H virtual void GUMP_RADIO_EVENT_C
#define GUMP_TEXT_ENTRY_EVENT_H virtual void GUMP_TEXT_ENTRY_EVENT_C
#define GUMP_SLIDER_CLICK_EVENT_H virtual void GUMP_SLIDER_CLICK_EVENT_C
#define GUMP_SLIDER_MOVE_EVENT_H virtual void GUMP_SLIDER_MOVE_EVENT_C
#define GUMP_RESIZE_START_EVENT_H virtual void GUMP_RESIZE_START_EVENT_C
#define GUMP_RESIZE_EVENT_H virtual void GUMP_RESIZE_EVENT_C
#define GUMP_RESIZE_END_EVENT_H virtual void GUMP_RESIZE_END_EVENT_C
#define GUMP_DIRECT_HTML_LINK_EVENT_H virtual void GUMP_DIRECT_HTML_LINK_EVENT_C
#define GUMP_COMBOBOX_SELECTION_EVENT_H virtual void GUMP_COMBOBOX_SELECTION_EVENT_C
#define GUMP_SCROLL_BUTTON_EVENT_H virtual void GUMP_SCROLL_BUTTON_EVENT_C

class CGump : public CRenderObject
{
public:
    GUMP_TYPE GumpType = GT_NONE;
    uint32_t ID = 0;
    int MinimizedX = 0;
    int MinimizedY = 0;
    bool NoMove = false;
    bool NoClose = false;
    bool Minimized = false;
    bool FrameCreated = false;
    bool WantRedraw = false;
    bool WantUpdateContent = true;
    bool Blocked = false;
    bool LockMoving = false;
    int Page = 0;
    int Draw2Page = 0;
    bool Transparent = false;
    bool RemoveMark = false;
    bool NoProcess = false;
    bool Visible = true;
    CRect GumpRect = CRect();

protected:
    CGUIButton m_Locker{ CGUIButton(0, 0, 0, 0, 0, 0) };

    CGLFrameBuffer m_FrameBuffer{ CGLFrameBuffer() };

    virtual void CalculateGumpState();

    virtual void RecalculateSize();

public:
    CGump();
    CGump(GUMP_TYPE type, uint32_t serial, int x, int y);
    virtual ~CGump();

    virtual void PasteClipboardData(std::wstring &data);
    static void ProcessListing();

    static bool
    ApplyTransparent(CBaseGUI *item, int page, int currentPage, const int draw2Page = 0);

    static void DrawItems(CBaseGUI *start, int currentPage, int draw2Page = 0);
    static class CRenderObject *SelectItems(CBaseGUI *start, int currentPage, int draw2Page = 0);

    static void GetItemsSize(
        CGump *gump,
        CBaseGUI *start,
        CPoint2Di &minPosition,
        CPoint2Di &maxPosition,
        CPoint2Di &offset,
        int count,
        int currentPage,
        int draw2Page);
    static void TestItemsLeftMouseDown(
        CGump *gump, CBaseGUI *start, int currentPage, int draw2Page = 0, int count = -1);
    static void
    TestItemsLeftMouseUp(CGump *gump, CBaseGUI *start, int currentPage, int draw2Page = 0);
    static void TestItemsDragging(
        CGump *gump, CBaseGUI *start, int currentPage, int draw2Page = 0, int count = -1);
    static void
    TestItemsScrolling(CGump *gump, CBaseGUI *start, bool up, int currentPage, int draw2Page = 0);

    virtual void DelayedClick(class CRenderObject *obj) {}
    virtual void PrepareContent() {}
    virtual void UpdateContent() {}
    virtual class CTextRenderer *GetTextRenderer() { return nullptr; }
    virtual void PrepareTextures() override;
    virtual void GenerateFrame(bool stop);
    virtual bool CanBeDisplayed() { return true; }

    void FixCoordinates();
    bool CanBeMoved();
    void DrawLocker();
    bool SelectLocker();
    bool TestLockerClick();

    virtual bool EntryPointerHere();
    virtual void Draw();
    virtual class CRenderObject *Select();
    virtual void InitToolTip() {}

    GUMP_BUTTON_EVENT_H { TRACE(Client, "OnButton(%i)", serial); }
    GUMP_CHECKBOX_EVENT_H { TRACE(Client, "OnCheckbox(%i, %i)", serial, state); }
    GUMP_RADIO_EVENT_H { TRACE(Client, "OnRadio(%i, %i)", serial, state); }
    GUMP_TEXT_ENTRY_EVENT_H { TRACE(Client, "OnTextEntry(%i)", serial); }
    GUMP_SLIDER_CLICK_EVENT_H { TRACE(Client, "OnSliderClick(%i)", serial); }
    GUMP_SLIDER_MOVE_EVENT_H { TRACE(Client, "OnSliderMove(%i)", serial); }
    GUMP_RESIZE_START_EVENT_H { TRACE(Client, "OnResizeStart(%i)", serial); }
    GUMP_RESIZE_EVENT_H { TRACE(Client, "OnResize(%i)", serial); }
    GUMP_RESIZE_END_EVENT_H { TRACE(Client, "OnResizeEnd(%i)", serial); }
    GUMP_DIRECT_HTML_LINK_EVENT_H;
    GUMP_COMBOBOX_SELECTION_EVENT_H { TRACE(Client, "OnComboboxSelection(%i)", serial); }
    GUMP_SCROLL_BUTTON_EVENT_H { TRACE(Client, "OnScrollButton()"); }

    virtual void OnLeftMouseButtonDown();
    virtual void OnLeftMouseButtonUp();
    virtual bool OnLeftMouseButtonDoubleClick() { return false; }
    virtual void OnRightMouseButtonDown() {}
    virtual void OnRightMouseButtonUp() {}
    virtual bool OnRightMouseButtonDoubleClick() { return false; }
    virtual void OnMidMouseButtonDown() {}
    virtual void OnMidMouseButtonUp() {}
    virtual bool OnMidMouseButtonDoubleClick() { return false; }
    virtual void OnMidMouseButtonScroll(bool up);
    virtual void OnDragging();
    virtual void OnTextInput(const TextEvent &ev) {}
    virtual void OnKeyDown(const KeyEvent &ev) {}
    virtual void OnKeyUp(const KeyEvent &ev) {}
};

extern CGump *g_ResizedGump;
extern CGump *g_CurrentCheckGump;
