// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The macro executor (Phase 3, master plan §2.6/§7 item 3): the playback half
// of the original app's assist/tabmacros.cpp (the tick-driven ProcessMacro state
// machine) extracted from the widget. Semantics kept verbatim:
//   - a 10 ms frame tick drove TabMacros::ProcessMacro; here Tick() runs from
//     AssistantUI_Tick every game frame.
//     The while (m_Playing && m_NextMacroTick < g_Ticks) loop and the
//     m_PlayPosition / m_NextMacroTick semantics are unchanged: a waiting
//     action sets m_NextMacroTick = now + delay and returns, so the macro
//     simply pauses until the tick passes (no threads, no blocking).
//   - m_PlayList holds copies of the macro's actions (Play:931), looped
//     macros restart with m_NextMacroTick = now + loopDelay (ProcessMacro
//     1123-1135), loop delay 0 (checkbox off) stops.
//   - the recording buffer (on_pb_Record_clicked:337) is fed by the client's
//     send sites (CGame::AttackReq -> RecordAttack, CGame::DoubleClick /
//     PaperdollReq -> RecordUse, CPathFinder::Walk -> RecordWalk,
//     CGame::ChangeWarmode -> RecordWarMode, the help gumps ->
//     RecordHelpRequest, CGame::UseSkill / CastSpell / CastSpellFromBook ->
//     RecordUseSkill / RecordCast / RecordCastFromBook / RecordCastNew) and
//     the target system taps (assist/target.cpp:190/:210 -> CAssistantEvents)
//     while the game sends its target cursors; RecordStop replaces the
//     selected macro's action list (an empty capture is discarded).
// Agent-dependent bodies (dress/arm lists use the config dressbag and move
// item delays exactly like the original code) are included; script bodies are not
// part of the executor.

#pragma once

#include "macro.h"

#include <initializer_list>

namespace xuoassist {

class CMacroExecutor
{
public:
    // The frame tick (was a 10 ms timer -> ProcessMacro).
    void Tick();

    // TabMacros::Play(name, doNotAutoIterrupt) - the hotkey
    // entry point; unknown names are a no-op.
    void Play(const string &name, bool doNotAutoIterrupt);
    // TabMacros::Play(CMacroListItem *, ...) - the Play button entry point.
    // macroIndex indexes g_AssistantConfig.Macros().
    void PlayIndex(int macroIndex, bool doNotAutoIterrupt);
    // TabMacros::Stop.
    void Stop();

    // Loop options (the original Play read cb_Looped/dsb_LoopTime directly; the UI
    // pushes them here before Play).
    void SetLoopOptions(bool looped, float loopSeconds);

    // The selected macro name (the UI shows Play/Stop for it).
    bool IsPlaying() const { return m_Playing; }
    int PlayingIndex() const { return m_PlayingIndex; }
    bool IsRecording() const { return m_Recording; }
    int RecordingIndex() const { return m_RecordingIndex; }

    // TabMacros::on_pb_Record_clicked halves: the UI
    // calls RecordStart on the selected macro, target taps append, and
    // RecordStop commits (or discards when nothing was captured).
    void RecordStart(int macroIndex);
    // Returns the captured action count that was committed (0 = discarded).
    int RecordStop();

    // The target-system taps (was g_TabMacros->AddAction).
    void RecordTargetObject(uint serial, ushort graphic, ushort color);
    void RecordTargetTile(ushort graphic, int x, int y, int z);

    // The client's attack send site (CGame::AttackReq): the MT_ATTACK
    // observation tap (was the 0x05 AttackRequest send tap in
    // packetmanager.cpp, same conversions in the same order).
    void RecordAttack(uint serial);

    // The client's use send sites (CGame::DoubleClick, and
    // CGame::PaperdollReq with the 0x80000000 bit set): the MT_USE
    // observation tap (was the 0x06 DoubleClick send tap in
    // packetmanager.cpp, same conversions) plus the g_LastUseObject mirror
    // feed (the tap fed it outside the recording gate).
    void RecordUse(uint serial);

    // The client's walk send site (CPathFinder::Walk): the MT_WALK
    // observation tap (was the 0x02 WalkRequest send tap in
    // packetmanager.cpp) - a direction differing from the current facing
    // records as the negative -(direction | 0x80) turn encoding. Not
    // suspension-gated: the old tap saw every walk packet the client sent,
    // however the walk was started (walker autowalk included).
    void RecordWalk(int direction);

