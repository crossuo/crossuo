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

    // PORT-TODO(phase-3/4/5): buffs (OnBuffAdd/Remove), corpses
    // (OnCorpseAdd), party/guild member info, skills, spells, macro lists,
    // object properties (OnMegaCliloc), inspector, title/map redraws and
    // tracker updates stay default-inert until their UI rounds land.

    // Ring of the last N lines (oldest first); capped, event-time copies.
    const vector<Line> &Lines() const { return m_Lines; }

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
