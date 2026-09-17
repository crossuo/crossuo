// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// AssistantConfig implementation. Every Load*/Save* pair persists one config
// file; the file format is INI (config/configio.h) - one .ini per file,
// indexed record sections + a [Meta] count.

#include "assistantconfig.h"
#include <algorithm>
#include <string.h>

#include <common/fs.h>
#include <common/logging/logging.h>
#include <common/str.h> // wstr_from

#include "../core/assist.h"
#include "../macros/macro.h"
#include "../managers/displaymanager.h"
#include "Party.h" // ::g_Party (party membership for the friends option)
#if !defined(XUO_ASSISTANT_HARNESS)
#include "CrossUO.h" // ::g_Game (the client config accessors)
#endif
#include "../core/world.h"
#include <xuocore/mappedfile.h> // ::CDataReader / ::CDataWriter

namespace xuoassist {

CAssistantConfig g_AssistantConfig;

static const char *CONFIG_DIR = "Config";
static const char *GLOBAL_CONFIG_DIR = "GlobalConfig";

// The shared count section of the repeated-record schema ([Meta] count = N).
static const char *INI_META = "Meta";

static astr_t ini_section(const char *base, int index)
{
    return IniSectionName(base, index);
}

static astr_t to_lower(astr_t text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

static astr_t fs_entry_name(const fs_path &path)
{
    const astr_t full = fs_path_str(path);
    const size_t slash = full.find_last_of('/');
    return (slash == astr_t::npos) ? full : full.substr(slash + 1);
}

CMacroEntry::~CMacroEntry()
{
    for (CMacro *macro : m_Items)
    {
        delete macro;
    }
    m_Items.clear();
}

CAssistantConfig::CAssistantConfig()
{
    memset(&m_Options[0], 0, sizeof(m_Options));
}

// --- paths -----------------------------------------------------------------

fs_path CAssistantConfig::GetRootPath()
{
    return fs_path_current(); // client cwd == binary dir (src/Main.cpp:414)
}

fs_path CAssistantConfig::GetGlobalConfigPath()
{
    return fs_path_join(GetRootPath(), GLOBAL_CONFIG_DIR);
}

fs_path CAssistantConfig::GetConfigPath()
{
    return fs_path_join(GetRootPath(), CONFIG_DIR);
}

fs_path CAssistantConfig::GetProfilePath() const
{
    return fs_path_join(GetConfigPath(), ProfileName);
}

void CAssistantConfig::EnsureDirectories()
{
    fs_path_create(GetConfigPath());
    fs_path_create(GetGlobalConfigPath());
    fs_path_create(GetProfilePath());
}

// --- profiles ---------------------------------------------------------------

void CAssistantConfig::ScanProfiles()
{
    // Was the AssistForm::Init profile scan: every
    // non-hidden subdirectory of Config/ is a profile; serial-looking names
    // ("0x...") feed the profile map.
    m_Profiles.clear();
    LoadProfileMap();

    fs_path configDir = GetConfigPath();
    std::vector<fs_path> entries;
    fs_path_list(configDir, entries);

    bool profileAdded = false;
    for (const fs_path &entry : entries)
    {
        if (!fs_path_is_dir(entry))
        {
            continue;
        }
        const astr_t name = fs_entry_name(entry);
        if (name.empty() || name[0] == '.')
        {
            continue;
        }
        m_Profiles.push_back(name);
        if (name.size() > 2 && name[0] == '0' && name[1] == 'x')
        {
            const uint32_t serial = CAssistant::TextToSerial(name);
            if (serial != 0u && m_ProfileMap.find(serial) == m_ProfileMap.end())
            {
                m_ProfileMap.insert(std::make_pair(serial, name));
                profileAdded = true;
            }
        }
    }

    std::sort(m_Profiles.begin(), m_Profiles.end());

    if (profileAdded)
    {
        SaveProfileMap();
    }
}

void CAssistantConfig::SetCurrentProfile(const astr_t &name)
{
    const astr_t nameLower = to_lower(name);
    if (to_lower(ProfileName) == nameLower)
    {
        return;
    }
    for (const astr_t &profile : m_Profiles)
    {
        if (to_lower(profile) == nameLower)
        {
            ProfileName = profile;
            return;
        }
    }
    ProfileName = name;
}

bool CAssistantConfig::LoadProfileBySerial(uint32_t serial)
{
    LoadProfileMap();
    std::map<uint32_t, astr_t>::iterator it = m_ProfileMap.find(serial);
    if (it != m_ProfileMap.end())
    {
        SetCurrentProfile(it->second);
        return true;
    }
    return false;
}

void CAssistantConfig::SaveProfileMap()
{
    // Was AssistForm::SaveProfileMap -> GlobalConfig/Profiles.ini.
    EnsureDirectories();
    if (g_Connected && g_Player != nullptr)
    {
        m_ProfileMap[g_PlayerSerial] = ProfileName;
    }

    CIniFile ini;
    int index = 0;
    for (std::map<uint32_t, astr_t>::iterator i = m_ProfileMap.begin(); i != m_ProfileMap.end(); ++i)
    {
        const astr_t section = ini_section("Profile", index++);
        ini.Set(section, "serial", CAssistant::SerialToText(i->first));
        ini.Set(section, "file", i->second);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "Profiles.ini"));
}

void CAssistantConfig::LoadProfileMap()
{
    // Was AssistForm::LoadProfileMap.
    m_ProfileMap.clear();
    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "Profiles.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Profile", i);
        const uint32_t serial = CAssistant::TextToSerial(ini.Get(section, "serial"));
        m_ProfileMap[serial] = ini.Get(section, "file");
    }
}

bool CAssistantConfig::LoadProfile()
{
    // Was AssistForm::LoadCurrentProfile.
    if (ProfileName.empty())
    {
        return false;
    }

    EnsureDirectories();
    LoadMainOptions();
    LoadObjects();
    LoadHotkeys();
    LoadDress();
    LoadParty();
    LoadFriends();
    LoadEnemies();
    LoadMacros();

    // The original form applied the fast-rotation / stamina-check host values right
    // after the load; keep them in the same place.
    if (!FastRotation() || !EnabledFeatureFastRotation())
    {
        g_Game.ValueInt(VKI_FAST_ROTATION, 0);
    }
    else
    {
        g_Game.ValueInt(VKI_FAST_ROTATION, 1);
    }

    if (!IgnoreStaminaCheck() || !EnabledFeatureIgnoreStaminaCheck())
    {
        g_Game.ValueInt(VKI_IGNORE_STAMINA_CHECK, 0);
    }
    else
    {
        g_Game.ValueInt(VKI_IGNORE_STAMINA_CHECK, 1);
    }

    g_Assistant.ClientPrint("Profile '" + ProfileName + "' loaded!");
    return true;
}

bool CAssistantConfig::SaveProfile()
{
    // Was AssistForm::SaveCurrentProfile. Note the
    // original did NOT save Macros.ini here (macros save on list change) and
    // did not save the profile map; keep both behaviors.
    if (ProfileName.empty())
    {
        return false;
    }

    EnsureDirectories();
    SaveMainOptions();
    SaveObjects();
    SaveHotkeys();
    SaveDress();
    SaveParty();
    SaveFriends();
    SaveEnemies();

    g_Assistant.ClientPrint("Configuration saved.");
    return true;
}

// --- Options.ini -------------------------------------------------------------

bool CAssistantConfig::GetFontColor(uint16_t &value) const
{
    bool result = m_UseFontColor;
    if (!m_FontColor.empty())
    {
        value = CAssistant::TextToGraphic(m_FontColor);
    }
    else
    {
        value = 0x02B2;
    }
    return result;
}