    // The client's warmode send site (CGame::ChangeWarmode): the MT_WAR_MODE
    // observation tap (was the 0x72 WarmodeS send tap in packetmanager.cpp).
    void RecordWarMode(uint8_t status);

    // The client's help gump send sites (CGumpMenubar / CGumpPaperdoll): the
    // MT_HELP_MENU_REQUEST observation tap (was the 0x9B HelpRequest send tap
    // in packetmanager.cpp).
    void RecordHelpRequest();

    // The client's skill send sites (CGame::UseSkill, and the client
    // PacketManager's UseSkillRequest message): the MT_USE_SKILL observation
    // tap (was the 0x12 Perform Action send tap, type 0x24, in
    // packetmanager.cpp, same conversions in the same order). MCT_LAST
    // compares against the previous g_LastSkillIndex BEFORE the update below
    // it; the update and the title-redraw check ran outside the recording
    // gate in the old tap and still do.
    void RecordUseSkill(int index);

    // The client's spell send sites (CGame::CastSpell / CastSpellFromBook,
    // and the client PacketManager's CastSpellRequest message) below client
    // 6.0.14.2: the MT_CAST / MT_CAST_FROM_BOOK observation taps (were the
    // 0x12 Perform Action send taps, types 0x56 / 0x27, in packetmanager.cpp,
    // same conversions in the same order). MCT_LAST compares against the
    // previous g_LastSpellIndex BEFORE the update below it; the update and
    // the title-redraw check ran outside the recording gate in the old tap
    // and still do.
    void RecordCast(int index);
    void RecordCastFromBook(int index);

    // The client's spell send sites from client 6.0.14.2 (the same
    // CGame::CastSpell / PacketManager CastSpellRequest call sites sending
    // the 0xBF extended cast instead of 0x12): the MT_CAST_NEW observation
    // tap (was the 0xBF ExtendedCommandS send tap, subcommand 0x001C, in
    // packetmanager.cpp, same conversions in the same order - the update of
    // g_LastSpellIndex happens BEFORE the MCT_LAST comparison here, so the
    // comparison semantics differ from RecordCast).
    void RecordCastNew(int index);

    // TabMacros::AddAction(macro, false) recording half:
    // appends an observed action to the open recording; ownership passes to
    // the executor (a stray call while not recording is freed). The packet
    // taps gate on IsRecording() BEFORE building the macro, so the
    // not-recording cost is a single bool check per packet.
    void Record(CMacro *macro);

    // TabMacros::PlayWaiting: when a playing macro sits
    // on a waiting action that one of `types` satisfies, rewind to it and
    // re-run immediately (the awaited event just arrived). true = the wait
    // was consumed and the CALLER consumed the packet (the macro replay
    // answered the server itself, exactly like the original app).
    bool PlayWaiting(std::initializer_list<MACRO_TYPE> types);

    // TabMacros::Reset: the client-side world teardown
    // (EnterWorld re-entry / disconnect) stops any playback and recording.
    void Reset();

    // Recording suspension: assistant-initiated game actions wrap their client
    // calls in a RecordingSuspension scope so they do not record themselves.
    // Depth counted, so scopes nest; every Record* method checks CanRecord()
    // first and no-ops while suspended (call sites stay a single RAII scope).
    void SuspendRecording() { ++m_SuspendDepth; }
    void ResumeRecording() { --m_SuspendDepth; }
    bool CanRecord() const { return m_Recording && m_SuspendDepth == 0; }

private:
    void ProcessMacro(); // was TabMacros::ProcessMacro
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
    int m_SuspendDepth{ 0 };         // RecordingSuspension scopes in flight
};

extern CMacroExecutor g_MacroExecutor;

// RAII scope that suppresses recording for its lifetime.
class RecordingSuspension
{
public:
    RecordingSuspension() { g_MacroExecutor.SuspendRecording(); }
    ~RecordingSuspension() { g_MacroExecutor.ResumeRecording(); }
    RecordingSuspension(const RecordingSuspension &) = delete;
    RecordingSuspension &operator=(const RecordingSuspension &) = delete;
};

} // namespace xuoassist

