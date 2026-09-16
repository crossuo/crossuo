// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// AssistantConfig: the persistence module (master plan §4.3, §7 Phase 1 item
// 6). The original app kept every reader/writer inside its widget classes
// (assist/tab*.cpp LoadConfig/SaveConfig, assistform.cpp profile handling,
// hotkeysform.cpp Hotkeys.xml, tabagentsdress.cpp Dress.coa); they are
// extracted here verbatim so Phase 2 can build UI straight on top.
//
// File formats are byte-compatible with the original writers (same elements,
// attribute names, value formatting - including the original `size = count+1`
// quirks of FindList.xml/IgnoreList.xml/Hotkeys.xml, kept for round-trip
// fidelity).
//
// Layout (matches the original app, relative to the binary dir):
//   Config/<profile>/ : Options.xml Objects.xml Hotkeys.xml Party.xml
//                       Friends.xml Enemies.xml Macros.xml Dress.coa
//   GlobalConfig/     : Types.xml Objects? no - Types.xml, FindList.xml,
//                       IgnoreList.xml, SpeechFilter.xml, TextReplaces.xml,
//                       SoundFilter.xml, ShopList.xml, DisplayItems.xml,
//                       DisplayGroups.xml, Profiles.xml
//
// Data that used to live in the old list-widget items is kept here as plain structs
// with the original item-class field names; the UI round binds its list
// widgets directly to these containers.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/global.h"
#include "config/xml.h"
#include "model/commonitems/finditem.h"
#include "model/commonitems/ignoreitem.h"
#include "model/commonitems/displayitem.h"