void CAssistantConfig::SetFontColor(const bool &state, uint16_t value)
{
    if (value > 0x0400)
    {
        value = 0x02B2;
    }
    m_UseFontColor = state;
    m_FontColor = CAssistant::GraphicToText(value);
}

bool CAssistantConfig::GetCharactersFontColor(uint16_t &value) const
{
    bool result = m_UseCharactersFontColor;
    if (!m_CharactersFontColor.empty())
    {
        value = CAssistant::TextToGraphic(m_CharactersFontColor);
    }
    else
    {
        value = 0x02B2;
    }
    return result;
}

void CAssistantConfig::SetCharactersFontColor(const bool &state, uint16_t value)
{
    if (value > 0x0400)
    {
        value = 0x02B2;
    }
    m_UseCharactersFontColor = state;
    m_CharactersFontColor = CAssistant::GraphicToText(value);
}

void CAssistantConfig::LoadMainOptions()
{
    // Was AssistForm::LoadCurrentProfile's Options.ini block dispatching into
    // TabMain/TabDisplay/TabHotkeys/TabScripts::LoadConfig.
    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Options.ini")))
    {
        return;
    }

    if (ini.Has("main", "flags"))
    {
        const uint32_t flags = (uint32_t)ini.GetInt("main", "flags", 0);
        for (int i = 0; i < CLBI_COUNT; i++)
        {
            m_Options[i] = (flags & (1 << i)) != 0u;
        }
    }
    if (ini.Has("main", "messageslevel"))
    {
        m_MessagesLevel = ini.GetInt("main", "messageslevel", 0);
    }
    if (ini.Has("main", "searchdistance"))
    {
        m_SearchItemsDistance = ini.GetInt("main", "searchdistance", 0);
    }
    if (ini.Has("main", "usedistance"))
    {
        m_UseItemsDistance = ini.GetInt("main", "usedistance", 0);
    }
    if (ini.Has("main", "opencorpsedistance"))
    {
        m_OpenCorpsesDistance = ini.GetInt("main", "opencorpsedistance", 0);
    }
    if (ini.Has("main", "corpsekeepdelay"))
    {
        m_CorpseKeepDelay = ini.GetInt("main", "corpsekeepdelay", 0);
    }
    if (ini.Has("main", "waittargetsdelay"))
    {
        m_WaitTargetsDelay = ini.GetInt("main", "waittargetsdelay", 0);
    }
    if (ini.Has("main", "moveitemsdelay"))
    {
        m_MoveItemsDelay = ini.GetInt("main", "moveitemsdelay", 0);
    }
    if (ini.Has("main", "useitemsdelay"))
    {
        m_UseItemsDelay = ini.GetInt("main", "useitemsdelay", 0);
    }
    if (ini.Has("main", "windowsopacity"))
    {
        m_WindowsOpacity = ini.GetInt("main", "windowsopacity", 0);
    }
    if (ini.Has("main", "usefontcolor"))
    {
        m_UseFontColor = ini.GetBool("main", "usefontcolor", false);
    }
    if (ini.Has("main", "fontcolor"))
    {
        m_FontColor = ini.Get("main", "fontcolor");
    }
    if (ini.Has("main", "usecharactersfontcolor"))
    {
        m_UseCharactersFontColor = ini.GetBool("main", "usecharactersfontcolor", false);
    }
    if (ini.Has("main", "charactersfontcolor"))
    {
        m_CharactersFontColor = ini.Get("main", "charactersfontcolor");
    }
    if (ini.Has("main", "autoreconnect"))
    {
        m_AutoReconnect = ini.GetBool("main", "autoreconnect", false);
    }
    if (ini.Has("main", "autoreconnecttimer"))
    {
        m_AutoReconnectSeconds = ini.GetInt("main", "autoreconnecttimer", 0);
    }
    if (ini.Has("main", "doubleclickforopendoors"))
    {
        m_DoubleClickForOpenDoors = ini.GetBool("main", "doubleclickforopendoors", false);
    }

    // TabDisplay::LoadConfig
    if (ini.Has("display", "enabled"))
    {
        m_DisplayEnabled = ini.GetBool("display", "enabled", false);
    }
    if (ini.Has("display", "titledata"))
    {
        m_DisplayTitleData = ini.Get("display", "titledata");
    }

    // // the INI schema stores only commandprefix
    if (ini.Has("hotkeys", "passhotkeys"))
    {
        m_PassHotkeys = ini.GetBool("hotkeys", "passhotkeys", false);
    }
    if (ini.Has("hotkeys", "commandprefix"))
    {
        m_CommandPrefix = ini.Get("hotkeys", "commandprefix");
    }

    // TabScripts::LoadConfig
    if (ini.Has("scripts", "sortscripts"))
    {
        m_SortScripts = ini.GetBool("scripts", "sortscripts", false);
    }
    if (ini.Has("scripts", "scriptfile"))
    {
        // The script path element
        m_ScriptFilePath = ini.Get("scripts", "scriptfile");
    }
}

void CAssistantConfig::SaveMainOptions()
{
    // Was AssistForm::SaveMainOptions: the main/display/hotkeys/scripts
    // records.
    EnsureDirectories();

    CIniFile ini;

    uint32_t flags = 0;
    for (int i = 0; i < CLBI_COUNT; i++)
    {
        if (m_Options[i])
        {
            flags |= 1 << i;
        }
    }
    ini.SetInt("main", "flags", (int)flags);
    ini.SetInt("main", "messageslevel", m_MessagesLevel);
    ini.SetInt("main", "searchdistance", m_SearchItemsDistance);
    ini.SetInt("main", "usedistance", m_UseItemsDistance);
    ini.SetInt("main", "opencorpsedistance", m_OpenCorpsesDistance);
    ini.SetInt("main", "corpsekeepdelay", m_CorpseKeepDelay);
    ini.SetInt("main", "waittargetsdelay", m_WaitTargetsDelay);
    ini.SetInt("main", "moveitemsdelay", m_MoveItemsDelay);
    ini.SetInt("main", "useitemsdelay", m_UseItemsDelay);
    ini.SetInt("main", "windowsopacity", m_WindowsOpacity);
    ini.SetBool("main", "usefontcolor", m_UseFontColor);
    ini.Set("main", "fontcolor", m_FontColor);
    ini.SetBool("main", "usecharactersfontcolor", m_UseCharactersFontColor);
    ini.Set("main", "charactersfontcolor", m_CharactersFontColor);
    ini.SetBool("main", "autoreconnect", m_AutoReconnect);
    ini.SetInt("main", "autoreconnecttimer", m_AutoReconnectSeconds);
    ini.SetBool("main", "doubleclickforopendoors", m_DoubleClickForOpenDoors);

    // TabDisplay::SaveConfig
    ini.SetBool("display", "enabled", m_DisplayEnabled);
    ini.Set("display", "titledata", m_DisplayTitleData);

    // TabHotkeys::SaveConfig
    ini.SetBool("hotkeys", "passhotkeys", m_PassHotkeys);
    ini.Set("hotkeys", "commandprefix", m_CommandPrefix);

    // TabScripts::SaveConfig
    ini.SetBool("scripts", "sortscripts", m_SortScripts);
    ini.Set("scripts", "scriptfile", m_ScriptFilePath);

    ini.Save(fs_path_join(GetProfilePath(), "Options.ini"));
}

// --- main option gates (TabMain getters, feature-flag gated) ------------------

#define XA_CONFIG_OPTION_GETTER(name, featureEnabled, index)                                       \
    bool CAssistantConfig::name() const                                                                  \
    {                                                                                              \
        if (!(featureEnabled))                                                                     \
        {                                                                                          \
            return false;                                                                          \
        }                                                                                          \
        return m_Options[index];                                                                   \
    }

