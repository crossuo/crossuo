// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Link stubs for the script harness (xuoassist-script-test): the harness
// links the REAL apiscript.cpp (registration + drain executor) without the
// client. Everything here is a stand-in for a client-side symbol the
// executor may reference; the assertions only exercise paths that either
// stay script-side or hit the fake hooks below (g_HarnessGump).
//
// Not-a-client warning: the executor's g_World/g_Player gated blocks stay
// inert (the globals are null/false here), mirroring a disconnected client.

#define XUO_ASSISTANT 1

#include "../../core/global.h"
#include "../../core/assist.h"
#include "../../core/dressagent.h"
#include "../../core/packets.h"
#include "../../core/shopagent.h"
#include "../../core/target.h"
#include "../../core/textcommandmanager.h"
#include "../../config/assistantconfig.h"
#include "../../managers/contextmenumanager.h"
#include "../../managers/gumpmanager.h"
#include "../../managers/menumanager.h"
#include "../../managers/objectpropertiesmanager.h"
#include "../../managers/promptmanager.h"
#include "../../managers/skillmanager.h"
#include "../../managers/spellmanager.h"
#include "../../managers/trademanager.h"
#include "../../model/commonitems/gump.h"
#include "../../model/commonitems/journalmessage.h"
#include "../../model/commonitems/menu.h"
#include "../../model/commonitems/skill.h"
#include "../../model/datareader.h"
#include "../../model/gameobjects/gameitem.h"
#include "../../model/gameobjects/gameplayer.h"
#include "../../model/gameobjects/gameworld.h"
#include "Network/Packets.h"
#include "ScreenshotBuilder.h"
#include "Walker/PathFinder.h"

// Minimal stand-ins for the two client connection globals the real assistant
// packet sends land in (core/packets.cpp is linked). The real client headers
// drag the whole SDL/Platform stack in, which the harness does not use; the
// member signatures match src/Managers/*.h.
class CConnectionManager
{
public:
    CConnectionManager();
    ~CConnectionManager();
    void Send(unsigned char *, int);
};
class CPacketManager
{
public:
    CPacketManager();
    virtual ~CPacketManager();
    void SavePluginReceivePacket(unsigned char *, int);
};

