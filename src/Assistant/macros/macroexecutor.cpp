// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The macro executor implementation (see macroexecutor.h). Every action body
// is the TabMacros::ProcessMacro case ported 1:1 (tabmacros.cpp:1105-1828);
// the original code's widget reads became config reads (g_TabMain distances ->
// g_AssistantConfig, g_TabAgentsDress flags -> g_AssistantConfig).

#define LOGGER_MODULE Assistant

#include "macroexecutor.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/assistantevents.h" // g_AssistantEvents->OnTitleRedraw (the old send taps fired it)
#include "../core/global.h"
#include "../core/packets.h"
#include "../core/target.h"
#include "../managers/contextmenumanager.h"
#include "../managers/gumpmanager.h"
#include "../managers/menumanager.h"
#include "../managers/promptmanager.h"
#include "../managers/skillmanager.h"
#include "../managers/spellmanager.h"
#include "../model/commonitems/gumphook.h"
#include "../core/world.h"
#include "../core/worldutil.h"
#include "Walker/PathFinder.h" // g_PathFinder (the client walker; was the plugin's PathFinder)
#include "CrossUO.h" // ::g_Game (the client send bridge)
#include "Network/Packets.h" // ::CPacketHelpRequest (the client's own 0x9B builder)

namespace xuoassist {

CMacroExecutor g_MacroExecutor;

namespace {

void free_macro_list(vector<CMacro *> &list)
{
    for (CMacro *macro : list)
    {
        delete macro;
    }
    list.clear();
}

void warmode_send(uint state)
{
    // was CCommandManager::OnCommandWarmode
    if (g_World == nullptr || g_Player == nullptr)
    {
        return;
    }
    uchar warmode = (uchar)!g_Player->Warmode;
    if (state < 2)
    {
        warmode = (uchar)state;
    }
    // assistant-initiated warmode changes must not record themselves; the
    // client's CGame::ChangeWarmode is the recording tap
    RecordingSuspension suspension;

    g_Game.ChangeWarmode(warmode);
}

uint serial_for_convert(CMacroWithObject *macro, MACRO_CONVERT_TYPE convertType)
{
    switch (convertType)
    {
        case MCT_SELF:
            return g_PlayerSerial;
        case MCT_LASTATTACK:
            return CAssistant::TextToSerial("lastattack");
        case MCT_LASTTARGET:
            return CAssistant::TextToSerial("lasttarget");
        case MCT_ENEMY:
            return CAssistant::TextToSerial("enemy");
        case MCT_FRIEND:
            return CAssistant::TextToSerial("friend");
        default:
            return macro != nullptr ? macro->Serial : 0;
    }
}

} // namespace

void CMacroExecutor::SetLoopOptions(bool looped, float loopSeconds)
{
    m_Looped = looped;
    m_LoopDelay = (uint)(loopSeconds * 1000.0f);
}

void CMacroExecutor::Play(const string &name, bool doNotAutoIterrupt)
{
    // TabMacros::Play: find by name, last first
    const vector<CMacroEntry> &macros = g_AssistantConfig.Macros();
    if (m_Playing && m_DoNotAutoIterrupt)
    {
        return;
    }
    for (int i = (int)macros.size() - 1; i >= 0; i--)
    {
        if (macros[i].Name == name)
        {
            PlayIndex(i, doNotAutoIterrupt);
            break;
        }
    }
}

void CMacroExecutor::PlayIndex(int macroIndex, bool doNotAutoIterrupt)
{
    // TabMacros::Play(CMacroListItem *)
    const vector<CMacroEntry> &macros = g_AssistantConfig.Macros();
    if (macroIndex < 0 || macroIndex >= (int)macros.size())
    {
        return; // a macro deleted between selection and play: no-op, no crash
    }
    const CMacroEntry &entry = macros[macroIndex];

    m_DoNotAutoIterrupt = doNotAutoIterrupt;
    m_PlayingIndex = macroIndex;
    m_NextMacroTick = 0;
    m_ArmList.clear();
    m_DressList.clear();
    m_WaitingEnabled = false;

    free_macro_list(m_PlayList);
    m_PlayPosition = 0;

    for (CMacro *macro : entry.m_Items)
    {
        m_PlayList.push_back(macro->Copy());
    }

    if (m_PlayList.empty())
    {
        m_Playing = false; // empty macro: play is a no-op
        return;
    }

    m_Playing = true;
    ProcessMacro();
}

void CMacroExecutor::Stop()
{
    // TabMacros::Stop
    m_Playing = false;
    m_PlayingIndex = -1;
    m_WaitingEnabled = false;
    m_NextMacroTick = 0;
    m_ArmList.clear();
    m_DressList.clear();
    free_macro_list(m_PlayList);
}

void CMacroExecutor::Tick()
{
    // was the 10 ms timer driving ProcessMacro; a headless frame tick keeps
    // playback alive while the assistant window is hidden (parity with the original)
    if (m_Playing)
    {
        ProcessMacro();
    }
}

void CMacroExecutor::RecordStart(int macroIndex)
{
    const vector<CMacroEntry> &macros = g_AssistantConfig.Macros();
    if (m_Playing || macroIndex < 0 || macroIndex >= (int)macros.size())
    {
        return;
    }
    m_Recording = true;
    m_RecordingIndex = macroIndex;
    free_macro_list(m_RecordBuffer);
}

int CMacroExecutor::RecordStop()
{
    // on_pb_Record_clicked stop half: commit the buffer
    // into the macro; an empty capture is discarded
    m_Recording = false;
    const int committed = (int)m_RecordBuffer.size();
    if (m_RecordingIndex < 0 || m_RecordingIndex >= (int)g_AssistantConfig.Macros().size())
    {
        free_macro_list(m_RecordBuffer);
        m_RecordingIndex = -1;
        return 0;
    }
    if (committed == 0)
    {
        m_RecordingIndex = -1;
        return 0; // discard
    }
    CMacroEntry &entry = g_AssistantConfig.m_Macros[m_RecordingIndex];
    free_macro_list(entry.m_Items);
    entry.m_Items.swap(m_RecordBuffer);
    m_RecordingIndex = -1;
    return committed;
}

void CMacroExecutor::RecordTargetObject(uint serial, ushort graphic, ushort color)
{
    if (!m_Recording)
    {
        return;
    }
    CMacroWithObject *macro = new CMacroWithObject(
        MT_WAIT_FOR_TARGET_OBJECT, serial, graphic, color);
    if (serial == g_PlayerSerial)
    {
        macro->ConvertType = MCT_SELF;
    }
    else if (serial == g_LastAttackObject)
    {
        macro->ConvertType = MCT_LASTATTACK;
    }
    else if (serial == g_LastTargetObject)
    {
        macro->ConvertType = MCT_LASTTARGET;
    }
    else if (serial == CAssistant::TextToSerial("enemy"))
    {
        macro->ConvertType = MCT_ENEMY;
    }
    else if (serial == CAssistant::TextToSerial("friend"))
    {
        macro->ConvertType = MCT_FRIEND;
    }
    m_RecordBuffer.push_back(macro);
}

void CMacroExecutor::RecordTargetTile(ushort graphic, int x, int y, int z)
{
    // relative coordinates against the player
    if (!m_Recording)
    {
        return;
    }
    const int px = g_Player != nullptr ? g_Player->GetX() : 0;
    const int py = g_Player != nullptr ? g_Player->GetY() : 0;
    const int pz = g_Player != nullptr ? g_Player->GetZ() : 0;
    m_RecordBuffer.push_back(
        new CMacroWithTargetTile(MT_WAIT_FOR_TARGET_TILE, graphic, x, y, z, x - px, y - py, z - pz));
}

void CMacroExecutor::Record(CMacro *macro)
{
    // TabMacros::AddAction(macro, false) recording half (the
    // SetSelectedItem path): append to the open recording. The packet taps
    // check IsRecording() before building the action; this only guards the
    // ownership (a stray action is dropped, not leaked).
    if (!m_Recording || macro == nullptr)
    {
        delete macro;
        return;
    }
    m_RecordBuffer.push_back(macro);
}

void CMacroExecutor::RecordAttack(uint serial)
{
    // was the 0x05 AttackRequest send tap (packetmanager.cpp): the original's
    // MCT_LASTATTACK / MCT_LASTTARGET / MCT_ENEMY conversions, in that order.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    CGameCharacter *obj = g_World->FindWorldCharacter(serial);

    CMacro *macro = nullptr;

    if (obj != nullptr)
    {
        macro = new CMacroWithObject(MT_ATTACK, serial, obj->Graphic, obj->Color);
    }
    else
    {
        macro = new CMacroWithObject(MT_ATTACK, serial, 0, 0);
    }

    if (serial == g_LastAttackObject)
    {
        macro->ConvertType = MCT_LASTATTACK;
    }
    else if (serial == g_LastTargetObject)
    {
        macro->ConvertType = MCT_LASTTARGET;
    }
    else if (serial == CAssistant::TextToSerial("enemy"))
    {
        macro->ConvertType = MCT_ENEMY;
    }

    Record(macro);
}

void CMacroExecutor::RecordUse(uint serial)
{
    // was the 0x06 DoubleClick send tap (packetmanager.cpp): the graphic/color
    // come from the world object when known; the player records as MCT_SELF.
    // The g_LastUseObject mirror was fed outside the recording gate.
    g_LastUseObject = serial;

    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    CGameObject *obj = g_World->FindWorldObject(serial);

    CMacro *macro = nullptr;

    if (obj != nullptr)
    {
        macro = new CMacroWithObject(MT_USE, serial, obj->Graphic, obj->Color);
    }
    else
    {
        macro = new CMacroWithObject(MT_USE, serial, 0, 0);
    }

    if (serial == g_PlayerSerial)
    {
        macro->ConvertType = MCT_SELF;
    }

    Record(macro);
}

void CMacroExecutor::RecordWalk(int direction)
{
    // was the 0x02 WalkRequest send tap (packetmanager.cpp): a turn (the run
    // bit set on a direction that differs from the current facing) records as
    // a negative value - the MT_WALK turn encoding. Not suspension-gated: the
    // old tap saw walker packets regardless of what started the walk.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    if ((direction & 7) != (g_Player->Direction & 7))
    {
        direction = -(direction | 0x80);
    }

    Record(new CMacroWithIntValue(MT_WALK, direction));
}

void CMacroExecutor::RecordWarMode(uint8_t status)
{
    // was the 0x72 WarmodeS send tap (packetmanager.cpp): observe-only - the
    // macro action is noted from the outgoing packet, which still flows to
    // the server.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacroWithIntValue(MT_WAR_MODE, status));
}

void CMacroExecutor::RecordHelpRequest()
{
    // was the 0x9B HelpRequest send tap (packetmanager.cpp).
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacro(MT_HELP_MENU_REQUEST));
}