XA_CONFIG_OPTION_GETTER(CorpsesAutoopen, EnabledFeatureCorpsesAutoopen(), CLBI_CORPSES_AUTOOPEN)
XA_CONFIG_OPTION_GETTER(
    StealthStepCounter, EnabledFeatureStealthStepCounter(), CLBI_STEALTH_STEP_COUNTER)
XA_CONFIG_OPTION_GETTER(SoundFilter, EnabledFeatureSoundFilter(), CLBI_SOUND_FILTER)
XA_CONFIG_OPTION_GETTER(LightFilter, EnabledFeatureLightFilter(), CLBI_FILTER_LIGHT)
XA_CONFIG_OPTION_GETTER(WeatherFilter, EnabledFeatureWeatherFilter(), CLBI_FILTER_WEATHER)
XA_CONFIG_OPTION_GETTER(SeasonFilter, EnabledFeatureSeasonFilter(), CLBI_FILTER_SEASON)
XA_CONFIG_OPTION_GETTER(NoDeathScreen, EnabledFeatureNoDeathScreen(), CLBI_NO_DEATH_SCREEN)
XA_CONFIG_OPTION_GETTER(AutoOpenDoors, EnabledFeatureAutoOpenDoors(), CLBI_OPEN_DOORS)
XA_CONFIG_OPTION_GETTER(Tracker, EnabledFeatureTracker(), CLBI_TRACKER)
XA_CONFIG_OPTION_GETTER(FastRotation, EnabledFeatureFastRotation(), CLBI_FAST_ROTATION)
XA_CONFIG_OPTION_GETTER(
    RecurseContainersSearch, EnabledFeatureRecurseContainersSearch(), CLBI_RECURSE_CONTAINERS_SEARCH)
XA_CONFIG_OPTION_GETTER(Autostart, EnabledFeatureAutostart(), CLBI_AUTOSTART)
XA_CONFIG_OPTION_GETTER(
    IgnoreStaminaCheck, EnabledFeatureIgnoreStaminaCheck(), CLBI_IGNORE_STAMINA_CHECK)
XA_CONFIG_OPTION_GETTER(MinimizeToTray, EnabledFeatureMinimizeToTray(), CLBI_MINIMIZE_TO_TRAY)
XA_CONFIG_OPTION_GETTER(SoundEcho, EnabledFeatureSoundEcho(), CLBI_SOUND_ECHO)
XA_CONFIG_OPTION_GETTER(AnimationEcho, EnabledFeatureAnimationEcho(), CLBI_ANIMATION_ECHO)
XA_CONFIG_OPTION_GETTER(EffectEcho, EnabledFeatureEffectEcho(), CLBI_EFFECT_ECHO)
XA_CONFIG_OPTION_GETTER(SpeechFilter, EnabledFeatureSpeechFilter(), CLBI_FILTER_SPEECH)
XA_CONFIG_OPTION_GETTER(TextReplace, EnabledFeatureTextReplace(), CLBI_TEXT_REPLACE)
// Not feature-gated:
bool CAssistantConfig::ObjectInspector() const
{
    return m_Options[CLBI_OBJECT_INSPECTOR];
}
XA_CONFIG_OPTION_GETTER(NoGrayScreen, EnabledFeatureNoDeathScreen(), CLBI_NO_GRAY_SCREEN)
// Not feature-gated:
bool CAssistantConfig::SaveFarObjects() const
{
    return m_Options[CLBI_SAVE_FAR_OBJECTS];
}
bool CAssistantConfig::StayOnTop() const
{
    return m_Options[CLBI_STAY_ON_TOP];
}

void CAssistantConfig::UpdateFeatures()
{
    // Was TabMain::UpdateFeatures: clear every option
    // whose feature flag is not enabled. The original table skipped Object
    // inspector (0) and Save far objects (0); the No gray screen row maps to
    // XAEF_NO_DEATH_SCREEN there and here. The widget checkbox unchecks
    // became direct option clears.
    const uint32_t table[CLBI_COUNT] = {
        XAEF_CORPSES_AUTOOPEN,
        XAEF_STEALTH_STEP_COUNTER,
        XAEF_SOUND_FILTER,
        XAEF_LIGHT_FILTER,
        XAEF_WEATHER_FILTER,
        XAEF_SEASON_FILTER,
        XAEF_NO_DEATH_SCREEN,
        XAEF_AUTO_OPEN_DOORS,
        XAEF_TRACKER,
        XAEF_FAST_ROTATION,
        XAEF_RECURSE_CONTAINERS_SEARCH,
        XAEF_AUTOSTART,
        XAEF_IGNORE_STAMINA_CHECK,
        XAEF_MINIMIZE_TO_TRAY,
        XAEF_SOUND_ECHO,
        XAEF_ANIMATION_ECHO,
        XAEF_EFFECT_ECHO,
        XAEF_SPEECH_FILTER,
        XAEF_TEXT_REPLACE,
        0,                    //Object inspector
        XAEF_NO_DEATH_SCREEN, //No gray screen
        0,                    //Save far objects
    };

    for (int i = 0; i < CLBI_COUNT; i++)
    {
        const uint32_t &flags = table[i];

        if ((flags == 0u) || ((g_EnabledFeature & flags) != 0u))
        {
            continue;
        }

        if (m_Options[i])
        {
            m_Options[i] = false; // the toggle applies immediately
        }
    }
}

// --- list queries --------------------------------------------------------------

uint16_t CAssistantConfig::SeekType(const astr_t &text) const
{
    // Was TabListsTypes::SeekType.
    for (const CTypeEntry &entry : m_Types)
    {
        if (to_lower(entry.Name) == text)
        {
            return entry.Type;
        }
    }
    return 0;
}

uint32_t CAssistantConfig::SeekObject(const astr_t &text) const
{
    // Was TabListsObjects::SeekObject.
    for (const CObjectEntry &entry : m_Objects)
    {
        if (to_lower(entry.Name) == text)
        {
            return entry.Serial;
        }
    }
    return 0;
}

CFindListEntry *CAssistantConfig::GetFindList(const astr_t &name)
{
    for (CFindListEntry &entry : m_FindLists)
    {
        if (to_lower(entry.Name) == to_lower(name))
        {
            return &entry;
        }
    }
    return nullptr;
}

CIgnoreListEntry *CAssistantConfig::GetIgnoreList(const astr_t &name)
{
    for (CIgnoreListEntry &entry : m_IgnoreLists)
    {
        if (to_lower(entry.Name) == to_lower(name))
        {
            return &entry;
        }
    }
    return nullptr;
}

bool CAssistantConfig::InFriendList(uint32_t serial) const
{
    // Was TabListsFriends::InFriendList.
    if (m_FeelPartyMembersAsFriends && g_Party.Contains((int)serial))
    {
        return true;
    }
    for (const CWorldObjectEntry &entry : m_Friends)
    {
        if (entry.Enabled && entry.Serial == serial)
        {
            return true;
        }
    }
    return false;
}

bool CAssistantConfig::InEnemyList(uint32_t serial) const
{
    // Was TabListsEnemies::InEnemyList.
    for (const CWorldObjectEntry &entry : m_Enemies)
    {
        if (entry.Enabled && entry.Serial == serial)
        {
            return true;
        }
    }
    return false;
}

void CAssistantConfig::AddType(const astr_t &name, const astr_t &type)
{
    // Was TabListsTypes::AddType (minus the target-request UI branch; the
    // empty-type "from target" flow is UI and stays there).
    const astr_t lowerName = to_lower(name);
    for (CTypeEntry &entry : m_Types)
    {
        if (to_lower(entry.Name) == lowerName)
        {
            entry.Name = name;
            entry.Type = CAssistant::TextToGraphic(type);
            SaveTypes();
            return;
        }
    }
    CTypeEntry entry;
    entry.Name = name;
    entry.Type = CAssistant::TextToGraphic(type);
    m_Types.push_back(entry);
    SaveTypes();
}

