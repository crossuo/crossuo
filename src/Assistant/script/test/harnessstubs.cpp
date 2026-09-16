// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Link stubs for the script harness (xuoassist-script-test): the harness
// links the REAL apiscript.cpp (registration + drain executor) without the
// client. Everything here is a stand-in for a client-side symbol the
// executor may reference; the assertions only exercise paths that either
// stay script-side or hit the fake hooks below (g_HarnessGump).
//
// Not-a-client warning: the executor's g_World/g_Player gated blocks stay
// inert (the globals are null/false here), matching a disconnected client.

#define XUO_ASSISTANT 1
#define XUO_ASSISTANT_HARNESS 1

#include <cctype> // tolower (the str_lower stub below)

#include "../../core/global.h"
#include "../../core/world.h" // the worldstubs.h stand-ins (disconnected)
#include "../../core/assist.h"
#include "../../core/dressagent.h"
#include "../../core/shopagent.h"
#include "Target.h" // the fused target cursor (the harness stubs it below)
#include "../../core/textcommandmanager.h"
#include "../../config/assistantconfig.h"
#include "../../managers/contextmenumanager.h"
#include "../../managers/gumpmanager.h"
#include "../../managers/menumanager.h"
#include "Managers/ObjectPropertiesManager.h"
#include "../../managers/promptmanager.h"
#include "../../managers/spellmanager.h"
#include "../../managers/trademanager.h"
#include "../../model/commonitems/gump.h"
#include "../../model/commonitems/journalmessage.h"
#include "../../model/commonitems/menu.h"
#include "Network/Packets.h"
#include "Target.h" // the real client target-cursor class (stubbed below)
#include "ScreenshotBuilder.h"
#include "Walker/PathFinder.h"
#include "../../macros/macroexecutor.h" // RecordingSuspension (stubbed below)
#include <stdlib.h> // abs (the Distance stub)

// Minimal stand-in for the client-side talk-display core the executor's
// prints call (the real client headers drag the whole SDL/Platform stack in,
// which the harness does not use); the member signatures match
// src/Managers/PacketManager.h.
class CPacketManager
{
public:
    CPacketManager();
    virtual ~CPacketManager();
    void DisplayTalkTextA(uint32_t, CGameObject *, const astr_t &, astr_t, uint16_t, uint16_t, uint8_t);
    void DisplayTalkTextW(uint32_t, CGameObject *, const astr_t &, wstr_t, uint16_t, uint8_t);
};

// The client-side entry points the executor now calls directly (the calls
// that replaced the 0xFC messages), faked like the connection globals above:
// the member signatures match src/Managers/*.h and src/TextEngine/GameConsole.h
// (GUMP_TYPE is the real xuocore enum: it is part of the mangled signatures).
// The paths reached in the harness run with g_World == nullptr, so the stub
// bodies keep those calls inert.
#include <xuocore/enumlist.h>

// The client skill-send bridge the executor calls directly (CGame::UseSkill
// is the real client recording tap; the executor suspends recording around
// it). The class is the worldstubs.h stand-in for CrossUO.h; the body is
// inert here (the harness runs disconnected).
CGame g_Game;
void CGame::UseSkill(int) {}
int CGame::ValueInt(const VALUE_KEY_INT &, int value)
{
    // the bridge pointers were null here (get returned 0, set was skipped)
    return value;
}

class CGumpManager
{
public:
    CGumpManager();
    ~CGumpManager();
    void CloseGump(uint32_t, uint32_t, GUMP_TYPE);
    void SelectMenu(uint32_t, uint32_t, int);
    void ShowStatusbar(uint32_t, int, int, bool);
    void MovePaperdoll(uint32_t, int, int);
    void SecureTradeCheck(uint32_t, bool);
    void SecureTradeClose(uint32_t);
};
class CMacroManager
{
public:
    CMacroManager();
    ~CMacroManager();
    void ExecuteFromLists(const std::vector<astr_t> &, const std::vector<astr_t> &);
};