void CMacroExecutor::RecordUseSkill(int index)
{
    // was the 0x12 Perform Action send tap (packetmanager.cpp), type 0x24:
    // the MCT_LAST conversion compares against the previous index (the
    // assignment below), and the skill branch shares the spell redraw check.
    bool lastSpell = g_LastSpellIndex != 0;

    if (g_World != nullptr && CanRecord())
    {
        CMacro *macro = new CMacroWithIntValue(MT_USE_SKILL, index);

        if (index == g_LastSkillIndex)
        {
            macro->ConvertType = MCT_LAST;
        }

        Record(macro);
    }

    g_LastSkillIndex = index;

    if (static_cast<int>(lastSpell) != g_LastSpellIndex)
    {
        g_AssistantEvents->OnTitleRedraw();
    }
}

void CMacroExecutor::RecordCast(int index)
{
    // was the 0x12 Perform Action send tap (packetmanager.cpp), type 0x56:
    // the MCT_LAST conversion compares against the previous index (the
    // assignment below).
    bool lastSpell = g_LastSpellIndex != 0;

    if (g_World != nullptr && CanRecord())
    {
        CMacro *macro = new CMacroWithIntValue(MT_CAST, index);

        if (index == g_LastSpellIndex)
        {
            macro->ConvertType = MCT_LAST;
        }

        Record(macro);
    }

    g_LastSpellIndex = index;

    if (static_cast<int>(lastSpell) != g_LastSpellIndex)
    {
        g_AssistantEvents->OnTitleRedraw();
    }
}