void CAssistantConfig::RemoveType(const astr_t &name)
{
    // Was TabListsTypes::RemoveType.
    const astr_t lowerName = to_lower(name);
    for (vector<CTypeEntry>::iterator it = m_Types.begin(); it != m_Types.end();)
    {
        if (to_lower(it->Name) == lowerName)
        {
            it = m_Types.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CAssistantConfig::AddObject(const astr_t &name, const astr_t &serial)
{
    // Was TabListsObjects::AddObject (minus the target-request UI branch).
    const astr_t lowerName = to_lower(name);
    for (CObjectEntry &entry : m_Objects)
    {
        if (to_lower(entry.Name) == lowerName)
        {
            entry.Name = name;
            entry.Serial = CAssistant::TextToSerial(serial);
            return;
        }
    }
    CObjectEntry entry;
    entry.Name = name;
    entry.Serial = CAssistant::TextToSerial(serial);
    m_Objects.push_back(entry);
}

void CAssistantConfig::RemoveObject(const astr_t &name)
{
    // Was TabListsObjects::RemoveObject.
    const astr_t lowerName = to_lower(name);
    for (vector<CObjectEntry>::iterator it = m_Objects.begin(); it != m_Objects.end();)
    {
        if (to_lower(it->Name) == lowerName)
        {
            it = m_Objects.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// --- party agent -----------------------------------------------------------------

bool CAssistantConfig::PartyInviteCanBeAutoAccepted(uint32_t serial) const
{
    // Was TabAgentsParty::PartyInviteCanBeAutoAccepted.
    if (m_PartyAutoAcceptAll)
    {
        return true;
    }
    if (m_PartyAutoAcceptFriends && InFriendList(serial))
    {
        return true;
    }
    for (const CWorldObjectEntry &entry : m_PartyAccept)
    {
        if (entry.Serial == serial && entry.Enabled)
        {
            return true;
        }
    }
    return false;
}

bool CAssistantConfig::PartyInviteCanBeAutoDeclined(uint32_t serial) const
{
    // Was TabAgentsParty::PartyInviteCanBeAutoDeclined.
    if (m_PartyAutoDeclineAll)
    {
        return true;
    }
    if (m_PartyAutoDeclineEnemies && InEnemyList(serial))
    {
        return true;
    }
    for (const CWorldObjectEntry &entry : m_PartyDecline)
    {
        if (entry.Serial == serial && entry.Enabled)
        {
            return true;
        }
    }
    return false;
}

// --- filters ----------------------------------------------------------------------

bool CAssistantConfig::CheckSpeechInFilter(const astr_t &text, const uint16_t &color) const
{
    // Was TabFiltersSpeech::CheckSpeechInFilter.
    if (!SpeechFilter())
    {
        return false;
    }
    for (const CFilterSpeechEntry &item : m_SpeechFilters)
    {
        const uint16_t itemColor = CAssistant::TextToGraphic(item.Color);
        if (itemColor == 0xFFFF || itemColor == color)
        {
            bool found = false;
            if (item.FullMatch)
            {
                found = (item.Text == text);
            }
            else
            {
                found = text.find(item.Text) != astr_t::npos;
            }
            if (found)
            {
                return true;
            }
        }
    }
    return false;
}

bool CAssistantConfig::CheckReplaceText(
    const astr_t &text,
    const uint32_t &serial,
    const uint16_t &color,
    const bool &unicode,
    astr_t &replacedText,
    uint16_t &replacedColor) const
{
    // Was TabFiltersReplaces::CheckReplaceText. On a match the replacement is
    // returned instead of queuing a reprint packet: the caller displays it
    // directly (synchronously inside the talk handler it already runs in, so
    // no packet re-processing round-trip).
    if (!TextReplace())
    {
        return false;
    }
    int type = RTT_ASCII;
    if (unicode)
    {
        type = RTT_UNICODE;
    }
    for (const CFilterReplaceEntry &item : m_Replaces)
    {
        if (item.Type != RTT_ALL && item.Type != type)
        {
            continue;
        }
        uint16_t itemColor = CAssistant::TextToGraphic(item.OriginalColor);
        if (itemColor != 0xFFFF && itemColor != color)
        {
            continue;
        }
        bool found = false;
        if (item.FullMatch)
        {
            found = (item.OriginalText == text);
        }
        else
        {
            found = text.find(item.OriginalText) != astr_t::npos;
        }
        if (!found)
        {
            continue;
        }

        bool canBeReplaced = true;
        if (item.Mode == RTM_FRIENDS_AND_YOU)
        {
            canBeReplaced = (serial == g_PlayerSerial || InFriendList(serial));
        }
        else if (item.Mode == RTM_ENEMIES)
        {
            canBeReplaced = InEnemyList(serial);
        }
        else if (item.Mode == RTM_OTHERS)
        {
            canBeReplaced = (serial != g_PlayerSerial && !InFriendList(serial) &&
                             !InEnemyList(serial));
        }

        if (canBeReplaced)
        {
            replacedColor = CAssistant::TextToGraphic(item.ReplacedColor);
            if (replacedColor == 0xFFFF)
            {
                replacedColor = color;
            }
            replacedText = item.ReplacedText;
            return true;
        }
    }
    return false;
}

bool CAssistantConfig::CheckSoundInFilter(const uint16_t &index) const
{
    // Was TabFiltersSound::CheckSoundInFilter.
    if (!SoundFilter())
    {
        return false;
    }
    for (const CFilterSoundEntry &item : m_SoundFilters)
    {
        if (CAssistant::TextToGraphic(item.Index) == index)
        {
            return true;
        }
    }
    return false;
}

// --- distance / delay helpers ------------------------------------------------------

int CAssistantConfig::DistanceFromText(astr_t text)
{
    // Was AssistForm::DistanceFromText.
    text = to_lower(text);
    if (text == "finddistance")
    {
        return m_SearchItemsDistance;
    }
    if (text == "usedistance")
    {
        return m_UseItemsDistance;
    }
    if (text == "opencorpsedistance")
    {
        return m_OpenCorpsesDistance;
    }
    int result = atoi(text.c_str());
    if (result < 0 || text.empty())
    {
        result = m_SearchItemsDistance;
    }
    return result;
}

int CAssistantConfig::DelayFromText(astr_t text)
{
    // Was AssistForm::DelayFromText.
    text = to_lower(text);
    if (text == "moveitemdelay")
    {
        return m_MoveItemsDelay;
    }
    if (text == "waittargetdelay")
    {
        return m_WaitTargetsDelay;
    }
    if (text == "useitemdelay")
    {
        return m_UseItemsDelay;
    }
    if (text == "keepcorpsedelay")
    {
        return m_CorpseKeepDelay;
    }
    return atoi(text.c_str());
}

int CAssistantConfig::IsCommandPrefix(const astr_t &text) const
{
    // Was TabHotkeys::IsCommandPrefix.
    int result = 0;
    const astr_t prefix = to_lower(m_CommandPrefix);
    if (!prefix.empty())
    {
        astr_t head = text;
        if (head.size() > prefix.size())
        {
            head.resize(prefix.size());
        }
        if (to_lower(head) == prefix)
        {
            result = (int)prefix.size();
        }
    }
    return result;
}

// --- display items / groups ----------------------------------------------------------

DisplayInfo CAssistantConfig::GetCustomDisplayItemByName(const astr_t &name) const
{
    // Was TabDisplay::GetCustomDisplayItemByName.
    for (const DisplayItemEntry &entry : m_DisplayItems)
    {
        if (to_lower(entry.Name) == to_lower(name))
        {
            return entry.m_Item;
        }
    }
    return DisplayInfo();
}

astr_t CAssistantConfig::GetGroupContent(const astr_t &name) const
{
    // Was TabDisplay::GetGroupContent.
    for (const CDisplayGroupEntry &entry : m_DisplayGroups)
    {
        if (to_lower(entry.Name) == to_lower(name))
        {
            return entry.TitleData;
        }
    }
    return "";
}

// --- dress sets ------------------------------------------------------------------------

CDressSetEntry *CAssistantConfig::GetDressSet(const astr_t &name)
{
    for (CDressSetEntry &entry : m_DressSets)
    {
        if (to_lower(entry.Name) == to_lower(name))
        {
            return &entry;
        }
    }
    return nullptr;
}

CDressSetEntry *CAssistantConfig::AddDressSet(const astr_t &name)
{
    CDressSetEntry *existing = GetDressSet(name);
    if (existing != nullptr)
    {
        return existing;
    }
    CDressSetEntry entry;
    entry.Name = name;
    m_DressSets.push_back(entry);
    return &m_DressSets.back();
}

void CAssistantConfig::ClearDressSet(CDressSetEntry *set)
{
    if (set != nullptr)
    {
        memset(&set->m_Items[0], 0, sizeof(set->m_Items));
    }
}

// --- helpers -------------------------------------------------------------------

// --- Types.ini ------------------------------------------------------------------

void CAssistantConfig::SaveTypes()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CTypeEntry &entry : m_Types)
    {
        const astr_t section = ini_section("Type", index++);
        ini.Set(section, "name", entry.Name);
        ini.Set(section, "id", CAssistant::GraphicToText(entry.Type));
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "Types.ini"));
}

void CAssistantConfig::LoadTypes()
{
    m_Types.clear();

    CIniFile ini;
    if (ini.Load(fs_path_join(GetGlobalConfigPath(), "Types.ini")))
    {
        const int count = ini.GetInt(INI_META, "count", 0);
        for (int i = 0; i < count; i++)
        {
            const astr_t section = ini_section("Type", i);
            CTypeEntry entry;
            entry.Name = ini.Get(section, "name");
            entry.Type = CAssistant::TextToGraphic(ini.Get(section, "id"));
            m_Types.push_back(entry);
        }
    }

    if (m_Types.empty())
    {
        // The original app's default rec-list (TabListsTypes::LoadTypes).
        static const char *defaults[][2] = {
            { "bm", "0x0F7B" }, { "bp", "0x0F7A" }, { "ga", "0x0F84" }, { "gs", "0x0F85" },
            { "mr", "0x0F86" }, { "ns", "0x0F88" }, { "sa", "0x0F8C" }, { "ss", "0x0F8D" },
            { "en", "0x0F87" }, { "bandage", "0x0E21" }, { "bottle", "0x0F0E" },
            { "scroll", "0x0E34" }, { "map", "0x14EB" }, { "gp", "0x0EED" },
        };
        for (const auto &def : defaults)
        {
            CTypeEntry entry;
            entry.Name = def[0];
            entry.Type = CAssistant::TextToGraphic(def[1]);
            m_Types.push_back(entry);
        }
        SaveTypes();
    }
}

// --- FindList.ini ----------------------------------------------------------------

void CAssistantConfig::SaveFindList()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CFindListEntry &entry : m_FindLists)
    {
        const astr_t section = ini_section("FindList", index++);
        ini.Set(section, "name", entry.Name);
        ini.SetInt(section, "items", (int)entry.m_Items.size());
        int itemIndex = 0;
        for (const CFindItem &item : entry.m_Items)
        {
            const astr_t itemSection = section + "Item" + str_from(itemIndex++);
            ini.Set(itemSection, "graphic", item.Graphic);
            ini.Set(itemSection, "color", item.Color);
            ini.Set(itemSection, "comment", item.Comment);
        }
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "FindList.ini"));
}