extern CGumpManager g_GumpManager;
CGumpManager g_GumpManager;
CGumpManager::CGumpManager() {}
CGumpManager::~CGumpManager() {}
void CGumpManager::CloseGump(uint32_t, uint32_t, GUMP_TYPE) {}
void CGumpManager::SelectMenu(uint32_t, uint32_t, int) {}
void CGumpManager::ShowStatusbar(uint32_t, int, int, bool) {}
void CGumpManager::MovePaperdoll(uint32_t, int, int) {}
void CGumpManager::SecureTradeCheck(uint32_t, bool) {}
void CGumpManager::SecureTradeClose(uint32_t) {}

extern CMacroManager g_MacroManager;
CMacroManager g_MacroManager;
CMacroManager::CMacroManager() {}
CMacroManager::~CMacroManager() {}
void CMacroManager::ExecuteFromLists(const std::vector<astr_t> &, const std::vector<astr_t> &)
{
}

void SendConsoleText(wstr_t, uint16_t) {}

namespace xuoassist {

extern CGump *g_HarnessGump;

// --- harness hooks -----------------------------------------------------------
// The executor deep-copies the manager record for GetGump; the harness points
// the fake manager at this one.
CGump *g_HarnessGump = nullptr;


uint32_t CAssistant::TextToSerial(astr_t text)
{
    // Parity with the original for the tests: plain hex text
    return (uint32_t)strtoul(text.c_str(), nullptr, 16);
}
astr_t CAssistant::SerialToText(const uint32_t &value)
{
    char buf[16] = { 0 };
    snprintf(buf, sizeof(buf), "0x%X", value);
    return buf;
}
astr_t CAssistant::GraphicToText(const uint16_t &value)
{
    char buf[16] = { 0 };
    snprintf(buf, sizeof(buf), "0x%X", value);
    return buf;
}
uint16_t CAssistant::TextToGraphic(astr_t text)
{
    return (uint16_t)strtoul(text.c_str(), nullptr, 16);
}
void CAssistant::ClientPrint(const astr_t &, const uint16_t &) const {}
void CAssistant::ClientCharPrint(const uint32_t &, const astr_t &, const uint16_t &) const {}
void CAssistant::DoubleClick(const uint32_t &) const {}
void CAssistant::MoveEquip(const uint32_t &, const int &, const uint32_t &) const {}
void CAssistant::GetFriendsStatus() const {}
void CAssistant::GetEnemiesStatus() const {}
void CAssistant::HandleSetPVPCaller(uint8_t *) const {}
CGameObject *CAssistant::FindFriend(const uint16_t &, const astr_t &) const { return nullptr; }
CGameObject *CAssistant::FindEnemy(const uint16_t &, const astr_t &) const { return nullptr; }
int CAssistant::TextToLayer(astr_t) { return 0; }
void CAssistant::RequestTarget(const astr_t &) const {}
int CAssistant::CountType(const vector<astr_t> &, const vector<astr_t> &, const astr_t &,
    const astr_t &, bool) const
{
    return 0;
}
CGameObject *CAssistant::FindType(const vector<astr_t> &, const vector<astr_t> &, const astr_t &,
    const astr_t &, const uint16_t &, const astr_t &, bool, vector<astr_t> &) const
{
    return nullptr;
}
CGameObject *CAssistant::FindList(const astr_t &, const astr_t &, const astr_t &, const uint16_t &,
    const astr_t &, bool, vector<astr_t> &) const
{
    return nullptr;
}

// --- config (DelayFromText keeps the old stub contract: len) -----------------
CAssistantConfig::CAssistantConfig()
{
    memset(&m_Options[0], 0, sizeof(m_Options));
}
CAssistantConfig g_AssistantConfig;
CMacroEntry::~CMacroEntry() = default;
int CAssistantConfig::DelayFromText(astr_t text) { return (int)text.size(); }
bool CAssistantConfig::GetFontColor(uint16_t &) const { return false; }
void CAssistantConfig::SetFontColor(const bool &, uint16_t) {}
bool CAssistantConfig::GetCharactersFontColor(uint16_t &) const { return false; }
void CAssistantConfig::SetCharactersFontColor(const bool &, uint16_t) {}
void CAssistantConfig::SaveHotkeys() {}
bool CAssistantConfig::LoadHotkeys() { return true; }

// --- managers (inert; the gump manager serves the GetGump deep copy) ---------
CGumpManager::CGumpManager() {}
CGumpManager::~CGumpManager() {}
CGumpManager g_GumpManager;
CGump *CGumpManager::GetGump(const uint32_t &) { return g_HarnessGump; }
CGump *CGumpManager::GetGump(const uint32_t &, const uint32_t &) { return g_HarnessGump; }
CGump *CGumpManager::GetLastGump() { return g_HarnessGump; }
int CGumpManager::GetGumpCount()
{
    return g_HarnessGump != nullptr ? 1 : 0;
}
void CGumpManager::AddHook(CGumpHook *) {}
void CGumpManager::ClearHooks() {}
bool CGumpManager::SendGumpChoice(CGump *, CGumpHook &) { return false; }
void CGumpManager::InfoGump(const astr_t &) {}

CMenuManager::CMenuManager() {}
CMenuManager::~CMenuManager() {}
CMenuManager g_MenuManager;
CMenu *CMenuManager::GetLastMenu() { return nullptr; }
CMenu *CMenuManager::GetMenu(const astr_t &) { return nullptr; }
int CMenuManager::GetMenuCount() { return 0; }
void CMenuManager::SetHook(const astr_t &, const astr_t &) {}
void CMenuManager::ClearHooks() {}
bool CMenuManager::SendChoiceIfMenuExists(const astr_t &, const astr_t &, const bool &)
{
    return false;
}

CPromptManager::CPromptManager() {}
CPromptManager g_PromptManager;
void CPromptManager::AddHook(const CPrompt &) {}
void CPromptManager::ClearHooks() {}
void CPromptManager::Send(astr_t) {}

CTradeManager::CTradeManager() {}
CTradeManager::~CTradeManager() {}
CTradeManager g_TradeManager;
int CTradeManager::Count() { return 0; }
CTradeWindow *CTradeManager::GetTrade(const astr_t &) { return nullptr; }

CContextMenuManager::CContextMenuManager() {}
CContextMenuManager g_ContextMenuManager;
void CContextMenuManager::SetHook(const uint32_t &, const uint32_t &) {}
void CContextMenuManager::RemoveHook() {}

} // namespace xuoassist