namespace xuoassist {

class CMacro;

// From the original app's assist/tabmain.h - the 23-option checklist order (§2.2).
enum CHECKLISTBOX_IDENTIFICATOR
{
    CLBI_CORPSES_AUTOOPEN = 0,
    CLBI_STEALTH_STEP_COUNTER,
    CLBI_SOUND_FILTER,
    CLBI_FILTER_LIGHT,
    CLBI_FILTER_WEATHER,
    CLBI_FILTER_SEASON,
    CLBI_NO_DEATH_SCREEN,
    CLBI_OPEN_DOORS,
    CLBI_TRACKER,
    CLBI_STAY_ON_TOP,
    CLBI_FAST_ROTATION,
    CLBI_RECURSE_CONTAINERS_SEARCH,
    CLBI_AUTOSTART,
    CLBI_IGNORE_STAMINA_CHECK,
    CLBI_MINIMIZE_TO_TRAY,
    CLBI_SOUND_ECHO,
    CLBI_ANIMATION_ECHO,
    CLBI_EFFECT_ECHO,
    CLBI_FILTER_SPEECH,
    CLBI_TEXT_REPLACE,
    CLBI_OBJECT_INSPECTOR,
    CLBI_NO_GRAY_SCREEN,
    CLBI_SAVE_FAR_OBJECTS,
    CLBI_COUNT
};

// From assist/tabfiltersreplaces.cpp / gui/filterreplacelistitem.h
enum REPLACE_TEXT_TYPE
{
    RTT_ALL = 0,
    RTT_ASCII,
    RTT_UNICODE
};

enum REPLACE_TEXT_MODE
{
    RTM_ALL = 0,
    RTM_FRIENDS_AND_YOU,
    RTM_ENEMIES,
    RTM_OTHERS
};

// From the original app's assist/hotkeysform.h (HOTKEY_TYPES; kept here so the
// hotkey data model survives ahead of the Phase 3 hotkey engine).
enum HOTKEY_TYPES
{
    HT_PLAY_MACRO = 0,
    HT_STOP_MACRO,
    HT_SCRIPT,
    HT_EXTERNAL_CODE,
    HT_TOGGLE_HOTKEYS,
    HT_TOGGLE_MAP,
    HT_ATTACK,
    HT_USE_OBJECT,
    HT_ALLNAMES,
    HT_CAST,
    HT_CAST_SELF,
    HT_CAST_LAST_TARGET,
    HT_CAST_LAST_ATTACK,
    HT_CAST_ENEMY,
    HT_CAST_FRIEND,
    HT_CAST_NEW_TARGET_SYSTEM,
    HT_USE_SKILL,
    HT_USE_SKILL_SELF,
    HT_USE_SKILL_LAST_TARGET,
    HT_USE_SKILL_LAST_ATTACK,
    HT_USE_SKILL_ENEMY,
    HT_USE_SKILL_FRIEND,
    // the enum tail - the Phase-1c model stopped at
    // the skill entries; the stored uint32_t values and the Hotkeys.xml schema
    // are unchanged (values 0..21 above already match the original writer)
    HT_USE_SKILL_NEW_TARGET_SYSTEM,
    HT_TARGETING,
    HT_BANDAGE_HEALING,
    HT_NEW_TARGETING_SYSTEM_SET,
    HT_LAST // == the original HT_COUNT_OF_ITEMS (26)
};

// gui/worldobjectlistitem.h (friends / enemies / party accept / decline lists)
struct CWorldObjectEntry
{
    uint32_t Serial{ 0 };
    astr_t Name{ "" };
    bool Enabled{ false }; // was the item's checked state
};

// gui/typelistitem.h
struct CTypeEntry
{
    astr_t Name{ "" };
    uint16_t Type{ 0 };
};

// gui/objectlistitem.h
struct CObjectEntry
{
    astr_t Name{ "" };
    uint32_t Serial{ 0 };
};

// gui/findlistitem.h
struct CFindListEntry
{
    astr_t Name{ "" };
    vector<CFindItem> m_Items;
};

// gui/ignorelistitem.h
struct CIgnoreListEntry
{
    astr_t Name{ "" };
    vector<CIgnoreItem> m_Items;
};

// gui/filterspeechlistitem.h
struct CFilterSpeechEntry
{
    astr_t Text{ "" };
    astr_t Color{ "" };
    bool FullMatch{ false };
};

// gui/filtersoundlistitem.h
struct CFilterSoundEntry
{
    astr_t Index{ "" };
    astr_t Name{ "" };
};

// gui/filterreplacelistitem.h
struct CFilterReplaceEntry
{
    astr_t OriginalText{ "" };
    astr_t OriginalColor{ "" };
    astr_t ReplacedText{ "" };
    astr_t ReplacedColor{ "" };
    int Type{ 0 };
    int Mode{ 0 };
    bool FullMatch{ false };
};

// gui/shoplistitem.h (the CShopItem payload fields)
struct CShopItemEntry
{
    astr_t Graphic{ "" };
    astr_t Color{ "" };
    astr_t Name{ "" };
    astr_t Comment{ "" };
    int Count{ 0 };
    int Price{ 0 };
};

struct CShopListEntry
{
    astr_t Name{ "" };
    vector<CShopItemEntry> m_Items;
};

// gui/displaygrouplistitem.h
struct CDisplayGroupEntry
{
    astr_t Name{ "" };
    astr_t TitleData{ "" };
};

// gui/displayitemlistitem.h
struct CDisplayItemEntry
{
    astr_t Name{ "" };
    CDisplayItem m_Item;
};

// gui/dresslistitem.h - 25 layer slots (equip layers up to OL_24)
struct CDressSetEntry
{
    astr_t Name{ "" };
    uint32_t m_Items[25];

    CDressSetEntry() { memset(&m_Items[0], 0, sizeof(m_Items)); }
};

// gui/hotkeytableitem.h (data subset; SDL key codes since Hotkeys version 1)
struct CHotkeyEntry
{
    uint32_t Key{ 0 };
    uint8_t Modifiers{ 0 };
    uint32_t Type{ 0 };
    uint32_t Action{ 0 };
    astr_t ScriptOrMacroName{ "" };
    astr_t ExternalCode{ "" };
    bool RunningMode{ false };
};

// gui/macrolistitem.h (owner of the CMacro action list)
struct CMacroEntry
{
    astr_t Name{ "" };
    vector<CMacro *> m_Items;

    ~CMacroEntry();
};

class CAssistantConfig
{
public:
    astr_t ProfileName = "Default";

    CAssistantConfig();

    // --- paths (plan §4.3: <binary dir>/Config/<profile>/) -----------------
    // The client resolves writable data relative to the working directory at
    // startup (src/Main.cpp:414 g_ExePath = fs_path_current()), which equals
    // "next to the exe" for a normal install - the same place the original app
    // (g_DllPath) used.
    static fs_path GetRootPath();
    static fs_path GetGlobalConfigPath();
    static fs_path GetConfigPath();
    fs_path GetProfilePath() const;
    void EnsureDirectories(); // mkdir Config/, GlobalConfig/, profile/