void CAssistantConfig::LoadFindList()
{
    m_FindLists.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "FindList.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("FindList", i);
        CFindListEntry entry;
        entry.Name = ini.Get(section, "name");
        const int itemCount = ini.GetInt(section, "items", 0);
        for (int j = 0; j < itemCount; j++)
        {
            const astr_t itemSection = section + "Item" + str_from(j);
            CFindItem fi(ini.Get(itemSection, "graphic"), ini.Get(itemSection, "color"));
            fi.Comment = ini.Get(itemSection, "comment");
            entry.m_Items.push_back(fi);
        }
        m_FindLists.push_back(entry);
    }
}

// --- IgnoreList.ini ---------------------------------------------------------------

void CAssistantConfig::SaveIgnoreList()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CIgnoreListEntry &entry : m_IgnoreLists)
    {
        const astr_t section = ini_section("IgnoreList", index++);
        ini.Set(section, "name", entry.Name);
        ini.SetInt(section, "items", (int)entry.m_Items.size());
        int itemIndex = 0;
        for (const CIgnoreItem &item : entry.m_Items)
        {
            const astr_t itemSection = section + "Item" + str_from(itemIndex++);
            ini.Set(itemSection, "serial", item.Serial);
            ini.Set(itemSection, "graphic", item.Graphic);
            ini.Set(itemSection, "color", item.Color);
            ini.Set(itemSection, "comment", item.Comment);
        }
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "IgnoreList.ini"));
}

void CAssistantConfig::LoadIgnoreList()
{
    m_IgnoreLists.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "IgnoreList.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("IgnoreList", i);
        CIgnoreListEntry entry;
        entry.Name = ini.Get(section, "name");
        const int itemCount = ini.GetInt(section, "items", 0);
        for (int j = 0; j < itemCount; j++)
        {
            const astr_t itemSection = section + "Item" + str_from(j);
            CIgnoreItem ii(
                ini.Get(itemSection, "serial"),
                ini.Get(itemSection, "graphic"),
                ini.Get(itemSection, "color"));
            ii.Comment = ini.Get(itemSection, "comment");
            entry.m_Items.push_back(ii);
        }
        m_IgnoreLists.push_back(entry);
    }
}

// --- SpeechFilter.ini ----------------------------------------------------------

void CAssistantConfig::SaveSpeechFilter()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CFilterSpeechEntry &entry : m_SpeechFilters)
    {
        const astr_t section = ini_section("Speech", index++);
        ini.Set(section, "text", entry.Text);
        ini.Set(section, "color", entry.Color);
        ini.SetBool(section, "fullmatch", entry.FullMatch);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "SpeechFilter.ini"));
}

void CAssistantConfig::LoadSpeechFilter()
{
    m_SpeechFilters.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "SpeechFilter.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Speech", i);
        CFilterSpeechEntry entry;
        entry.Text = ini.Get(section, "text");
        entry.Color = ini.Get(section, "color");
        entry.FullMatch = ini.GetBool(section, "fullmatch", false);
        m_SpeechFilters.push_back(entry);
    }
}

// --- TextReplaces.ini ---------------------------------------------------------

void CAssistantConfig::SaveTextReplaces()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CFilterReplaceEntry &entry : m_Replaces)
    {
        const astr_t section = ini_section("Replace", index++);
        ini.Set(section, "originaltext", entry.OriginalText);
        ini.Set(section, "originalcolor", entry.OriginalColor);
        ini.Set(section, "replacedtext", entry.ReplacedText);
        ini.Set(section, "replacedcolor", entry.ReplacedColor);
        ini.SetInt(section, "type", entry.Type);
        ini.SetInt(section, "mode", entry.Mode);
        ini.SetBool(section, "fullmatch", entry.FullMatch);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "TextReplaces.ini"));
}