void CMacroExecutor::RecordCastFromBook(int index)
{
    // was the 0x12 Perform Action send tap (packetmanager.cpp), type 0x27:
    // the MCT_LAST conversion compares against the previous index (the
    // assignment below).
    bool lastSpell = g_LastSpellIndex != 0;

    if (g_World != nullptr && CanRecord())
    {
        CMacro *macro = new CMacroWithIntValue(MT_CAST_FROM_BOOK, index);

        if (index == g_LastSpellIndex)
        {
            macro->ConvertType = MCT_LAST;
        }

        Record(macro);
    }

    g_LastSpellIndex = index;

    if (static_cast<int>(lastSpell) != g_LastSpellIndex)
    {
        g_AssistantEvents->OnTitleRedraw();
    }
}

void CMacroExecutor::RecordCastNew(int index)
{
    // was the 0xBF ExtendedCommandS send tap (packetmanager.cpp), subcommand
    // 0x001C: the index is stored BEFORE the MCT_LAST comparison, which
    // compares the new index against the previous index's "was set" flag.
    bool lastSpell = g_LastSpellIndex != 0;

    g_LastSpellIndex = index;

    if (static_cast<int>(lastSpell) != g_LastSpellIndex)
    {
        g_AssistantEvents->OnTitleRedraw();
    }

    if (g_World != nullptr && CanRecord())
    {
        CMacro *macro = new CMacroWithIntValue(MT_CAST_NEW, index);

        if (static_cast<int>(lastSpell) == g_LastSpellIndex)
        {
            macro->ConvertType = MCT_LAST;
        }

        Record(macro);
    }
}

