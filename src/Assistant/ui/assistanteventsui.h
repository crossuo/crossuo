// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The UI events consumer: a CAssistantEvents subclass installed into
// g_AssistantEvents that feeds the journal pane and the assistant windows.
// The default-inert methods stay default; the consumer covers the full
// notification set.

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

    // Feed the journal pane ring.
    virtual void OnJournalMessage(const CJournalMessage &message) override;

    // --- hotkey/macro consumers ----------------------------------------------
    // The hotkey action-list feeds (the skill/spell list events).
    virtual void OnSpellList(const vector<vector<astr_t>> &books) override;
    virtual void OnSkillsList(const vector<astr_t> &hotkeyActions) override;
    // The script buff mirror (BuffExists reads it through the queue).
    virtual void OnBuffAdd(uint16_t graphic, uint32_t timer, const astr_t &name) override;
    virtual void OnBuffRemove(uint16_t graphic) override;
    // The macro recording taps.
    virtual void OnRecordTargetObject(uint32_t serial, uint16_t graphic, uint16_t color) override;
    virtual void OnRecordTargetTile(uint16_t graphic, int x, int y, int z) override;
    // The Autostart script run (from EnterWorld).
    virtual void OnAutostartRequest() override;
    // The closeuo request (the host window quit path).
    virtual void OnClientCloseRequest() override;
    // The from-target replies (HandleTarget* forwards).
    virtual void OnFromTarget(
        int kind, uint32_t serial, uint16_t graphic, uint16_t color, const astr_t &name) override;

    // --- display caption ------------------------------------------------------
    // Fired from the client's send and recv sites; recompose + apply the
    // display caption.
    virtual void OnTitleRedraw() override;
    virtual void OnTitleRedraw(uint32_t serial) override;

    // --- map/inspector consumers -----------------------------------------------
    // The map window (overlay recomputation and the open-map request:
    // the ShowOnLogin option and the hotkey launcher).
    virtual void OnMapUpdate(uint32_t serial) override;
    virtual void OnMapRedraw() override;
    virtual void OnMapOpenRequest() override;
    // The tracker point (the map window draws it).
    virtual void OnTrack(bool state, int x, int y) override;
    // The object inspector sections (snapshot refresh from the mirror pokes
    // and DumpObjectInfo).
    virtual void OnInspectorUpdate(uint32_t serial) override;
    // The Krrios party/guild position packets (the map window markers).
    virtual void OnPartyMemberInfo(const XA_PARTY_MEMBER_INFO &info) override;
    virtual void OnGuildMemberInfo(const XA_GUILD_MEMBER_INFO &info) override;

    // The script BuffExists mirror feed (script/scriptstate.cpp). OnMegaCliloc
    // stays default-inert (the object-properties cache needs no UI).

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