void CAssistantConfig::LoadTextReplaces()
{
    m_Replaces.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "TextReplaces.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Replace", i);
        CFilterReplaceEntry entry;
        entry.OriginalText = ini.Get(section, "originaltext");
        entry.OriginalColor = ini.Get(section, "originalcolor");
        entry.ReplacedText = ini.Get(section, "replacedtext");
        entry.ReplacedColor = ini.Get(section, "replacedcolor");
        entry.Type = ini.GetInt(section, "type", 0);
        entry.Mode = ini.GetInt(section, "mode", 0);
        entry.FullMatch = ini.GetBool(section, "fullmatch", false);
        m_Replaces.push_back(entry);
    }
}

// --- SoundFilter.ini ------------------------------------------------------------

void CAssistantConfig::SaveSoundFilter()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CFilterSoundEntry &entry : m_SoundFilters)
    {
        const astr_t section = ini_section("Sound", index++);
        ini.Set(section, "index", entry.Index);
        ini.Set(section, "name", entry.Name);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "SoundFilter.ini"));
}

void CAssistantConfig::LoadSoundFilter()
{
    m_SoundFilters.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "SoundFilter.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Sound", i);
        CFilterSoundEntry entry;
        entry.Index = ini.Get(section, "index");
        entry.Name = ini.Get(section, "name");
        m_SoundFilters.push_back(entry);
    }
}

// --- ShopList.ini ------------------------------------------------------------------

void CAssistantConfig::SaveShopList()
{
    EnsureDirectories();

    CIniFile ini;
    ini.SetBool(INI_META, "show_shopping_progress_bar", m_ShowShoppingProgressBar);
    ini.SetInt(INI_META, "max_shop_items", m_MaxShopItems);
    ini.SetInt(INI_META, "max_sell_price", m_MaxSellPrice);
    ini.SetInt(INI_META, "shop_delay", m_ShopDelay);
    ini.SetInt(INI_META, "shop_timeout", m_ShopTimeout);

    int index = 0;
    for (const CShopListEntry &entry : m_ShopLists)
    {
        const astr_t section = ini_section("ShopList", index++);
        ini.Set(section, "name", entry.Name);
        ini.SetInt(section, "items", (int)entry.m_Items.size());
        int itemIndex = 0;
        for (const CShopItemEntry &item : entry.m_Items)
        {
            const astr_t itemSection = section + "Item" + str_from(itemIndex++);
            ini.Set(itemSection, "graphic", item.Graphic);
            ini.Set(itemSection, "color", item.Color);
            ini.Set(itemSection, "name", item.Name);
            ini.SetInt(itemSection, "count", item.Count);
            ini.SetInt(itemSection, "price", item.Price);
            ini.Set(itemSection, "comment", item.Comment);
        }
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "ShopList.ini"));
}

void CAssistantConfig::LoadShopList()
{
    m_ShopLists.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetGlobalConfigPath(), "ShopList.ini")))
    {
        return;
    }

    if (ini.Has(INI_META, "show_shopping_progress_bar"))
    {
        m_ShowShoppingProgressBar = ini.GetBool(INI_META, "show_shopping_progress_bar", true);
    }
    if (ini.Has(INI_META, "max_shop_items"))
    {
        m_MaxShopItems = ini.GetInt(INI_META, "max_shop_items", 0);
    }
    if (ini.Has(INI_META, "max_sell_price"))
    {
        m_MaxSellPrice = ini.GetInt(INI_META, "max_sell_price", 0);
    }
    if (ini.Has(INI_META, "shop_delay"))
    {
        m_ShopDelay = ini.GetInt(INI_META, "shop_delay", 0);
    }
    if (ini.Has(INI_META, "shop_timeout"))
    {
        m_ShopTimeout = ini.GetInt(INI_META, "shop_timeout", 0);
    }

    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("ShopList", i);
        CShopListEntry entry;
        entry.Name = ini.Get(section, "name");
        const int itemCount = ini.GetInt(section, "items", 0);
        for (int j = 0; j < itemCount; j++)
        {
            const astr_t itemSection = section + "Item" + str_from(j);
            CShopItemEntry item;
            item.Graphic = ini.Get(itemSection, "graphic");
            item.Color = ini.Get(itemSection, "color");
            item.Name = ini.Get(itemSection, "name");
            item.Count = ini.GetInt(itemSection, "count", 0);
            item.Price = ini.GetInt(itemSection, "price", 0);
            item.Comment = ini.Get(itemSection, "comment");
            entry.m_Items.push_back(item);
        }
        m_ShopLists.push_back(entry);
    }
}

// --- DisplayItems.ini / DisplayGroups.ini -------------------------------------------

void CAssistantConfig::SaveDisplayItems()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const DisplayItemEntry &entry : m_DisplayItems)
    {
        const DisplayInfo &di = entry.m_Item;
        const astr_t section = ini_section("Item", index++);
        ini.Set(section, "name", entry.Name);
        ini.SetInt(section, "type", (int)di.Type);
        ini.Set(section, "text", di.Text);
        ini.SetInt(section, "minvalue", di.MinValue);
        ini.SetInt(section, "midvalue", di.MidValue);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "DisplayItems.ini"));
}

void CAssistantConfig::LoadDisplayItems()
{
    m_DisplayItems.clear();

    CIniFile ini;
    if (ini.Load(fs_path_join(GetGlobalConfigPath(), "DisplayItems.ini")))
    {
        const int count = ini.GetInt(INI_META, "count", 0);
        for (int i = 0; i < count; i++)
        {
            const astr_t section = ini_section("Item", i);
            if (!ini.Has(section, "name"))
            {
                continue;
            }
            DisplayItemEntry entry;
            entry.Name = ini.Get(section, "name");
            if (ini.Has(section, "type"))
            {
                entry.m_Item.Type = (DISPLAY_ITEM_TYPE)ini.GetInt(section, "type", 0);
            }
            if (ini.Has(section, "text"))
            {
                entry.m_Item.Text = ini.Get(section, "text");
            }
            if (ini.Has(section, "minvalue"))
            {
                entry.m_Item.MinValue = ini.GetInt(section, "minvalue", 0);
            }
            if (ini.Has(section, "midvalue"))
            {
                entry.m_Item.MidValue = ini.GetInt(section, "midvalue", 0);
            }
            m_DisplayItems.push_back(entry);
        }
    }

    if (m_DisplayItems.empty())
    {
        // The original app's defaults (TabDisplay::LoadDisplayItems): one entry per
        // info command.
        for (const DisplayInfo &info : g_DisplayManager.GetCommands())
        {
            if (info.Name.empty() || info.Command.empty())
            {
                continue;
            }
            DisplayInfo di = g_DisplayManager.GetDisplayItemByName(info.Command);
            if (di.Type != DIT_NONE)
            {
                DisplayItemEntry entry;
                entry.Name = info.Command;
                entry.m_Item = di;
                m_DisplayItems.push_back(entry);
            }
        }

        SaveDisplayItems();
    }
}

void CAssistantConfig::SaveDisplayGroups()
{
    EnsureDirectories();

    CIniFile ini;
    int index = 0;
    for (const CDisplayGroupEntry &entry : m_DisplayGroups)
    {
        const astr_t section = ini_section("Group", index++);
        ini.Set(section, "name", entry.Name);
        ini.Set(section, "text", entry.TitleData);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetGlobalConfigPath(), "DisplayGroups.ini"));
}