// the fused client manager (global namespace, per Managers/ObjectPropertiesManager.h)
CObjectPropertiesManager g_ObjectPropertiesManager;
CObjectPropertiesManager::~CObjectPropertiesManager() = default;
astr_t CObjectPropertiesManager::GetData(const uint32_t &) { return {}; }

namespace xuoassist {

CDressAgent g_DressAgent;
void CDressAgent::Disarm() {}
void CDressAgent::Undress() {}

CShopAgent g_ShopAgent;
bool CShopAgent::IsActiveShopping() const { return false; }
void CShopAgent::SendShopRequest(int, const astr_t &, const astr_t &) {}

// The recording suspension the executor's send calls wrap (the RAII guard
// is header-inline and drives g_MacroExecutor below; the harness runs
// disconnected, so the depth counter is inert).
CMacroExecutor g_MacroExecutor;

CSpellManager::CSpellManager() {}
CSpellManager::~CSpellManager() {}
CSpellManager g_SpellManager;
void CSpellManager::Cast(const astr_t &) {}
void CSpellManager::CastIndex(int) {}
void CSpellManager::TargetCast(const astr_t &, const uint32_t &) {}

CTextCommandManager g_TextCommandManager;
bool CTextCommandManager::RunCommand(const astr_t &, const vector<astr_t> &) { return false; }
uint16_t CTextCommandManager::GetFindFlags(const astr_t &) { return 0; }
uint8_t CTextCommandManager::GetJournalFlags(const astr_t &) { return 0; }
uint8_t CTextCommandManager::GetSkillsFlags(const astr_t &) { return 0; }

bool g_Connected = false;
uint64_t g_EnabledCommands = 0;
uint64_t g_EnabledFeature = 0;
uint16_t g_MessagesColor = 0x02B2;
CPoint2Di g_LastTargetPosition;
CPoint2Di g_LastAttackPosition;
bool g_TargetReceived = false;
bool g_MenuReceived = false;
bool g_GumpReceived = false;
bool g_PromptReceived = false;
bool g_ShopReceived = false;
bool g_TradeReceived = false;
bool g_ContextMenuReceived = false;
SelectedTile g_SelectedClientWorldObject;
TARGET_HANDLER g_TargetHandler = nullptr;
} // namespace xuoassist