void CMacroExecutor::RecordCombatAbility(uint8_t ability)
{
    // was the 0xD7 AOSCommandsS send tap (packetmanager.cpp), subcommand
    // 0x0019: the ability byte is compared against g_Ability[0] for the
    // primary/secondary conversion.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    CMacro *macro = new CMacro(MT_COMBAT_ABILITY);

    if (g_Ability[0] == ability)
    {
        macro->ConvertType = MCT_PRIMARY;
    }
    else
    {
        macro->ConvertType = MCT_SECONDARY;
    }

    Record(macro);
}

void CMacroExecutor::RecordGuildMenuRequest()
{
    // was the 0xD7 AOSCommandsS send tap (packetmanager.cpp), subcommand 0x0028.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacro(MT_GUILD_MENU_REQUEST));
}

void CMacroExecutor::RecordQuestMenuRequest()
{
    // was the 0xD7 AOSCommandsS send tap (packetmanager.cpp), subcommand 0x0032.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacro(MT_QUEST_MENU_REQUEST));
}

void CMacroExecutor::RecordContextMenuRequest(uint serial)
{
    // was the 0xBF ExtendedCommandS send tap (packetmanager.cpp), subcommand
    // 0x0013.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacroWithObject(MT_REQUEST_CONTEXT_MENU, serial, 0, 0));
}

void CMacroExecutor::RecordWaitContextMenu(uint serial, ushort menuID)
{
    // was the 0xBF ExtendedCommandS send tap (packetmanager.cpp), subcommand
    // 0x0015: the menu id rides the graphic field, like the original
    // CMacroWithObject.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacroWithObject(MT_WAIT_FOR_CONTEXT_MENU, serial, menuID, 0));
}

void CMacroExecutor::RecordSay(const string &text)
{
    // were the 0x03 ClientTalkS / 0xAD UnicodeClientTalk send taps
    // (packetmanager.cpp). Gate is the old handler's verbatim
    // (g_World != nullptr && IsRecording()) - see the header note.
    if (g_World == nullptr || !IsRecording())
    {
        return;
    }

    Record(new CMacroWithStringValue(MT_SAY, text));
}

void CMacroExecutor::RecordToggleGargoyleFlying()
{
    // was the 0xBF ExtendedCommandS send tap (packetmanager.cpp), subcommand
    // 0x0032.
    if (!CanRecord() || g_World == nullptr)
    {
        return;
    }

    Record(new CMacro(MT_TOGGLE_GARGOYLE_FLYING));
}

bool CMacroExecutor::PlayWaiting(std::initializer_list<MACRO_TYPE> types)
{
    // TabMacros::PlayWaiting: step back onto the waiting
    // action and re-run it now that the awaited event arrived.
    if (m_Playing && m_PlayPosition >= 1 && m_PlayPosition <= (int)m_PlayList.size())
    {
        CMacro *macro = m_PlayList[m_PlayPosition - 1];

        if (macro != nullptr)
        {
            const MACRO_TYPE type = macro->Type;

            for (MACRO_TYPE check : types)
            {
                if (type == check)
                {
                    m_PlayPosition--;
                    m_NextMacroTick = 0;
                    ProcessMacro();

                    return true;
                }
            }
        }
    }

    return false;
}

void CMacroExecutor::Reset()
{
    // TabMacros::Reset - the UI half became state reads
    // (TabMacros_Draw), so this is the state teardown: an open recording is
    // discarded and playback stops (world teardown in EnterWorld).
    m_Recording = false;
    free_macro_list(m_RecordBuffer);
    m_RecordingIndex = -1;
    m_Playing = false;
    m_PlayingIndex = -1;
    m_Looped = false;
    m_LoopDelay = 1000;
    m_WaitingEnabled = false;
    m_NextMacroTick = 0;
    m_ArmList.clear();
    m_DressList.clear();
    free_macro_list(m_PlayList);
    m_PlayPosition = 0;
}