    // --- profiles ----------------------------------------------------------
    const vector<astr_t> &GetProfiles() const { return m_Profiles; }
    void ScanProfiles();
    bool LoadProfileBySerial(uint32_t serial);
    void SetCurrentProfile(const astr_t &name); // case-insensitive, like TabMain
    bool LoadProfile();                         // was AssistForm::LoadCurrentProfile
    bool SaveProfile();                         // was AssistForm::SaveCurrentProfile
    void SaveProfileMap();                      // GlobalConfig/Profiles.xml
    void LoadProfileMap();
    const std::map<uint32_t, astr_t> &GetProfileMap() const { return m_ProfileMap; }

    // --- Options.xml (assistform.cpp SaveMainOptions + 4 tab SaveConfigs) ---
    void SaveMainOptions();
    void LoadMainOptions();

    // --- global files (GlobalConfig/) ---------------------------------------
    void LoadGlobals(); // Types/Find/Ignore/Speech/Replaces/Sound/Shop/Display/Profiles
    void SaveGlobals();
    void SaveTypes();         // was TabListsTypes::SaveTypes
    void LoadTypes();         // (with the original app's default rec list on empty)
    void SaveFindList();      // was TabListsFind::SaveFindList (size=count+1)
    void LoadFindList();
    void SaveIgnoreList();    // was TabListsIgnore::SaveIgnoreList (size=count+1)
    void LoadIgnoreList();
    void SaveSpeechFilter();  // was TabFiltersSpeech::SaveSpeechFilter
    void LoadSpeechFilter();
    void SaveTextReplaces();  // was TabFiltersReplaces::SaveTextReplaces
    void LoadTextReplaces();
    void SaveSoundFilter();   // was TabFiltersSound::SaveSoundFilter
    void LoadSoundFilter();
    void SaveShopList();      // was TabAgentsShop::SaveShopList
    void LoadShopList();      // (with the original app's defaults on empty)
    void SaveDisplayItems();  // was TabDisplay::SaveDisplayItems
    void LoadDisplayItems();  // (with the original app's default set on empty)
    void SaveDisplayGroups(); // was TabDisplay::SaveDisplayGroups
    void LoadDisplayGroups(); // (with allreag/default on empty)

    // --- per-profile files ---------------------------------------------------
    void SaveObjects();       // was TabListsObjects::SaveObjects (Objects.xml)
    void LoadObjects();
    void SaveHotkeys();       // was HotkeysForm::Save (Hotkeys.xml, version 1)
    bool LoadHotkeys();       // was HotkeysForm::Load (tolerates version 0)
    void SaveParty();         // was TabAgentsParty::SaveParty (Party.xml)
    void LoadParty();
    void SaveFriends();       // was TabListsFriends::SaveFriends (Friends.xml)
    void LoadFriends();
    void SaveEnemies();       // was TabListsEnemies::SaveEnemies (Enemies.xml)
    void LoadEnemies();
    void SaveMacros();        // was TabMacros::Save (Macros.xml)
    bool LoadMacros();        // was TabMacros::Load
    bool SaveDress();         // was TabAgentsDress::SaveDress (Dress.coa)
    bool LoadDress();         // was TabAgentsDress::LoadDress

    // --- main options (TabMain getters, incl. the g_EnabledFeature gates) ---
    bool CorpsesAutoopen() const;
    bool StealthStepCounter() const;
    bool SoundFilter() const;
    bool LightFilter() const;
    bool WeatherFilter() const;
    bool SeasonFilter() const;
    bool NoDeathScreen() const;
    bool AutoOpenDoors() const;
    bool Tracker() const;
    bool FastRotation() const;
    bool RecurseContainersSearch() const;
    bool Autostart() const;
    bool IgnoreStaminaCheck() const;
    bool MinimizeToTray() const;
    bool StayOnTop() const;
    bool SoundEcho() const;
    bool AnimationEcho() const;
    bool EffectEcho() const;
    bool SpeechFilter() const;
    bool TextReplace() const;
    bool ObjectInspector() const;
    bool NoGrayScreen() const;
    bool SaveFarObjects() const;
    bool GetOption(int index) const { return m_Options[index]; }
    void SetOption(int index, bool state) { m_Options[index] = state; }
    bool *Options() { return m_Options; }