// world-model stand-in globals (global namespace, as declared in worldstubs.h)
CGameWorld *g_World = nullptr;
CPlayer *g_Player = nullptr;
// the client globals the fused assistant LastTarget/LastAttack reads consume
uint32_t g_LastTargetObject = 0;
uint32_t g_LastAttackObject = 0;
// the client global the xuoassist using-declaration (global.h) aliases
uint32_t g_PlayerSerial = 0;
CSkillsManager g_SkillsManager;

// world stand-in out-of-line bodies (global namespace)
CGameObject *CGameWorld::FindWorldObject(uint32_t) { return nullptr; }
CGameItem *CGameWorld::FindWorldItem(uint32_t) { return nullptr; }
CGameCharacter *CGameWorld::FindWorldCharacter(uint32_t) { return nullptr; }
// CPlayer::FindBandage is defined inline in worldstubs.h

// worldutil.h bodies (worldutil.cpp is not linked here)
int xuoassist::EquipLayer(const CGameItem *) { return 0; }
int xuoassist::Distance(int x1, int y1, int x2, int y2)
{
    int distx = abs(x2 - x1);
    int disty = abs(y2 - y1);
    return distx > disty ? distx : disty;
}

namespace xuoassist {

CAssistant g_Assistant;
void CAssistant::LogOut() const {}

// --- client-side pieces the executor references ------------------------------
int Actions_NewTargetSystemSerial() { return 0; }

} // namespace xuoassist

CPathFinder::CPathFinder() {}
CPathFinder::~CPathFinder() = default;
CPathFinder g_PathFinder;
bool CPathFinder::CanWalk(uint8_t &, int &, int &, char &) { return false; }
bool CPathFinder::Walk(bool, uint8_t) { return false; }
bool CPathFinder::WalkTo(int, int, int, int) { return false; }
void CPathFinder::StopAutoWalk() {}

CScreenshotBuilder g_ScreenshotBuilder;
CScreenshotBuilder::CScreenshotBuilder() {}
CScreenshotBuilder::~CScreenshotBuilder() {}
void CScreenshotBuilder::SaveScreen() {}
void CScreenshotBuilder::SaveScreen(int, int, int, int) {}

CPacket::CPacket(size_t, bool) {}
CPacketClientViewRange::CPacketClientViewRange(uint8_t) : CPacket(0) {}
CPacketWrestlingStun::CPacketWrestlingStun() : CPacket(0) {}
CPacketWrestlingDisarm::CPacketWrestlingDisarm() : CPacket(0) {}
CPacketInvokeVirtueRequest::CPacketInvokeVirtueRequest(uint8_t) : CPacket(0) {}
// client send builders now used directly by apiscript (E1 swap); never
// exercised in the harness, only linked
CPacketRequestPopupMenu::CPacketRequestPopupMenu(uint32_t) : CPacket(0) {}
CPacketDoubleClickRequest::CPacketDoubleClickRequest(uint32_t) : CPacket(0) {}
CPacketTargetSelectedObject::CPacketTargetSelectedObject(int, int) : CPacket(0) {}
CPacketUseCombatAbility::CPacketUseCombatAbility(uint8_t) : CPacket(0) {}
CPacketTargetUseSkill::CPacketTargetUseSkill(uint16_t, uint32_t) : CPacket(0) {}
void CPacket::Send() {}