void CAssistantConfig::LoadDisplayGroups()
{
    m_DisplayGroups.clear();

    CIniFile ini;
    if (ini.Load(fs_path_join(GetGlobalConfigPath(), "DisplayGroups.ini")))
    {
        const int count = ini.GetInt(INI_META, "count", 0);
        for (int i = 0; i < count; i++)
        {
            const astr_t section = ini_section("Group", i);
            CDisplayGroupEntry entry;
            entry.Name = ini.Get(section, "name");
            entry.TitleData = ini.Get(section, "text");
            m_DisplayGroups.push_back(entry);
        }
    }

    if (m_DisplayGroups.empty())
    {
        // The original app's defaults (TabDisplay::LoadDisplayGroups).
        CDisplayGroupEntry allreag;
        allreag.Name = "allreag";
        allreag.TitleData = "{bm} {bp} {ga} {gs} {mr} {ns} {sa} {ss}";
        m_DisplayGroups.push_back(allreag);

        CDisplayGroupEntry def;
        def.Name = "default";
        def.TitleData =
            "UO - {name} ({server}) {barmiddle} {hp_maxhp} {mp_maxmp} {sp_maxsp} {armor} "
            "{weight_maxweight} {gold} {group allreag} {x} {y}";
        m_DisplayGroups.push_back(def);

        SaveDisplayGroups();
    }
}

// --- Objects.ini -----------------------------------------------------------------------

void CAssistantConfig::SaveObjects()
{
    CIniFile ini;
    ini.Set(INI_META, "dressbag", CAssistant::SerialToText(g_DressBag));

    int index = 0;
    for (const CObjectEntry &entry : m_Objects)
    {
        const astr_t section = ini_section("Object", index++);
        ini.Set(section, "name", entry.Name);
        ini.Set(section, "id", CAssistant::SerialToText(entry.Serial));
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetProfilePath(), "Objects.ini"));
}

void CAssistantConfig::LoadObjects()
{
    m_Objects.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Objects.ini")))
    {
        return;
    }
    if (ini.Has(INI_META, "dressbag"))
    {
        g_Assistant.SetDressBag(CAssistant::TextToSerial(ini.Get(INI_META, "dressbag")));
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Object", i);
        CObjectEntry entry;
        entry.Name = ini.Get(section, "name");
        entry.Serial = CAssistant::TextToSerial(ini.Get(section, "id"));
        m_Objects.push_back(entry);
    }
}

// --- Hotkeys.ini ------------------------------------------------------------------------

void CAssistantConfig::SaveHotkeys()
{
    // Was HotkeysForm::Save: records with SDL key codes.
    CIniFile ini;
    int index = 0;
    for (const CHotkeyEntry &entry : m_Hotkeys)
    {
        const astr_t section = ini_section("Hotkey", index++);
        ini.SetInt(section, "type", (int)entry.Type);
        ini.SetInt(section, "action", (int)entry.Action);
        ini.SetBool(section, "running_mode", entry.RunningMode);
        ini.SetInt(section, "key", (int)entry.Key);
        ini.SetInt(section, "modifiers", (int)entry.Modifiers);
        ini.Set(section, "name", entry.ScriptOrMacroName);
        ini.Set(section, "external_code", entry.ExternalCode);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetProfilePath(), "Hotkeys.ini"));
}

bool CAssistantConfig::LoadHotkeys()
{
    // reads the INI schema only
    m_Hotkeys.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Hotkeys.ini")))
    {
        return false;
    }

    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Hotkey", i);
        CHotkeyEntry entry;
        entry.Key = (uint32_t)strtoul(ini.Get(section, "key").c_str(), nullptr, 10);
        entry.Modifiers = (uint8_t)strtoul(ini.Get(section, "modifiers").c_str(), nullptr, 10);
        entry.Type = (uint32_t)strtoul(ini.Get(section, "type").c_str(), nullptr, 10);
        entry.Action = (uint32_t)strtoul(ini.Get(section, "action").c_str(), nullptr, 10);
        entry.RunningMode = ini.GetBool(section, "running_mode", false);
        entry.ScriptOrMacroName = ini.Get(section, "name");
        entry.ExternalCode = ini.Get(section, "external_code");
        m_Hotkeys.push_back(entry);
    }

    return true;
}

// --- Party.ini ----------------------------------------------------------------------------

void CAssistantConfig::SaveParty()
{
    CIniFile ini;
    ini.SetBool(INI_META, "autoacceptall", m_PartyAutoAcceptAll);
    ini.SetBool(INI_META, "autoacceptfriends", m_PartyAutoAcceptFriends);
    ini.SetBool(INI_META, "autodeclineall", m_PartyAutoDeclineAll);
    ini.SetBool(INI_META, "autodeclineenemies", m_PartyAutoDeclineEnemies);

    int index = 0;
    for (const CWorldObjectEntry &entry : m_PartyAccept)
    {
        const astr_t section = ini_section("Accept", index++);
        ini.Set(section, "id", CAssistant::SerialToText(entry.Serial));
        ini.Set(section, "name", entry.Name);
        ini.SetBool(section, "enabled", entry.Enabled);
    }
    index = 0;
    for (const CWorldObjectEntry &entry : m_PartyDecline)
    {
        const astr_t section = ini_section("Decline", index++);
        ini.Set(section, "id", CAssistant::SerialToText(entry.Serial));
        ini.Set(section, "name", entry.Name);
        ini.SetBool(section, "enabled", entry.Enabled);
    }
    ini.SetInt(INI_META, "count", (int)(m_PartyAccept.size() + m_PartyDecline.size()));

    ini.Save(fs_path_join(GetProfilePath(), "Party.ini"));
}

void CAssistantConfig::LoadParty()
{
    m_PartyAccept.clear();
    m_PartyDecline.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Party.ini")))
    {
        return;
    }
    if (ini.Has(INI_META, "autoacceptall"))
    {
        m_PartyAutoAcceptAll = ini.GetBool(INI_META, "autoacceptall", false);
    }
    if (ini.Has(INI_META, "autoacceptfriends"))
    {
        m_PartyAutoAcceptFriends = ini.GetBool(INI_META, "autoacceptfriends", false);
    }
    if (ini.Has(INI_META, "autodeclineall"))
    {
        m_PartyAutoDeclineAll = ini.GetBool(INI_META, "autodeclineall", false);
    }
    if (ini.Has(INI_META, "autodeclineenemies"))
    {
        m_PartyAutoDeclineEnemies = ini.GetBool(INI_META, "autodeclineenemies", false);
    }

    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        CWorldObjectEntry entry;
        for (int pass = 0; pass < 2; pass++)
        {
            const astr_t section =
                pass == 0 ? ini_section("Accept", i) : ini_section("Decline", i);
            if (!ini.Has(section, "id"))
            {
                continue;
            }
            entry.Serial = CAssistant::TextToSerial(ini.Get(section, "id"));
            entry.Name = ini.Get(section, "name");
            entry.Enabled = ini.GetBool(section, "enabled", false);
            if (pass == 0)
            {
                m_PartyAccept.push_back(entry);
            }
            else
            {
                m_PartyDecline.push_back(entry);
            }
        }
    }
}

// --- Friends.ini ---------------------------------------------------------------------------

void CAssistantConfig::SaveFriends()
{
    CIniFile ini;
    ini.SetBool(INI_META, "feel_party_members_as_friends", m_FeelPartyMembersAsFriends);

    int index = 0;
    for (const CWorldObjectEntry &entry : m_Friends)
    {
        const astr_t section = ini_section("Friend", index++);
        ini.Set(section, "id", CAssistant::SerialToText(entry.Serial));
        ini.Set(section, "name", entry.Name);
        ini.SetBool(section, "enabled", entry.Enabled);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetProfilePath(), "Friends.ini"));
}

void CAssistantConfig::LoadFriends()
{
    m_Friends.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Friends.ini")))
    {
        return;
    }
    if (ini.Has(INI_META, "feel_party_members_as_friends"))
    {
        m_FeelPartyMembersAsFriends = ini.GetBool(INI_META, "feel_party_members_as_friends", false);
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Friend", i);
        CWorldObjectEntry entry;
        entry.Serial = CAssistant::TextToSerial(ini.Get(section, "id"));
        entry.Name = ini.Get(section, "name");
        entry.Enabled = ini.GetBool(section, "enabled", false);
        m_Friends.push_back(entry);
    }
}