    int GetMessagesLevel() const { return m_MessagesLevel; }
    int SearchItemsDistance() const { return m_SearchItemsDistance; }
    int UseItemsDistance() const { return m_UseItemsDistance; }
    int OpenCorpsesDistance() const { return m_OpenCorpsesDistance; }
    int CorpseKeepDelay() const { return m_CorpseKeepDelay; }
    int WaitTargetsDelay() const { return m_WaitTargetsDelay; }
    int MoveItemsDelay() const { return m_MoveItemsDelay; }
    int UseItemsDelay() const { return m_UseItemsDelay; }
    int WindowsOpacity() const { return m_WindowsOpacity; }
    bool AutoReconnect() const { return m_AutoReconnect; }
    int AutoReconnectSeconds() const { return m_AutoReconnectSeconds; }
    bool DoubleClickForOpenDoors() const { return m_DoubleClickForOpenDoors; }

    // Text-window font colors (TabMain::GetFontColor / SetFontColor).
    bool GetFontColor(uint16_t &value) const;
    void SetFontColor(const bool &state, uint16_t value);
    bool GetCharactersFontColor(uint16_t &value) const;
    void SetCharactersFontColor(const bool &state, uint16_t value);

    // --- display options (TabDisplay LoadConfig/SaveConfig + checkboxes) ----
    int DisplayType() const { return m_DisplayType; } // 0=off 1=text 2=colored text 3=colored gfx
    uint32_t DisplayFlags() const { return m_DisplayFlags; }
    bool DoubleImage() const { return (m_DisplayFlags & 0x01) != 0; }
    bool ApplyNotorietyColorOnName() const { return (m_DisplayFlags & 0x02) != 0; }
    bool HightFont() const { return (m_DisplayFlags & 0x04) != 0; }
    bool HighlightReagents() const { return (m_DisplayFlags & 0x08) != 0; }
    uint32_t DisplayTextColor() const { return m_DisplayTextColor; }
    uint32_t DisplayBackgroundColor() const { return m_DisplayBackgroundColor; }
    const astr_t &DisplayTitleData() const { return m_DisplayTitleData; }

    // --- hotkeys options (TabHotkeys LoadConfig/SaveConfig) -----------------
    bool OnOffHotkeys() const { return m_OnOffHotkeys; }
    bool PassHotkeys() const { return m_PassHotkeys; }
    const astr_t &CommandPrefix() const { return m_CommandPrefix; }
    // Was TabHotkeys::IsCommandPrefix.
    int IsCommandPrefix(const astr_t &text) const;

    // --- scripts options (TabScripts LoadConfig/SaveConfig) -----------------
    bool SortScripts() const { return m_SortScripts; }
    void SetSortScripts(bool value) { m_SortScripts = value; }
    const astr_t &ScriptFilePath() const { return m_ScriptFilePath; }
    void SetScriptFilePath(const astr_t &path) { m_ScriptFilePath = path; }

    // Was TabMain::UpdateFeatures: clears the user's checklist choices for
    // options whose server feature flag is not enabled (the CLBI->XAEF
    // table from tabmain.cpp, minus the two UI-only rows).
    void UpdateFeatures();

    // --- list queries (were TabListsTypes/Objects/Friends/Enemies) ----------
    uint16_t SeekType(const astr_t &text) const;
    uint32_t SeekObject(const astr_t &text) const;
    CFindListEntry *GetFindList(const astr_t &name);
    CIgnoreListEntry *GetIgnoreList(const astr_t &name);
    bool InFriendList(uint32_t serial) const;
    bool InEnemyList(uint32_t serial) const;
    bool FeelPartyMembersAsFriends() const { return m_FeelPartyMembersAsFriends; }
    void AddType(const astr_t &name, const astr_t &type);     // was TabListsTypes::AddType
    void RemoveType(const astr_t &name);
    void AddObject(const astr_t &name, const astr_t &serial); // was TabListsObjects::AddObject
    void RemoveObject(const astr_t &name);

    // --- party agent (TabAgentsParty) ----------------------------------------
    bool PartyInviteCanBeAutoAccepted(uint32_t serial) const;
    bool PartyInviteCanBeAutoDeclined(uint32_t serial) const;

    // --- filters (were TabFiltersSpeech/Replaces/Sound; consulted by the
    //     packet handlers exactly like the original code did) -----------------------
    bool CheckSpeechInFilter(const astr_t &text, const uint16_t &color) const;
    // Returns true when the text matched a replace rule; the caller (the talk
    // handlers, which hold the display context) renders the replacement carried
    // by the out-params — the reprint is no longer an injected packet.
    bool CheckReplaceText(
        const astr_t &text,
        const uint32_t &serial,
        const uint16_t &color,
        const bool &unicode,
        astr_t &replacedText,
        uint16_t &replacedColor) const;
    bool CheckSoundInFilter(const uint16_t &index) const;

