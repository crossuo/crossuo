// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The macro executor: the playback half of the assistant macro feature - a
// tick-driven ProcessMacro state machine decoupled from any UI widget.
// Semantics:
//   - Tick() runs from AssistantUI_Tick every game frame; the
//     while (m_Playing && m_NextMacroTick < g_Ticks) loop and the
//     m_PlayPosition / m_NextMacroTick semantics mean a waiting action sets
//     m_NextMacroTick = now + delay and returns, so the macro simply pauses
//     until the tick passes (no threads, no blocking).
//   - m_PlayList holds copies of the macro's actions; looped macros restart
//     with m_NextMacroTick = now + loopDelay; loop delay 0 (checkbox off)
//     stops.
//   - the recording buffer is fed by the client's send sites and the target
//     system call sites while the game sends its target cursors; RecordStop
//     replaces the selected macro's action list (an empty capture is
//     discarded).
// Agent-dependent bodies (dress/arm lists use the config dressbag and move
// item delays) are included; script bodies are not part of the executor.

#pragma once

#include "macro.h"

#include <initializer_list>

namespace xuoassist {

class CMacroExecutor
{
public:
    CMacroExecutor() = default;

    CMacroExecutor(const CMacroExecutor &) = delete;
    CMacroExecutor &operator=(const CMacroExecutor &) = delete;

    // The frame tick; runs ProcessMacro.
    void Tick();

    // The hotkey entry point (a macro name); unknown names are a no-op.
    void Play(const astr_t &name, bool doNotAutoIterrupt);
    // The Play button entry point; macroIndex indexes g_AssistantConfig.Macros().
    void PlayIndex(int macroIndex, bool doNotAutoIterrupt);
    void Stop();

    // Loop options; the UI pushes them here before Play.
    void SetLoopOptions(bool looped, float loopSeconds);

    // The selected macro name (the UI shows Play/Stop for it).
    bool IsPlaying() const { return m_Playing; }
    int PlayingIndex() const { return m_PlayingIndex; }
    bool IsRecording() const { return m_Recording; }
    int RecordingIndex() const { return m_RecordingIndex; }

    // The record halves: the UI calls RecordStart on the selected macro,
    // the observation taps append, and
    // RecordStop commits (or discards when nothing was captured).
    void RecordStart(int macroIndex);
    // Returns the captured action count that was committed (0 = discarded).
    int RecordStop();

    // The target-system observation taps.
    void RecordTargetObject(uint32_t serial, uint16_t graphic, uint16_t color);
    void RecordTargetTile(uint16_t graphic, int x, int y, int z);

    // The client's attack send site (CGame::AttackReq): records the MT_ATTACK
    // action with the MCT_LASTATTACK / MCT_LASTTARGET / MCT_ENEMY conversions in
    // that order.
    void RecordAttack(uint32_t serial);

    // The client's use send sites (CGame::DoubleClick, and
    // CGame::PaperdollReq with the 0x80000000 bit set): records the MT_USE
    // action (graphic/color from the world object, the player as MCT_SELF) and
    // feeds the g_LastUseObject mirror outside the recording gate.
    void RecordUse(uint32_t serial);

    // The client's walk send site (CPathFinder::Walk): records the MT_WALK
    // action - a direction differing from the current facing records as the
    // negative -(direction | 0x80) turn encoding. Not suspension-gated: every
    // walk packet the client sends is observed, however the walk was started
    // (walker autowalk included).
    void RecordWalk(int direction);

    // The client's warmode send site (CGame::ChangeWarmode): observes the
    // MT_WAR_MODE action.
    void RecordWarMode(uint8_t status);

    // The client's help gump send sites (CGumpMenubar / CGumpPaperdoll): records
    // the MT_HELP_MENU_REQUEST action.
    void RecordHelpRequest();

    // The client's skill send sites (CGame::UseSkill, and the client
    // PacketManager's UseSkillRequest message): records the MT_USE_SKILL action.
    // MCT_LAST compares against the previous g_LastSkillIndex BEFORE the update
    // below it; the update and the title-redraw check run outside the recording
    // gate.
    void RecordUseSkill(int index);