namespace xuoassist {

extern CGump *g_HarnessGump;

// --- harness hooks -----------------------------------------------------------
// The executor deep-copies the manager record for GetGump; the harness points
// the fake manager at this one.
CGump *g_HarnessGump = nullptr;


uint CAssistant::TextToSerial(string text)
{
    // Parity with the original for the tests: plain hex text
    return (uint)strtoul(text.c_str(), nullptr, 16);
}
string CAssistant::SerialToText(const uint &value)
{
    char buf[16] = { 0 };
    snprintf(buf, sizeof(buf), "0x%X", value);
    return buf;
}
string CAssistant::GraphicToText(const ushort &value)
{
    char buf[16] = { 0 };
    snprintf(buf, sizeof(buf), "0x%X", value);
    return buf;
}
ushort CAssistant::TextToGraphic(string text)
{
    return (ushort)strtoul(text.c_str(), nullptr, 16);
}
void CAssistant::ClientPrint(const string &, const ushort &) {}
void CAssistant::ClientCharPrint(const uint &, const string &, const ushort &) {}
void CAssistant::DoubleClick(const uint &) {}
void CAssistant::MoveEquip(const uint &, const int &, const uint &) {}
void CAssistant::GetFriendsStatus() {}
void CAssistant::GetEnemiesStatus() {}
void CAssistant::HandleSetPVPCaller(uchar *) {}
CGameObject *CAssistant::FindFriend(const ushort &, const string &) { return nullptr; }
CGameObject *CAssistant::FindEnemy(const ushort &, const string &) { return nullptr; }
int CAssistant::TextToLayer(string) { return 0; }
void CAssistant::RequestTarget(const string &) {}
int CAssistant::CountType(const vector<string> &, const vector<string> &, const string &,
    const string &, bool)
{
    return 0;
}
CGameObject *CAssistant::FindType(const vector<string> &, const vector<string> &, const string &,
    const string &, const ushort &, const string &, bool, vector<string> &)
{
    return nullptr;
}
CGameObject *CAssistant::FindList(const string &, const string &, const string &, const ushort &,
    const string &, bool, vector<string> &)
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
int CAssistantConfig::DelayFromText(string text) { return (int)text.size(); }
bool CAssistantConfig::GetFontColor(ushort &) { return false; }
void CAssistantConfig::SetFontColor(const bool &, ushort) {}
bool CAssistantConfig::GetCharactersFontColor(ushort &) { return false; }
void CAssistantConfig::SetCharactersFontColor(const bool &, ushort) {}
void CAssistantConfig::SaveHotkeys() {}
bool CAssistantConfig::LoadHotkeys() { return true; }

// --- managers (inert; the gump manager serves the GetGump deep copy) ---------
CGumpManager::CGumpManager() {}
CGumpManager::~CGumpManager() {}
CGumpManager g_GumpManager;
CGump *CGumpManager::GetGump(const uint &) { return g_HarnessGump; }
CGump *CGumpManager::GetGump(const uint &, const uint &) { return g_HarnessGump; }
CGump *CGumpManager::GetLastGump() { return g_HarnessGump; }
int CGumpManager::GetGumpCount()
{
    return g_HarnessGump != nullptr ? 1 : 0;
}
void CGumpManager::AddHook(CGumpHook *) {}
void CGumpManager::ClearHooks() {}
bool CGumpManager::SendGumpChoice(CGump *, CGumpHook &) { return false; }
void CGumpManager::InfoGump(const string &) {}

CMenuManager::CMenuManager() {}
CMenuManager::~CMenuManager() {}
CMenuManager g_MenuManager;
CMenu *CMenuManager::GetLastMenu() { return nullptr; }
CMenu *CMenuManager::GetMenu(const string &) { return nullptr; }
int CMenuManager::GetMenuCount() { return 0; }
void CMenuManager::SetHook(const string &, const string &) {}
void CMenuManager::ClearHooks() {}
bool CMenuManager::SendChoiceIfMenuExists(const string &, const string &, const bool &)
{
    return false;
}

CPromptManager::CPromptManager() {}
CPromptManager g_PromptManager;
void CPromptManager::AddHook(const CPrompt &) {}
void CPromptManager::ClearHooks() {}
void CPromptManager::Send(string) {}

CTradeManager::CTradeManager() {}
CTradeManager::~CTradeManager() {}
CTradeManager g_TradeManager;
int CTradeManager::Count() { return 0; }
CTradeWindow *CTradeManager::GetTrade(const string &) { return nullptr; }

CContextMenuManager::CContextMenuManager() {}
CContextMenuManager g_ContextMenuManager;
void CContextMenuManager::SetHook(const uint &, const uint &) {}
void CContextMenuManager::RemoveHook() {}

CObjectPropertiesManager g_ObjectPropertiesManager;
CObjectPropertiesManager::~CObjectPropertiesManager() = default;
string CObjectPropertiesManager::GetData(const uint &) { return {}; }

CDressAgent g_DressAgent;
void CDressAgent::Disarm() {}
void CDressAgent::Undress() {}

CShopAgent g_ShopAgent;
bool CShopAgent::IsActiveShopping() { return false; }
void CShopAgent::SendShopRequest(int, const string &, const string &) {}

CSkillManager::CSkillManager() {}
CSkillManager::~CSkillManager() {}
CSkillManager g_SkillManager;
int CSkillManager::GetIndex(const string &) { return -1; }
void CSkillManager::Use(const string &) {}
void CSkillManager::TargetUse(const string &, const uint &) {}

CSpellManager::CSpellManager() {}
CSpellManager::~CSpellManager() {}
CSpellManager g_SpellManager;
void CSpellManager::Cast(const string &) {}
void CSpellManager::TargetCast(const string &, const uint &) {}

CTextCommandManager::CTextCommandManager() {}
CTextCommandManager::~CTextCommandManager() {}
CTextCommandManager g_TextCommandManager;
bool CTextCommandManager::RunCommand(const string &, const vector<string> &) { return false; }
ushort CTextCommandManager::GetFindFlags(const string &) { return 0; }
uchar CTextCommandManager::GetJournalFlags(const string &) { return 0; }
uchar CTextCommandManager::GetSkillsFlags(const string &) { return 0; }

CTarget::CTarget() {}
CTarget g_Target;
bool CTarget::CheckHook() { return false; }
void CTarget::ClearHooks() {}
void CTarget::SendCancelTarget() {}
void CTarget::SetObjectsHook(const vector<string> &) {}
void CTarget::SetTileHook(const string &, const int &, const int &, const int &, const bool &) {}
void CTarget::SetRecvAssistData(CDataReader &) {}

// --- world model (a disconnected client) -------------------------------------
CGameWorld *g_World = nullptr;
CPlayer *g_Player = nullptr;
uint g_PlayerSerial = 0;
bool g_Connected = false;
uint64_t g_EnabledCommands = 0;
uint64_t g_EnabledFeature = 0;
ushort g_MessagesColor = 0x02B2;
uint g_LastTargetObject = 0;
uint g_LastAttackObject = 0;
CPoint2Di g_LastTargetPosition;
CPoint2Di g_LastAttackPosition;
bool g_TargetReceived = false;
bool g_MenuReceived = false;
bool g_GumpReceived = false;
bool g_PromptReceived = false;
bool g_ShopReceived = false;
bool g_TradeReceived = false;
bool g_ContextMenuReceived = false;
UOI_SELECTED_TILE g_SelectedClientWorldObject;
int GetDistance(const int &, const int &, const int &, const int &) { return 0; }
FUNCDEF_GET_VALUE_INT *g_GetInt = nullptr;
FUNCDEF_SET_VALUE_INT *g_SetInt = nullptr;
TARGET_HANDLER g_TargetHandler = nullptr;

// model pieces the executor's getter paths reference
bool CGameObject::Flying() { return false; }
bool CGameObject::Poisoned() { return false; }
CGameItem *CGameObject::FindLayer(const int &) { return nullptr; }
int CGameItem::GetEquipLayer() { return 0; }
CGameObject *CGameWorld::FindWorldObject(const uint &) { return nullptr; }
CGameItem *CPlayer::FindBandage() { return nullptr; }
CGameWorld::CGameWorld(const uint &serial)
{
    (void)serial;
    m_Items = nullptr;
}
CGameWorld::~CGameWorld() {}

CAssistant::CAssistant() {}
CAssistant::~CAssistant() {}
CAssistant g_Assistant;

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
CPacketInvokeVirtueRequest::CPacketInvokeVirtueRequest(uint8_t) : CPacket(0) {}
void CPacket::Send() {}

// pulled in by the PathFinder/point includes (client-side, never exercised)
CPathNode::CPathNode() {}
CPathNode::~CPathNode() = default;
CPoint2Di::CPoint2Di() {}
CPoint2Di::CPoint2Di(int, int) {}
CPoint2Di::CPoint2Di(float, float) {}
CPoint2Di::CPoint2Di(const CPoint2Di &) {}
CPoint2Di::CPoint2Di(const CPoint2Df &) {}

// the packet send path of the assistant wrappers (real packets.cpp is linked;
// these two globals are the client connection it hands bytes to)
extern CConnectionManager g_ConnectionManager;
CConnectionManager g_ConnectionManager;
CConnectionManager::CConnectionManager() {}
CConnectionManager::~CConnectionManager() = default;
void CConnectionManager::Send(unsigned char *, int) {}
extern CPacketManager g_PacketManager;
CPacketManager g_PacketManager;
CPacketManager::CPacketManager() {}
CPacketManager::~CPacketManager() = default;
void CPacketManager::SavePluginReceivePacket(unsigned char *, int) {}

// --- remaining client/common pieces the real executor references -------------
xuoassist::CDataReader::CDataReader() {}
xuoassist::CDataReader::CDataReader(uint8_t *, size_t) {}
xuoassist::CDataReader::~CDataReader() {}
CDataWriter::~CDataWriter() {} // xuocore/mappedfile.h (the client packet base)
wstr_t wstr_from(const astr_t &) { return wstr_t(); }

// the skill table read (SkillValue); empty here
namespace xuoassist {
CSkill g_Skills[MAX_SKILLS_COUNT];
int g_SkillsCount = 0;
void ScriptManager_LoadScriptFile(const string &) {}
void ScriptManager_Terminate(const string &, const string &) {}
bool ScriptManager_Running(const string &) { return false; }
void ScriptManager_Exec(const string &, bool, const vector<string> &) {}
} // namespace xuoassist