    // --- distance / delay text helpers (were AssistForm methods) -------------
    int DistanceFromText(astr_t text);
    int DelayFromText(astr_t text);

    // --- display items / groups (were TabDisplay helpers) --------------------
    CDisplayItem GetCustomDisplayItemByName(const astr_t &name) const;
    astr_t GetGroupContent(const astr_t &name) const;

    // --- dress sets (data helpers from TabAgentsDress/commandmanager) --------
    CDressSetEntry *GetDressSet(const astr_t &name);
    CDressSetEntry *AddDressSet(const astr_t &name);
    void ClearDressSet(CDressSetEntry *set);
    bool UndressBeforeDressing() const { return m_DressUndressBeforeDress; }
    bool DoubleClickForEquipItems() const { return m_DressDoubleClickForEquipment; }

    // --- macros ---------------------------------------------------------------
    vector<CMacroEntry> &Macros() { return m_Macros; }

    // --- data (was the widget state of the original tabs) ---------------------------
    bool m_Options[CLBI_COUNT];
    int m_MessagesLevel{ 0 };
    int m_SearchItemsDistance{ 3 };
    int m_UseItemsDistance{ 3 };
    int m_OpenCorpsesDistance{ 2 };
    int m_CorpseKeepDelay{ 15000 };
    int m_WaitTargetsDelay{ 1000 };
    int m_MoveItemsDelay{ 600 };
    int m_UseItemsDelay{ 600 };
    int m_WindowsOpacity{ 100 };
    bool m_AutoReconnect{ false };
    int m_AutoReconnectSeconds{ 60 };
    bool m_DoubleClickForOpenDoors{ false };
    bool m_UseFontColor{ false };
    astr_t m_FontColor{ "0x02B2" };
    bool m_UseCharactersFontColor{ false };
    astr_t m_CharactersFontColor{ "0x02B2" };

    int m_DisplayType{ 0 };
    uint32_t m_DisplayFlags{ 0 };
    uint32_t m_DisplayTextColor{ 0 };
    uint32_t m_DisplayBackgroundColor{ 0 };
    astr_t m_DisplayTitleData{ "" };

    bool m_OnOffHotkeys{ true }; // cb_OnOffHotkeys is checked by default
    bool m_PassHotkeys{ false };
    astr_t m_CommandPrefix{ "_" };

    bool m_SortScripts{ false };
    astr_t m_ScriptFilePath{ "" };

    vector<CTypeEntry> m_Types;
    vector<CObjectEntry> m_Objects;
    vector<CFindListEntry> m_FindLists;
    vector<CIgnoreListEntry> m_IgnoreLists;
    vector<CFilterSpeechEntry> m_SpeechFilters;
    vector<CFilterSoundEntry> m_SoundFilters;
    vector<CFilterReplaceEntry> m_Replaces;
    vector<CShopListEntry> m_ShopLists;
    bool m_ShowShoppingProgressBar{ true };
    int m_MaxShopItems{ 0 };
    int m_MaxSellPrice{ 0 };
    int m_ShopDelay{ 450 };
    int m_ShopTimeout{ 30 };
    vector<CDisplayItemEntry> m_DisplayItems;
    vector<CDisplayGroupEntry> m_DisplayGroups;

    vector<CWorldObjectEntry> m_Friends;
    vector<CWorldObjectEntry> m_Enemies;
    bool m_FeelPartyMembersAsFriends{ false };
    vector<CWorldObjectEntry> m_PartyAccept;
    vector<CWorldObjectEntry> m_PartyDecline;
    bool m_PartyAutoAcceptAll{ false };
    bool m_PartyAutoAcceptFriends{ true }; // checked by default (tabagentsparty.ui)
    bool m_PartyAutoDeclineAll{ false };
    bool m_PartyAutoDeclineEnemies{ true }; // checked by default (tabagentsparty.ui)

    vector<CDressSetEntry> m_DressSets;
    bool m_DressUndressBeforeDress{ false };
    bool m_DressDoubleClickForEquipment{ false };

    vector<CHotkeyEntry> m_Hotkeys;

    vector<CMacroEntry> m_Macros;

    std::map<uint32_t, astr_t> m_ProfileMap;

private:
    vector<astr_t> m_Profiles;
};

extern CAssistantConfig g_AssistantConfig;

} // namespace xuoassist
