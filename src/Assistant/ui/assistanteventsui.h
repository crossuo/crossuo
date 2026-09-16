// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The Phase 2 UI events consumer (plan §7 Phase 2, item 6): a
// CAssistantEvents subclass installed into g_AssistantEvents that feeds the
// journal pane. Everything else keeps the default-inert behavior; the
// consumed notifications for later rounds arrived with their rounds; the
// consumer now covers the full notification set through Phase 6.

#include "../core/assistantevents.h"

namespace xuoassist {

// The latest from-target reply (tablists polls this; single-slot since one
// target cursor can be outstanding at a time).
struct XA_FROM_TARGET_RESULT
{
    int Kind{ -1 };
    uint32_t Serial{ 0 };
    uint16_t Graphic{ 0 };
    uint16_t Color{ 0 };
    astr_t Name;
    uint32_t Generation{ 0 }; // bumped per reply (poll for change)
};

// The shared from-target poll (the tablists.cpp helper made available to the
// agents/display tabs): true exactly once per reply of the given kind.
bool UI_PollFromTarget(
    bool &pending, uint32_t &generation, int kind, XA_FROM_TARGET_RESULT &out);

class CAssistantUIEvents : public CAssistantEvents
{
public:
    CAssistantUIEvents() = default;
    CAssistantUIEvents(const CAssistantUIEvents &) = delete;
    CAssistantUIEvents &operator=(const CAssistantUIEvents &) = delete;

    struct Line
    {
        astr_t Text;
        uint16_t Color{ 0 };
        uint8_t Flags{ 0 };
    };

    // Was g_JournalManager.Add feeding the journal window.
    virtual void OnJournalMessage(const CJournalMessage &message) override;

    // --- Phase 3 consumers -------------------------------------------------
    // The hotkey action-list feeds (the 0xFC list handlers).
    virtual void OnSpellList(const vector<vector<astr_t>> &books) override;
    virtual void OnSkillsList(const vector<astr_t> &hotkeyActions) override;
    // The script buff mirror (BuffExists reads it through the queue).
    virtual void OnBuffAdd(uint16_t graphic, uint32_t timer, const astr_t &name) override;
    virtual void OnBuffRemove(uint16_t graphic) override;
    // The macro recording taps.
    virtual void OnRecordTargetObject(uint32_t serial, uint16_t graphic, uint16_t color) override;
    virtual void OnRecordTargetTile(uint16_t graphic, int x, int y, int z) override;
    // The Autostart script run (was AssistForm::RunAutostart from EnterWorld).
    virtual void OnAutostartRequest() override;
    // The closeuo request (was the WM_CLOSE to the host window).
    virtual void OnClientCloseRequest() override;
    // The from-target replies (HandleTarget* forwards).
    virtual void OnFromTarget(
        int kind, uint32_t serial, uint16_t graphic, uint16_t color, const astr_t &name) override;

    // --- Phase 4a consumers -------------------------------------------------
    // The display caption redraw requests (fired from the client's send and
    // recv sites; was g_TabDisplay->RedrawTitle/CheckRedrawTitle).
    virtual void OnTitleRedraw() override;
    virtual void OnTitleRedraw(uint32_t serial) override;

    // --- Phase 5 consumers --------------------------------------------------
    // The map window (was g_Map->CheckForUpdate/SetWantRedraw and the
    // OpenMap pokes: OIPMT_OPEN_MAP, ShowOnLogin, the hotkey launcher).
    virtual void OnMapUpdate(uint32_t serial) override;
    virtual void OnMapRedraw() override;
    virtual void OnMapOpenRequest() override;
    // The tracker point (was the external-UOAM notify; the map draws it).
    virtual void OnTrack(bool state, int x, int y) override;
    // The object inspector sections (was g_ObjectInspectorForm->CheckUpdate
    // / Update from the mirror pokes and DumpObjectInfo).
    virtual void OnInspectorUpdate(uint32_t serial) override;
    // The Krrios party/guild position packets (the map window markers).
    virtual void OnPartyMemberInfo(const XA_PARTY_MEMBER_INFO &info) override;
    virtual void OnGuildMemberInfo(const XA_GUILD_MEMBER_INFO &info) override;

    // The script BuffExists mirror feed (script/scriptstate.cpp; was
    // g_BuffManager.Add/Remove). OnMegaCliloc stays default-inert (the
    // object-properties cache needs no UI round).

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
