// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_UI_ASSISTANTEVENTSUI_H
#define XUOASSIST_UI_ASSISTANTEVENTSUI_H

// The Phase 2 UI events consumer (plan §7 Phase 2, item 6): a
// CAssistantEvents subclass installed into g_AssistantEvents that feeds the
// journal pane. Everything else keeps the default-inert behavior; the
// consumed notifications for later rounds are listed as PORT-TODO markers.

#include "../core/assistantevents.h"

namespace xuoassist {

// The latest from-target reply (tablists polls this; single-slot since one
// target cursor can be outstanding at a time).
struct XA_FROM_TARGET_RESULT
{
    int Kind{ -1 };
    uint Serial{ 0 };
    ushort Graphic{ 0 };
    ushort Color{ 0 };
    string Name;
    uint Generation{ 0 }; // bumped per reply (poll for change)
};

// The shared from-target poll (the tablists.cpp helper made available to the
// agents/display tabs): true exactly once per reply of the given kind.
bool UI_PollFromTarget(
    bool &pending, uint &generation, int kind, XA_FROM_TARGET_RESULT &out);

class CAssistantUIEvents : public CAssistantEvents
{
public:
    struct Line
    {
        string Text;
        ushort Color{ 0 };
        uchar Flags{ 0 };
    };

    // Was g_JournalManager.Add feeding the Qt journal window.
    virtual void OnJournalMessage(const CJournalMessage &message) override;

    // --- Phase 3 consumers -------------------------------------------------
    // The hotkey action-list feeds (packetmanager.cpp:3248/3254).
    virtual void OnSpellList(const vector<vector<string>> &books) override;
    virtual void OnSkillsList(const vector<string> &hotkeyActions) override;
    // The script buff mirror (BuffExists reads it through the queue).
    virtual void OnBuffAdd(ushort graphic, uint timer, const string &name) override;
    virtual void OnBuffRemove(ushort graphic) override;
    // The macro recording taps (target.cpp:190/:210).
    virtual void OnRecordTargetObject(uint serial, ushort graphic, ushort color) override;
    virtual void OnRecordTargetTile(ushort graphic, int x, int y, int z) override;
    // The from-target replies (HandleTarget* forwards).
    virtual void OnFromTarget(
        int kind, uint serial, ushort graphic, ushort color, const string &name) override;

    // --- Phase 4a consumers -------------------------------------------------
    // The display caption redraw requests (packetmanager.cpp OnTitleRedraw
    // call sites; was g_TabDisplay->RedrawTitle/CheckRedrawTitle).
    virtual void OnTitleRedraw() override;
    virtual void OnTitleRedraw(uint serial) override;

    // The script BuffExists mirror feed (script/scriptstate.cpp; was
    // g_BuffManager.Add/Remove). The corpse/party/guild/megacliloc/inspector
    // events stay default-inert until their UI rounds land (phase 5).

    // Ring of the last N lines (oldest first); capped, event-time copies.
    const vector<Line> &Lines() const { return m_Lines; }

    static XA_FROM_TARGET_RESULT &FromTargetResult();

private:
    enum { LINE_CAP = 256 };
    vector<Line> m_Lines;
};

// The installed subclass instance (assistantui.cpp owns the install/restore).
CAssistantUIEvents *AssistantUIEvents_Get();

// The journal pane (right side of the shell): the last journal lines with
// their UO colors, empty state when not logged in. height <= 0 = all remaining.
void JournalPane_Draw(float height);

} // namespace xuoassist

#endif // XUOASSIST_UI_ASSISTANTEVENTSUI_H