// the ability-name table jsn_useability reads (the real definition lives in
// GumpCombatBook.cpp, which is not linked here)
#include "Gumps/GumpCombatBook.h"
const astr_t CGumpCombatBook::m_AbilityName[MAX_ABILITIES_COUNT]{
    "Armor Ignore",       "Bleed Attack",    "Concussion Blow",
    "Crushing Blow",      "Disarm",          "Dismount",
    "Double Strike",      "Infecting",       "Mortal Strike",
    "Moving Shot",        "Paralyzing Blow", "Shadow Strike",
    "Whirlwind Attack",
    "Riding Swipe", // CV_500a
    "Frenzied Whirlwind", "Block",           "Defense Mastery",
    "Nerve Strike",       "Talon Strike",    "Feint",
    "Dual Wield",         "Double Shot",     "Armor Pierce",
    "Bladeweave",         "Force Arrow",     "Lightning Arrow",
    "Psychic Attack",     "Serpent Arrow",   "Force of Nature",
    "Infused Throw", // CV_7000
    "Mystic Arc",         "Disrobe"
};

// the fused target cursor (Target.h): the apiscript hook/cancel calls stub
// against it; nothing in the harness opens, drains or consumes a cursor
CTarget g_Target;
CTarget::CTarget() {}
void CTarget::RequestCursor() {}
void CTarget::CancelCursor() {}
bool CTarget::CheckHook() { return false; }
void CTarget::ClearHooks() {}
void CTarget::SetObjectsHook(const std::vector<astr_t> &) {}
void CTarget::SetTileHook(const astr_t &, const int &, const int &, const int &, const bool &) {}
void CTarget::RestoreTarget() {}
void CTarget::SendTargetObject(int) {}
void CTarget::SendTargetTile(uint16_t, short, short, char) {}
void CTarget::SendCancelTarget() {}
void CTarget::SendLastTarget() {}
void CTarget::SendTarget() {}
void CTarget::SendHookTargetObject(const uint32_t &) {}
void CTarget::SendHookTargetTile(const uint16_t, const short &, const short &, short) {}
void CTarget::SendHookTargetLastTile() {}
void CTarget::SendHookCancelTarget() {}
bool CTarget::SetData(CDataReader &) { return false; }
bool CTarget::SetMultiData(CDataReader &) { return false; }

// pulled in by the PathFinder/point includes (client-side, never exercised)
CPathNode::CPathNode() {}
CPathNode::~CPathNode() = default;
CPoint2Di::CPoint2Di() {}
CPoint2Di::CPoint2Di(int, int) {}
CPoint2Di::CPoint2Di(float, float) {}
CPoint2Di::CPoint2Di(const CPoint2Di &) {}
CPoint2Di::CPoint2Di(const CPoint2Df &) {}

// the talk display core the assistant prints now call directly (inert here)
extern CPacketManager g_PacketManager;
CPacketManager g_PacketManager;
CPacketManager::CPacketManager() {}
CPacketManager::~CPacketManager() = default;
// the talk display core the assistant prints now call directly (inert here)
void CPacketManager::DisplayTalkTextA(uint32_t, CGameObject *, const astr_t &, astr_t, uint16_t, uint16_t, uint8_t)
{
}
void CPacketManager::DisplayTalkTextW(uint32_t, CGameObject *, const astr_t &, wstr_t, uint16_t, uint8_t)
{
}

// --- remaining client/common pieces the real executor references -------------
wstr_t wstr_from(const astr_t &) { return wstr_t(); }
// the client implementation lives in Misc.cpp's STR_IMPLEMENTATION (not
// linked here); the executor's case-insensitive matches only need the
// lowering itself
astr_t str_lower(astr_t value)
{
    for (char &c : value)
    {
        c = (char)tolower((unsigned char)c);
    }
    return value;
}

namespace xuoassist {
void ScriptManager_LoadScriptFile(const astr_t &) {}
void ScriptManager_Terminate(const astr_t &, const astr_t &) {}
bool ScriptManager_Running(const astr_t &) { return false; }
void ScriptManager_Exec(const astr_t &, bool, const vector<astr_t> &) {}
} // namespace xuoassist