    // The client's spell send sites (CGame::CastSpell / CastSpellFromBook, and
    // the client PacketManager's CastSpellRequest message) below client 6.0.14.2:
    // records the MT_CAST / MT_CAST_FROM_BOOK actions. MCT_LAST compares against
    // the previous g_LastSpellIndex BEFORE the update below it; the update and
    // the title-redraw check run outside the recording gate.
    void RecordCast(int index);
    void RecordCastFromBook(int index);

    // The client's spell send sites from client 6.0.14.2 (the same
    // CGame::CastSpell / PacketManager CastSpellRequest call sites sending the
    // 0xBF extended cast instead of 0x12): records the MT_CAST_NEW action. The
    // update of g_LastSpellIndex happens BEFORE the MCT_LAST comparison here, so
    // the comparison semantics differ from RecordCast.
    void RecordCastNew(int index);

    // The client's combat ability send site (CGumpAbility::OnAbilityUse): records
    // the MT_COMBAT_ABILITY action - the byte is compared against g_Ability[0]
    // for the MCT_PRIMARY / MCT_SECONDARY pick.
    void RecordCombatAbility(uint8_t ability);

    // The client's guild/quest menu send sites (the CGumpPaperdoll buttons, and
    // MacroManager's gump-menu macro actions): records the
    // MT_GUILD_MENU_REQUEST / MT_QUEST_MENU_REQUEST actions.
    void RecordGuildMenuRequest();
    void RecordQuestMenuRequest();

    // The client's context menu send sites (CGame::ProcessDelayedClicks, and the
    // CGumpPopupMenu selection): records the MT_REQUEST_CONTEXT_MENU /
    // MT_WAIT_FOR_CONTEXT_MENU actions (the menu id rides the graphic field).
    void RecordContextMenuRequest(uint32_t serial);
    void RecordWaitContextMenu(uint32_t serial, uint16_t menuID);

    // The client's speech send sites (the game console tail, MacroManager's
    // MC_SAY/EMOTE/WHISPER/YELL actions, CGame::SendASCIIText): records the
    // MT_SAY action. Gated on IsRecording() directly: assistant speech reaches
    // the wire through the console path (unlike the builder-bypassed actions),
    // so the recording suspension does not apply here.
    void RecordSay(const astr_t &text);

    // The client's gargoyle flying send sites (the CGumpRacialAbility button, and
    // MacroManager's macro action): records the MT_TOGGLE_GARGOYLE_FLYING action.
    void RecordToggleGargoyleFlying();

    // Appends an observed action to the open recording; ownership passes to the
    // executor (a stray call while not recording is freed). The call sites gate
    // on IsRecording() BEFORE building the macro, so the not-recording cost is a
    // single bool check per packet.
    void Record(CMacro *macro);

    // When a playing macro sits on a waiting action that one of `types`
    // satisfies, rewind to it and re-run immediately (the awaited event just
    // arrived). true = the wait was consumed and the CALLER consumed the packet
    // (the macro replay answered the server itself).
    bool PlayWaiting(std::initializer_list<MACRO_TYPE> types);

    // The client-side world teardown (EnterWorld re-entry / disconnect) stops any
    // playback and recording.
    void Reset();

    // Recording suspension: assistant-initiated game actions wrap their client
    // calls in a RecordingSuspension scope so they do not record themselves.
    // Depth counted, so scopes nest; every Record* method checks CanRecord()
    // first and no-ops while suspended (call sites stay a single RAII scope).
    void SuspendRecording() { ++m_SuspendDepth; }
    void ResumeRecording() { --m_SuspendDepth; }
    bool CanRecord() const { return m_Recording && m_SuspendDepth == 0; }

private:
    void ProcessMacro();
    uint32_t FindObject(const CMacroWithObject *macro, MACRO_CONVERT_TYPE convertType);

    bool m_Playing{ false };
    bool m_Recording{ false };
    bool m_Looped{ false };
    bool m_DoNotAutoIterrupt{ false };
    bool m_WaitingEnabled{ false };
    int m_PlayingIndex{ -1 };
    int m_RecordingIndex{ -1 };
    int m_PlayPosition{ 0 };
    uint32_t m_NextMacroTick{ 0 };
    uint32_t m_LoopDelay{ 1000 };
    vector<CMacro *> m_PlayList; // copies, freed by Play/Stop (Play:955)
    vector<uint32_t> m_ArmList;      // MT_SET_ARM / MT_ARM queue (serials)
    vector<uint32_t> m_DressList;    // MT_SET_DRESS / MT_DRESS queue (serials)
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