uint CMacroExecutor::FindObject(CMacroWithObject *macro, MACRO_CONVERT_TYPE convertType)
{
    // TabMacros::FindObject
    ushort graphic = macro->Graphic;

    if (graphic == 0u)
    {
        return macro->Serial;
    }

    const ushort color =
        ((convertType == MCT_BY_TYPE_COLOR || convertType == MCT_BY_TYPE_COLOR_GROUND) ?
             macro->Color :
             0xFFFF);

    vector<CIgnoreItem> ignoreList;
    vector<string> list;

    if (convertType == MCT_BY_TYPE_GROUND || convertType == MCT_BY_TYPE_COLOR_GROUND)
    {
        int distance = g_AssistantConfig.SearchItemsDistance();
        if (macro->Type == MT_USE)
        {
            distance = g_AssistantConfig.UseItemsDistance();
        }
        g_Assistant.FindType(
            CFindItem(CAssistant::GraphicToText(graphic), CAssistant::GraphicToText(color)),
            ignoreList,
            0xFFFFFFFF,
            distance,
            0,
            FTF_NEAREST,
            false,
            list);
    }
    else if (g_Player != nullptr)
    {
        FindType(
            g_Player,
            CFindItem(CAssistant::GraphicToText(graphic), CAssistant::GraphicToText(color)),
            ignoreList,
            FTF_FAST,
            true,
            list);
    }

    if (!list.empty())
    {
        return CAssistant::TextToSerial(list.front());
    }
    return 0;
}

