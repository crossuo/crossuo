// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The macro executor (Phase 3, master plan §2.6/§7 item 3): the playback half
// of the Qt app's assist/tabmacros.cpp (the tick-driven ProcessMacro state
// machine) extracted from the widget. Semantics kept verbatim:
//   - a 10 ms QTimer drove TabMacros::ProcessMacro (tabmacros.cpp:60,
//     1105-1121); here Tick() runs from AssistantUI_Tick every game frame.
//     The while (m_Playing && m_NextMacroTick < g_Ticks) loop and the
//     m_PlayPosition / m_NextMacroTick semantics are unchanged: a waiting
//     action sets m_NextMacroTick = now + delay and returns, so the macro
//     simply pauses until the tick passes (no threads, no blocking).
//   - m_PlayList holds copies of the macro's actions (Play:931), looped
//     macros restart with m_NextMacroTick = now + loopDelay (ProcessMacro
//     1123-1135), loop delay 0 (checkbox off) stops.
//   - the recording buffer (on_pb_Record_clicked:337) is fed by the packet
//     observation taps (packetmanager.cpp WarmodeS/DoubleClick/AttackRequest/
//     PerformAction/... - the exact conversions of the Qt sites) and the
//     target system taps (assist/target.cpp:190/:210 -> CAssistantEvents)
//     while the game sends its target cursors; RecordStop replaces the
//     selected macro's action list (an empty capture is discarded).
// Agent-dependent bodies (dress/arm lists use the config dressbag and move
// item delays exactly like the Qt code) are included; script bodies are not
// part of the executor.

#pragma once

#include "macro.h"

#include <initializer_list>

namespace xuoassist {

class CMacroExecutor
{
public:
    // The frame tick (was the 10 ms QTimer -> ProcessMacro).
    void Tick();

    // TabMacros::Play(const QString &name, doNotAutoIterrupt) - the hotkey
    // entry point (hotkeysform.cpp:1458); unknown names are a no-op.
    void Play(const string &name, bool doNotAutoIterrupt);
    // TabMacros::Play(CMacroListItem *, ...) - the Play button entry point.
    // macroIndex indexes g_AssistantConfig.Macros().
    void PlayIndex(int macroIndex, bool doNotAutoIterrupt);
    // TabMacros::Stop (tabmacros.cpp:988).
    void Stop();

    // Loop options (the Qt Play read cb_Looped/dsb_LoopTime directly; the UI
    // pushes them here before Play).
    void SetLoopOptions(bool looped, float loopSeconds);

    // The selected macro name (the UI shows Play/Stop for it, tabmacros.cpp:252).
    bool IsPlaying() const { return m_Playing; }
    int PlayingIndex() const { return m_PlayingIndex; }
    bool IsRecording() const { return m_Recording; }
    int RecordingIndex() const { return m_RecordingIndex; }

    // TabMacros::on_pb_Record_clicked halves (tabmacros.cpp:337): the UI
    // calls RecordStart on the selected macro, target taps append, and
    // RecordStop commits (or discards when nothing was captured).
    void RecordStart(int macroIndex);
    // Returns the captured action count that was committed (0 = discarded).
    int RecordStop();

    // The target-system taps (was g_TabMacros->AddAction from
    // assist/target.cpp:190/:210).
    void RecordTargetObject(uint serial, ushort graphic, ushort color);
    void RecordTargetTile(ushort graphic, int x, int y, int z);

    // TabMacros::AddAction(macro, false) recording half (tabmacros.cpp:217):
    // appends an observed action to the open recording; ownership passes to
    // the executor (a stray call while not recording is freed). The packet
    // taps gate on IsRecording() BEFORE building the macro, so the
    // not-recording cost is a single bool check per packet.
    void Record(CMacro *macro);

    // TabMacros::PlayWaiting (tabmacros.cpp:1023): when a playing macro sits
    // on a waiting action that one of `types` satisfies, rewind to it and
    // re-run immediately (the awaited event just arrived). true = the wait
    // was consumed and the CALLER consumed the packet (the macro replay
    // answered the server itself, exactly like the Qt app).
    bool PlayWaiting(std::initializer_list<MACRO_TYPE> types);

    // TabMacros::Reset (tabmacros.cpp:366): the client-side world teardown
    // (EnterWorld re-entry / disconnect) stops any playback and recording.
    void Reset();

private:
    void ProcessMacro(); // was TabMacros::ProcessMacro (tabmacros.cpp:1105)
    uint FindObject(CMacroWithObject *macro, MACRO_CONVERT_TYPE convertType);

    bool m_Playing{ false };
    bool m_Recording{ false };
    bool m_Looped{ false };
    bool m_DoNotAutoIterrupt{ false };
    bool m_WaitingEnabled{ false };
    int m_PlayingIndex{ -1 };
    int m_RecordingIndex{ -1 };
    int m_PlayPosition{ 0 };
    uint m_NextMacroTick{ 0 };
    uint m_LoopDelay{ 1000 };
    vector<CMacro *> m_PlayList; // copies, freed by Play/Stop (Play:955)
    vector<uint> m_ArmList;      // MT_SET_ARM / MT_ARM queue (serials)
    vector<uint> m_DressList;    // MT_SET_DRESS / MT_DRESS queue (serials)
    vector<CMacro *> m_RecordBuffer; // the open recording
};

extern CMacroExecutor g_MacroExecutor;

} // namespace xuoassist