// --- Enemies.ini ----------------------------------------------------------------------------

void CAssistantConfig::SaveEnemies()
{
    CIniFile ini;
    int index = 0;
    for (const CWorldObjectEntry &entry : m_Enemies)
    {
        const astr_t section = ini_section("Enemy", index++);
        ini.Set(section, "id", CAssistant::SerialToText(entry.Serial));
        ini.Set(section, "name", entry.Name);
        ini.SetBool(section, "enabled", entry.Enabled);
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetProfilePath(), "Enemies.ini"));
}

void CAssistantConfig::LoadEnemies()
{
    m_Enemies.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Enemies.ini")))
    {
        return;
    }
    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Enemy", i);
        CWorldObjectEntry entry;
        entry.Serial = CAssistant::TextToSerial(ini.Get(section, "id"));
        entry.Name = ini.Get(section, "name");
        entry.Enabled = ini.GetBool(section, "enabled", false);
        m_Enemies.push_back(entry);
    }
}

// --- Macros.ini -----------------------------------------------------------------------------

void CAssistantConfig::SaveMacros()
{
    CIniFile ini;
    int index = 0;
    for (const CMacroEntry &entry : m_Macros)
    {
        const astr_t section = ini_section("Macro", index++);
        ini.Set(section, "name", entry.Name);
        ini.SetInt(section, "actions", (int)entry.m_Items.size());
        int actionIndex = 0;
        for (CMacro *macro : entry.m_Items)
        {
            macro->Save(ini, section + "Action" + str_from(actionIndex++));
        }
    }
    ini.SetInt(INI_META, "count", index);

    ini.Save(fs_path_join(GetProfilePath(), "Macros.ini"));
}

bool CAssistantConfig::LoadMacros()
{
    // Was TabMacros::Load.
    for (CMacroEntry &entry : m_Macros)
    {
        entry.m_Items.clear();
    }
    m_Macros.clear();

    CIniFile ini;
    if (!ini.Load(fs_path_join(GetProfilePath(), "Macros.ini")))
    {
        return false;
    }

    const int count = ini.GetInt(INI_META, "count", 0);
    for (int i = 0; i < count; i++)
    {
        const astr_t section = ini_section("Macro", i);
        CMacroEntry entry;
        entry.Name = ini.Get(section, "name");
        const int actionCount = ini.GetInt(section, "actions", 0);
        for (int j = 0; j < actionCount; j++)
        {
            CMacro *action = CMacro::Load(ini, section + "Action" + str_from(j));
            if (action != nullptr)
            {
                entry.m_Items.push_back(action);
            }
        }
        m_Macros.push_back(entry);
    }

    return true;
}

// --- Dress.coa -----------------------------------------------------------------------------
//
// Custom little-endian binary format written through ::CDataWriter
// (xuocore/mappedfile.h). Layout:
//   uint32 version (=0), int32 setsCount, uint8 undressBefore,
//   per set: string name, uint8 layerCount, per layer { uint8 layer, uint32 serial },
//   uint8 doubleClickForEquipment.
// Dress.coa stores: int8 length (= utf8 length + 1), the utf8 bytes, one
// 0x00 byte; the reader accepts a full int32 length (tolerating writer
// asymmetry) so old and new files load identically (names that fail the
// bounds check come back empty and the set is skipped).

static const int XA_DRESS_MAX_LAYERS = 25;

bool CAssistantConfig::SaveDress()
{
    CDataWriter writer(0x1000, true);

    writer.WriteUInt32LE(0); // version

    writer.WriteInt32LE((int32_t)m_DressSets.size());
    writer.WriteUInt8(m_DressUndressBeforeDress ? 1 : 0);

    for (const CDressSetEntry &entry : m_DressSets)
    {
        // CFileObject::WriteString byte layout (see above).
        const astr_t utf8 = entry.Name;
        writer.WriteUInt8((uint8_t)(utf8.size() + 1));
        if (!utf8.empty())
        {
            writer.WriteDataLE((const uint8_t *)utf8.data(), utf8.size());
        }
        writer.WriteUInt8(0);

        int layersCount = 0;
        for (int layer = 0; layer < XA_DRESS_MAX_LAYERS; layer++)
        {
            if (entry.m_Items[layer] != 0u)
            {
                layersCount++;
            }
        }

        writer.WriteUInt8((uint8_t)layersCount);

        for (int layer = 0; layer < XA_DRESS_MAX_LAYERS; layer++)
        {
            const uint32_t serial = entry.m_Items[layer];
            if (serial != 0u)
            {
                writer.WriteUInt8((uint8_t)layer);
                writer.WriteUInt32LE(serial);
            }
        }
    }

    writer.WriteUInt8(m_DressDoubleClickForEquipment ? 1 : 0);

    const vector<uint8_t> bytes = writer.Data();
    // Dress.coa goes through the same crash-safe temp+rename path as the INI
    // writers
    return AtomicFileWrite(fs_path_join(GetProfilePath(), "Dress.coa"), &bytes[0], bytes.size());
}

bool CAssistantConfig::LoadDress()
{
    m_DressSets.clear();

    fs_path path = fs_path_join(GetProfilePath(), "Dress.coa");
    size_t size = 0;
    unsigned char *data = fs_map(path, &size);
    if (data == nullptr)
    {
        return false;
    }

    CDataReader reader(data, size);

    reader.ReadUInt32LE(); // version (was file.ReadUInt(); unchecked, like the original)

    const int count = reader.ReadInt32LE();
    m_DressUndressBeforeDress = reader.ReadUInt8() != 0u;

    for (int i = 0; i < count && !reader.IsEOF(); i++)
    {
        // CFileObject::ReadString (see the format comment): int32 length +
        // bounds check, empty result tolerated.
        astr_t name;
        if (!reader.IsEOF())
        {
            const int len = reader.ReadInt32LE();
            const size_t remaining = size - (reader.Ptr - reader.Start);
            if (len > 0 && (size_t)len <= remaining)
            {
                name = astr_t((const char *)reader.Ptr, len);
            }
        }

        if (name.empty())
        {
            continue;
        }

        CDressSetEntry entry;
        entry.Name = name;

        const uint8_t layerCount = reader.ReadUInt8();
        for (int j = 0; j < layerCount && !reader.IsEOF(); j++)
        {
            const uint8_t layer = reader.ReadUInt8();
            const uint32_t serial = reader.ReadUInt32LE();
            if (layer < XA_DRESS_MAX_LAYERS)
            {
                entry.m_Items[layer] = serial;
            }
        }

        m_DressSets.push_back(entry);
    }

    if (!reader.IsEOF())
    {
        m_DressDoubleClickForEquipment = reader.ReadUInt8() != 0u;
    }

    fs_unmap(data, size);
    return true;
}

// --- globals load / save ------------------------------------------------------------------------

void CAssistantConfig::LoadGlobals()
{
    // Was the AssistForm::Init global-load sequence.
    LoadTypes();
    LoadFindList();
    LoadIgnoreList();
    LoadSpeechFilter();
    LoadTextReplaces();
    LoadSoundFilter();
    LoadShopList();
    LoadDisplayItems();
    LoadDisplayGroups();
    LoadProfileMap();
}

void CAssistantConfig::SaveGlobals()
{
    SaveTypes();
    SaveFindList();
    SaveIgnoreList();
    SaveSpeechFilter();
    SaveTextReplaces();
    SaveSoundFilter();
    SaveShopList();
    SaveDisplayItems();
    SaveDisplayGroups();
    SaveProfileMap();
}

} // namespace xuoassist