void CMacroExecutor::ProcessMacro()
{
    // TabMacros::ProcessMacro
    const int dressLayersCount = 18;
    const int dressLayers[dressLayersCount] = { OL_TORSO,    OL_HELMET,   OL_GLOVES,   OL_LEGS,
                                                OL_WAIST,    OL_ARMS,     OL_TUNIC,    OL_CLOAK,
                                                OL_ROBE,     OL_SKIRT,    OL_SHIRT,    OL_SHOES,
                                                OL_PANTS,    OL_BRACELET, OL_EARRINGS, OL_RING,
                                                OL_NECKLACE, OL_TALISMAN };

    while (m_Playing && m_NextMacroTick < g_Ticks)
    {
        const int playListSize = (int)m_PlayList.size();

        if (m_PlayPosition >= playListSize)
        {
            m_PlayPosition = 0;
            m_NextMacroTick = g_Ticks + m_LoopDelay;
            if (!m_Looped)
            {
                Stop();
            }
            break;
        }

        CMacro *macro = m_PlayList[m_PlayPosition];
        m_PlayPosition++;

        if (macro == nullptr || g_Player == nullptr)
        {
            return;
        }

        const MACRO_TYPE type = macro->Type;
        const MACRO_CONVERT_TYPE convertType = macro->ConvertType;

        switch (type)
        {
            case MT_USE:
            {
                uint serial = ((CMacroWithObject *)macro)->Serial;
                if (convertType == MCT_BY_TYPE || convertType == MCT_BY_TYPE_COLOR ||
                    convertType == MCT_BY_TYPE_GROUND || convertType == MCT_BY_TYPE_COLOR_GROUND)
                {
                    serial = FindObject((CMacroWithObject *)macro, convertType);
                }
                else if (convertType == MCT_LAST)
                {
                    serial = CAssistant::TextToSerial("lastobject");
                }
                else if (convertType == MCT_SELF)
                {
                    serial = g_PlayerSerial;
                }
                if (serial != 0u)
                {
                    g_Assistant.DoubleClick(serial);
                }
                break;
            }
            case MT_ATTACK:
            {
                // convertType LAST and LASTATTACK both mean "lastattack" here
                uint serial = ((CMacroWithObject *)macro)->Serial;
                if (convertType == MCT_LAST || convertType == MCT_LASTATTACK)
                {
                    serial = CAssistant::TextToSerial("lastattack");
                }
                else if (convertType == MCT_LASTTARGET)
                {
                    serial = CAssistant::TextToSerial("lasttarget");
                }
                else if (convertType == MCT_ENEMY)
                {
                    serial = CAssistant::TextToSerial("enemy");
                }
                if (serial != 0u)
                {
                    g_Assistant.Attack(serial);
                }
                break;
            }
            case MT_WAIT_FOR_TARGET_OBJECT:
            {
                if (!g_Target.IsTargeting())
                {
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    return;
                }
                uint serial = ((CMacroWithObject *)macro)->Serial;
                if (convertType == MCT_BY_TYPE || convertType == MCT_BY_TYPE_COLOR ||
                    convertType == MCT_BY_TYPE_GROUND || convertType == MCT_BY_TYPE_COLOR_GROUND)
                {
                    serial = FindObject((CMacroWithObject *)macro, convertType);
                }
                else
                {
                    serial = serial_for_convert((CMacroWithObject *)macro, convertType);
                }
                if (serial != 0u)
                {
                    g_Target.SendTargetObject(serial);
                    CPacketCancelTarget().SendClient();
                }
                else
                {
                    g_Target.SendCancelTarget();
                }
                break;
            }
            case MT_WAIT_FOR_TARGET_TILE:
            {
                if (!g_Target.IsTargeting())
                {
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    return;
                }
                if (convertType == MCT_LAST)
                {
                    g_Target.SendTargetLastTile();
                    CPacketCancelTarget().SendClient();
                }
                else
                {
                    CMacroWithTargetTile *withTarget = (CMacroWithTargetTile *)macro;
                    ushort graphic = withTarget->Graphic;
                    int x = withTarget->X;
                    int y = withTarget->Y;
                    int z = withTarget->Z;
                    if (convertType == MCT_BY_TARGET_RELATIVE)
                    {
                        x = g_Player->GetX() + withTarget->RelativeX;
                        y = g_Player->GetY() + withTarget->RelativeY;
                        z = g_Player->GetZ() + withTarget->RelativeZ;
                        // Was g_FileManager.GetTile(...) graphic resolution;
                        // the numeric-graphic fallback matches the original's
                        // empty-tile-list path (see core/target.cpp CheckHook)
                    }
                    g_Target.SendTargetTile(graphic, x, y, z);
                    CPacketCancelTarget().SendClient();
                }
                break;
            }
            case MT_USE_SKILL:
            {
                if (convertType == MCT_LAST)
                {
                    g_SkillManager.Use("last");
                }
                else
                {
                    char value[16] = { 0 };
                    snprintf(value, sizeof(value), "%i", ((CMacroWithIntValue *)macro)->Value);
                    g_SkillManager.Use(value);
                }
                break;
            }
            case MT_CAST:
            case MT_CAST_NEW:
            case MT_CAST_FROM_BOOK:
            {
                if (convertType == MCT_LAST)
                {
                    g_SpellManager.Cast("last");
                }
                else
                {
                    char value[16] = { 0 };
                    snprintf(value, sizeof(value), "%i", ((CMacroWithIntValue *)macro)->Value);
                    g_SpellManager.Cast(value);
                }
                break;
            }
            case MT_SAY:
            {
                string text = ((CMacroWithStringValue *)macro)->Value;
                if (convertType == MCT_SAY_YELL)
                {
                    text = "! " + text;
                }
                else if (convertType == MCT_SAY_WHISPER)
                {
                    text = "; " + text;
                }
                else if (convertType == MCT_SAY_EMOTE)
                {
                    text = ": " + text;
                }
                else if (convertType == MCT_SAY_BROADCAST)
                {
                    text = "? " + text;
                }
                else if (convertType == MCT_SAY_PARTY)
                {
                    text = "/ " + text;
                }
                else if (convertType == MCT_SAY_GUILD)
                {
                    text = "\\ " + text;
                }
                else if (convertType == MCT_SAY_ALLIANCE)
                {
                    text = "| " + text;
                }
                // was g_CommandManager.OnCommandSay
                if (g_World != nullptr && !text.empty())
                {
                    ushort color = 0;
                    if (!g_AssistantConfig.GetFontColor(color))
                    {
                        color = 0;
                    }
                    // the command text is ASCII speech: widen like the old toStdWString
                    wstring wide;
                    wide.reserve(text.size());
                    for (char c : text)
                    {
                        wide.push_back((wchar_t)(uchar)c);
                    }
                    CPacketUnicodeSpeechRequest(color, wide).SendClient();
                }
                break;
            }
            case MT_WAIT_FOR_PROMPT:
            {
                if (!EnabledCommandPrompts())
                {
                    break;
                }
                if (!m_WaitingEnabled)
                {
                    g_PromptReceived = false;
                    m_WaitingEnabled = true;
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    return;
                }
                m_WaitingEnabled = false;
                if (g_PromptReceived)
                {
                    g_PromptManager.Send(((CMacroWithStringValue *)macro)->Value);
                }
                break;
            }
            case MT_WAIT_FOR_MENU:
            {
                if (!m_WaitingEnabled)
                {
                    g_MenuReceived = false;
                    m_WaitingEnabled = true;
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    return;
                }
                m_WaitingEnabled = false;
                if (!g_MenuReceived)
                {
                    break;
                }
                CMenu *menu = g_MenuManager.GetLastMenu();
                if (menu == nullptr || menu->Replayed)
                {
                    break;
                }
                if (convertType == MCT_ANY ||
                    (((CMacroWithMenu *)macro)->Name == menu->Name))
                {
                    if (g_MenuManager.SendMenuChoice(
                            menu, ((CMacroWithMenu *)macro)->ItemName, true))
                    {
                        break;
                    }
                }
                break;
            }
            case MT_WAIT_FOR_GUMP:
            {
                if (!m_WaitingEnabled)
                {
                    g_GumpReceived = false;
                    m_WaitingEnabled = true;
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    return;
                }
                m_WaitingEnabled = false;
                if (!g_GumpReceived)
                {
                    break;
                }
                CGump *gump = g_GumpManager.GetLastGump();
                if (gump == nullptr || gump->Replayed)
                {
                    break;
                }
                if (convertType == MCT_ANY || (((CMacroWithGump *)macro)->ID == gump->ID))
                {
                    CGumpHook hook;
                    hook.Index = ((CMacroWithGump *)macro)->Button;
                    if (g_GumpManager.SendGumpChoice(gump, hook))
                    {
                        break;
                    }
                }
                break;
            }
            case MT_REQUEST_CONTEXT_MENU:
            {
                if (!EnabledCommandContextMenu())
                {
                    break;
                }
                uint serial = serial_for_convert((CMacroWithObject *)macro, convertType);
                if (serial != 0)
                {
                    ::CPacketRequestPopupMenu(serial).Send();
                }
                break;
            }
            case MT_WAIT_FOR_CONTEXT_MENU:
            {
                if (!EnabledCommandContextMenu())
                {
                    break;
                }
                if (!m_WaitingEnabled)
                {
                    g_ContextMenuReceived = false;
                    m_WaitingEnabled = true;
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    return;
                }
                m_WaitingEnabled = false;
                if (!g_ContextMenuReceived)
                {
                    break;
                }
                if (convertType == MCT_ANY ||
                    (((CMacroWithObject *)macro)->Serial == g_ContextMenuManager.Serial))
                {
                    ::CPacketPopupMenuSelection(
                        g_ContextMenuManager.Serial,
                        ((CMacroWithObject *)macro)->Graphic)
                        .Send();
                }
                break;
            }
            case MT_WAR_MODE:
            {
                if (convertType == MCT_TOGGLE)
                {
                    warmode_send(2);
                }
                else
                {
                    warmode_send((uint)((CMacroWithIntValue *)macro)->Value);
                }
                break;
            }
            case MT_HELP_MENU_REQUEST:
            {
                // assistant-initiated help requests must not record
                // themselves; the client's help gump buttons are the
                // recording taps
                RecordingSuspension suspension;

                ::CPacketHelpRequest().Send();
                break;
            }
            case MT_QUEST_MENU_REQUEST:
                ::CPacketQuestMenuRequest().Send();
                break;
            case MT_GUILD_MENU_REQUEST:
                ::CPacketGuildMenuRequest().Send();
                break;
            case MT_WALK:
            {
                // TabMacros::ProcessMacro MT_WALK: the
                // step went through CCommandManager::OnCommandStep ->
                // PathFinder::Walk(run, direction); in-process the client's
                // own walker (src/Walker, g_PathFinder) is driven directly.
                if (!EnabledCommandMoving())
                {
                    break;
                }

                int dir = ((CMacroWithIntValue *)macro)->Value;
                int walkDelay = 0;

                if (dir < 0) //Turn
                {
                    dir = (-dir) & 7;

                    if ((g_Player->Direction & 7) != dir)
                    {
                        g_PathFinder.Walk(false, (uint8_t)dir);
                        walkDelay = (g_AssistantConfig.FastRotation() ? 50 : 100);
                    }
                }
                else
                {
                    g_PathFinder.Walk((dir & 0x80) != 0, (uint8_t)(dir & 7));

                    if (g_Player->FindLayer(OL_MOUNT) != nullptr)
                    {
                        walkDelay = ((dir & 0x80) != 0 ? 100 : 200);
                    }
                    else
                    {
                        walkDelay = ((dir & 0x80) != 0 ? 200 : 400);
                    }
                }

                if (walkDelay != 0)
                {
                    // was g_Timer.elapsed() + walkDelay + 15 (the executor
                    // ticks on g_Ticks here)
                    m_NextMacroTick = g_Ticks + walkDelay + 15;
                    return;
                }

                break;
            }
            case MT_WRESTLING_STUN:
                CPacketWrestlingStun().SendServer();
                break;
            case MT_WRESTLING_DISARM:
                CPacketWrestlingDisarm().SendServer();
                break;
            case MT_COMBAT_ABILITY:
                CPacketUseAbilityRequest((int)(convertType == MCT_SECONDARY)).SendClient();
                break;
            case MT_TOGGLE_GARGOYLE_FLYING:
                ::CPacketToggleGargoyleFlying().Send();
                break;
            case MT_SET_ARM:
                m_ArmList.clear();
                for (int i = 1; i < 3; i++)
                {
                    CGameItem *obj = g_Player->FindLayer(i);
                    if (obj != nullptr)
                    {
                        m_ArmList.push_back(obj->Serial);
                    }
                }
                break;
            case MT_DISARM:
            {
                for (int i = 1; i < 3; i++)
                {
                    CGameItem *obj = g_Player->FindLayer(i);
                    if (obj != nullptr)
                    {
                        g_MoveItemData.Reset();
                        g_MoveItemData.Container = CAssistant::TextToSerial("dressbag");
                        if (g_MoveItemData.Container == 0u)
                        {
                            g_MoveItemData.Container = CAssistant::TextToSerial("backpack");
                        }
                        g_Assistant.MoveItem(obj);
                        m_NextMacroTick = g_Ticks + macro->WaitingTime;
                        m_PlayPosition--;
                        return;
                    }
                }
                break;
            }
            case MT_ARM:
            {
                while (!m_ArmList.empty())
                {
                    uint serial = m_ArmList.front();
                    CGameItem *obj = g_World != nullptr ? g_World->FindWorldItem(serial) : nullptr;
                    if (obj == nullptr || obj->Layer == EquipLayer(obj))
                    {
                        m_ArmList.erase(m_ArmList.begin());
                        continue;
                    }
                    CGameItem *equipped = g_Player->FindLayer(EquipLayer(obj));
                    if (equipped != nullptr && g_AssistantConfig.UndressBeforeDressing())
                    {
                        g_MoveItemData.Reset();
                        g_MoveItemData.Container = CAssistant::TextToSerial("dressbag");
                        if (g_MoveItemData.Container == 0u)
                        {
                            g_MoveItemData.Container = CAssistant::TextToSerial("backpack");
                        }
                        g_Assistant.MoveItem(equipped);
                    }
                    else if (g_AssistantConfig.DoubleClickForEquipItems())
                    {
                        g_Assistant.DoubleClick(obj->Serial);
                    }
                    else
                    {
                        g_Assistant.MoveEquip(obj->Serial, EquipLayer(obj), g_PlayerSerial);
                    }
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    m_PlayPosition--;
                    return;
                }
                break;
            }
            case MT_SET_DRESS:
                m_DressList.clear();
                for (int i = 0; i < dressLayersCount; i++)
                {
                    CGameItem *obj = g_Player->FindLayer(dressLayers[i]);
                    if (obj != nullptr)
                    {
                        m_DressList.push_back(obj->Serial);
                    }
                }
                break;
            case MT_UNDRESS:
            {
                for (int i = 0; i < dressLayersCount; i++)
                {
                    CGameItem *obj = g_Player->FindLayer(dressLayers[i]);
                    if (obj != nullptr)
                    {
                        g_MoveItemData.Reset();
                        g_MoveItemData.Container = CAssistant::TextToSerial("dressbag");
                        if (g_MoveItemData.Container == 0u)
                        {
                            g_MoveItemData.Container = CAssistant::TextToSerial("backpack");
                        }
                        g_Assistant.MoveItem(obj);
                        m_NextMacroTick = g_Ticks + macro->WaitingTime;
                        m_PlayPosition--;
                        return;
                    }
                }
                break;
            }
            case MT_DRESS:
            {
                while (!m_DressList.empty())
                {
                    uint serial = m_DressList.front();
                    CGameItem *obj = g_World != nullptr ? g_World->FindWorldItem(serial) : nullptr;
                    if (obj == nullptr || obj->Layer == EquipLayer(obj))
                    {
                        m_DressList.erase(m_DressList.begin());
                        continue;
                    }
                    CGameItem *equipped = g_Player->FindLayer(EquipLayer(obj));
                    if (equipped != nullptr && g_AssistantConfig.UndressBeforeDressing())
                    {
                        g_MoveItemData.Reset();
                        g_MoveItemData.Container = CAssistant::TextToSerial("dressbag");
                        if (g_MoveItemData.Container == 0u)
                        {
                            g_MoveItemData.Container = CAssistant::TextToSerial("backpack");
                        }
                        g_Assistant.MoveItem(equipped);
                    }
                    else if (g_AssistantConfig.DoubleClickForEquipItems())
                    {
                        g_Assistant.DoubleClick(obj->Serial);
                    }
                    else
                    {
                        g_Assistant.MoveEquip(obj->Serial, EquipLayer(obj), g_PlayerSerial);
                    }
                    m_NextMacroTick = g_Ticks + macro->WaitingTime;
                    m_PlayPosition--;
                    return;
                }
                break;
            }
            case MT_WAIT:
                m_NextMacroTick = g_Ticks + macro->WaitingTime;
                return;
            default:
                break;
        }
        m_WaitingEnabled = false;
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
