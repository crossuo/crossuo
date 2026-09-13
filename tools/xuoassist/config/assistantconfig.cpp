// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// AssistantConfig implementation. Every Load*/Save* pair below is an
// extraction of the identically named logic from the tab classes listed in
// the header comments; the XML element/attribute names and the value
// formatting match the original XML writer output byte for byte.

#include "assistantconfig.h"
#include <algorithm>
#include <string.h>

#include <common/fs.h>
#include <common/logging/logging.h>
#include <common/str.h> // wstr_from

#include "../core/assist.h"
#include "../core/packets.h"
#include "../macros/macro.h"
#include "../managers/displaymanager.h"
#include "../managers/partymanager.h"
#include "../model/gameobjects/gameplayer.h"
#include "../model/datareader.h"

namespace xuoassist {

CAssistantConfig g_AssistantConfig;

static const char *CONFIG_DIR = "Config";
static const char *GLOBAL_CONFIG_DIR = "GlobalConfig";

static string to_lower(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

static string fs_entry_name(const fs_path &path)
{
    const string full = fs_path_str(path);
    const size_t slash = full.find_last_of('/');
    return (slash == string::npos) ? full : full.substr(slash + 1);
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

fs_path CAssistantConfig::GetProfilePath()
{
    return fs_path_join(GetConfigPath(), m_ProfileName);
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
        const string name = fs_entry_name(entry);
        if (name.empty() || name[0] == '.')
        {
            continue;
        }
        m_Profiles.push_back(name);
        if (name.size() > 2 && name[0] == '0' && name[1] == 'x')
        {
            const uint serial = CAssistant::TextToSerial(name);
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

void CAssistantConfig::SetCurrentProfile(const string &name)
{
    const string nameLower = to_lower(name);
    if (to_lower(m_ProfileName) == nameLower)
    {
        return;
    }
    for (const string &profile : m_Profiles)
    {
        if (to_lower(profile) == nameLower)
        {
            m_ProfileName = profile;
            return;
        }
    }
    m_ProfileName = name;
}

bool CAssistantConfig::LoadProfileBySerial(uint serial)
{
    LoadProfileMap();
    std::map<uint, string>::iterator it = m_ProfileMap.find(serial);
    if (it != m_ProfileMap.end())
    {
        SetCurrentProfile(it->second);
        return true;
    }
    return false;
}

void CAssistantConfig::SaveProfileMap()
{
    // Was AssistForm::SaveProfileMap -> GlobalConfig/Profiles.xml.
    EnsureDirectories();
    if (g_Connected && g_Player != nullptr)
    {
        m_ProfileMap[g_PlayerSerial] = m_ProfileName;
    }

    XMLDocument doc;
    doc.Root = XMLElement("data");
    for (std::map<uint, string>::iterator i = m_ProfileMap.begin(); i != m_ProfileMap.end(); ++i)
    {
        XMLElement &element = doc.Root.AddChild("profile");
        element.SetAttribute("serial", CAssistant::SerialToText(i->first));
        element.SetAttribute("file", i->second);
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "Profiles.xml"));
}

void CAssistantConfig::LoadProfileMap()
{
    // Was AssistForm::LoadProfileMap.
    m_ProfileMap.clear();
    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetGlobalConfigPath(), "Profiles.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "profile" && element.HasAttribute("serial") &&
            element.HasAttribute("file"))
        {
            const uint serial = CAssistant::TextToSerial(element.Attribute("serial"));
            m_ProfileMap[serial] = element.Attribute("file");
        }
    }
}

bool CAssistantConfig::LoadProfile()
{
    // Was AssistForm::LoadCurrentProfile.
    if (m_ProfileName.empty())
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
        g_SetInt(VKI_FAST_ROTATION, 0);
    }
    else
    {
        g_SetInt(VKI_FAST_ROTATION, 1);
    }

    if (!IgnoreStaminaCheck() || !EnabledFeatureIgnoreStaminaCheck())
    {
        g_SetInt(VKI_IGNORE_STAMINA_CHECK, 0);
    }
    else
    {
        g_SetInt(VKI_IGNORE_STAMINA_CHECK, 1);
    }

    g_Assistant.ClientPrint("Profile '" + m_ProfileName + "' loaded!");
    return true;
}

bool CAssistantConfig::SaveProfile()
{
    // Was AssistForm::SaveCurrentProfile. Note the
    // original did NOT save Macros.xml here (macros save on list change) and
    // did not save the profile map; keep both behaviors.
    if (m_ProfileName.empty())
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

// --- Options.xml -------------------------------------------------------------

bool CAssistantConfig::GetFontColor(ushort &value)
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

void CAssistantConfig::SetFontColor(const bool &state, ushort value)
{
    if (value > 0x0400)
    {
        value = 0x02B2;
    }
    m_UseFontColor = state;
    m_FontColor = CAssistant::GraphicToText(value);
}

bool CAssistantConfig::GetCharactersFontColor(ushort &value)
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

void CAssistantConfig::SetCharactersFontColor(const bool &state, ushort value)
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
    // Was AssistForm::LoadCurrentProfile's Options.xml block dispatching into
    // TabMain/TabDisplay/TabHotkeys/TabScripts::LoadConfig.
    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Options.xml")))
    {
        return;
    }

    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "main")
        {
            if (element.HasAttribute("flags"))
            {
                const uint flags = (uint)element.AttributeInt("flags");
                for (int i = 0; i < CLBI_COUNT; i++)
                {
                    m_Options[i] = (flags & (1 << i)) != 0u;
                }
            }
            if (element.HasAttribute("messageslevel"))
            {
                m_MessagesLevel = element.AttributeInt("messageslevel");
            }
            if (element.HasAttribute("searchdistance"))
            {
                m_SearchItemsDistance = element.AttributeInt("searchdistance");
            }
            if (element.HasAttribute("usedistance"))
            {
                m_UseItemsDistance = element.AttributeInt("usedistance");
            }
            if (element.HasAttribute("opencorpsedistance"))
            {
                m_OpenCorpsesDistance = element.AttributeInt("opencorpsedistance");
            }
            if (element.HasAttribute("corpsekeepdelay"))
            {
                m_CorpseKeepDelay = element.AttributeInt("corpsekeepdelay");
            }
            if (element.HasAttribute("waittargetsdelay"))
            {
                m_WaitTargetsDelay = element.AttributeInt("waittargetsdelay");
            }
            if (element.HasAttribute("moveitemsdelay"))
            {
                m_MoveItemsDelay = element.AttributeInt("moveitemsdelay");
            }
            if (element.HasAttribute("useitemsdelay"))
            {
                m_UseItemsDelay = element.AttributeInt("useitemsdelay");
            }
            if (element.HasAttribute("windowsopacity"))
            {
                m_WindowsOpacity = element.AttributeInt("windowsopacity");
            }
            if (element.HasAttribute("usefontcolor"))
            {
                m_UseFontColor = CAssistant::RawStringToBool(element.Attribute("usefontcolor"));
            }
            if (element.HasAttribute("fontcolor"))
            {
                m_FontColor = element.Attribute("fontcolor");
            }
            if (element.HasAttribute("usecharactersfontcolor"))
            {
                m_UseCharactersFontColor =
                    CAssistant::RawStringToBool(element.Attribute("usecharactersfontcolor"));
            }
            if (element.HasAttribute("charactersfontcolor"))
            {
                m_CharactersFontColor = element.Attribute("charactersfontcolor");
            }
            if (element.HasAttribute("autoreconnect"))
            {
                m_AutoReconnect = CAssistant::RawStringToBool(element.Attribute("autoreconnect"));
            }
            if (element.HasAttribute("autoreconnecttimer"))
            {
                m_AutoReconnectSeconds = element.AttributeInt("autoreconnecttimer");
            }
            if (element.HasAttribute("doubleclickforopendoors"))
            {
                m_DoubleClickForOpenDoors =
                    CAssistant::RawStringToBool(element.Attribute("doubleclickforopendoors"));
            }
        }
        else if (element.Name == "display")
        {
            if (element.HasAttribute("type"))
            {
                m_DisplayType = element.AttributeInt("type");
            }
            if (element.HasAttribute("flags"))
            {
                m_DisplayFlags = (uint)element.AttributeInt("flags");
            }
            if (element.HasAttribute("textcolor"))
            {
                m_DisplayTextColor = CAssistant::RawStringToUInt(element.Attribute("textcolor"));
            }
            if (element.HasAttribute("backgroundcolor"))
            {
                m_DisplayBackgroundColor =
                    CAssistant::RawStringToUInt(element.Attribute("backgroundcolor"));
            }
            if (element.HasAttribute("titledata"))
            {
                m_DisplayTitleData = element.Attribute("titledata");
            }
        }
        else if (element.Name == "hotkeys")
        {
            // TabHotkeys::LoadConfig (the original also had a bug where the
            // guard checked "passhotkeys" but the attribute written was
            // "commandprefix" - reproduce both sides verbatim).
            if (element.HasAttribute("passhotkeys"))
            {
                m_PassHotkeys = CAssistant::RawStringToBool(element.Attribute("passhotkeys"));
            }
            if (element.HasAttribute("hotkeyprefix"))
            {
                m_CommandPrefix = element.Attribute("commandprefix");
            }
            else if (element.HasAttribute("commandprefix"))
            {
                m_CommandPrefix = element.Attribute("commandprefix");
            }
        }
        else if (element.Name == "scripts")
        {
            if (element.HasAttribute("sortscripts"))
            {
                m_SortScripts = CAssistant::RawStringToBool(element.Attribute("sortscripts"));
            }
            if (element.HasAttribute("scriptfile"))
            {
                // Was TabScripts::LoadScript(...): the script engine round
                // re-adds the auto-run; the path itself is config data.
                m_ScriptFilePath = element.Attribute("scriptfile");
            }
        }
    }
}

void CAssistantConfig::SaveMainOptions()
{
    // Was AssistForm::SaveMainOptions: <data version="0"> + main/display/
    // hotkeys/scripts records.
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");

    XMLElement &main = doc.Root.AddChild("main");
    uint flags = 0;
    for (int i = 0; i < CLBI_COUNT; i++)
    {
        if (m_Options[i])
        {
            flags |= 1 << i;
        }
    }
    main.SetAttribute("flags", std::to_string(flags));
    main.SetAttribute("messageslevel", std::to_string(m_MessagesLevel));
    main.SetAttribute("searchdistance", std::to_string(m_SearchItemsDistance));
    main.SetAttribute("usedistance", std::to_string(m_UseItemsDistance));
    main.SetAttribute("opencorpsedistance", std::to_string(m_OpenCorpsesDistance));
    main.SetAttribute("corpsekeepdelay", std::to_string(m_CorpseKeepDelay));
    main.SetAttribute("waittargetsdelay", std::to_string(m_WaitTargetsDelay));
    main.SetAttribute("moveitemsdelay", std::to_string(m_MoveItemsDelay));
    main.SetAttribute("useitemsdelay", std::to_string(m_UseItemsDelay));
    main.SetAttribute("windowsopacity", std::to_string(m_WindowsOpacity));
    main.SetAttribute("usefontcolor", (m_UseFontColor ? "true" : "false"));
    main.SetAttribute("fontcolor", m_FontColor);
    main.SetAttribute("usecharactersfontcolor", (m_UseCharactersFontColor ? "true" : "false"));
    main.SetAttribute("charactersfontcolor", m_CharactersFontColor);
    main.SetAttribute("autoreconnect", (m_AutoReconnect ? "true" : "false"));
    main.SetAttribute("autoreconnecttimer", std::to_string(m_AutoReconnectSeconds));
    main.SetAttribute("doubleclickforopendoors", (m_DoubleClickForOpenDoors ? "true" : "false"));

    // TabDisplay::SaveConfig
    XMLElement &display = doc.Root.AddChild("display");
    display.SetAttribute("type", std::to_string(m_DisplayType));
    display.SetAttribute("flags", std::to_string(m_DisplayFlags));
    display.SetAttribute("textcolor", std::to_string(m_DisplayTextColor));
    display.SetAttribute("backgroundcolor", std::to_string(m_DisplayBackgroundColor));
    display.SetAttribute("titledata", m_DisplayTitleData);

    // TabHotkeys::SaveConfig
    XMLElement &hotkeys = doc.Root.AddChild("hotkeys");
    hotkeys.SetAttribute("passhotkeys", (m_PassHotkeys ? "true" : "false"));
    hotkeys.SetAttribute("commandprefix", m_CommandPrefix);

    // TabScripts::SaveConfig
    XMLElement &scripts = doc.Root.AddChild("scripts");
    scripts.SetAttribute("sortscripts", (m_SortScripts ? "true" : "false"));
    scripts.SetAttribute("scriptfile", m_ScriptFilePath);

    doc.Save(fs_path_join(GetProfilePath(), "Options.xml"));
}

// --- main option gates (TabMain getters, feature-flag gated) ------------------

#define XA_CONFIG_OPTION_GETTER(name, featureEnabled, index)                                       \
    bool CAssistantConfig::name()                                                                  \
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
// Not feature-gated in the original:
bool CAssistantConfig::ObjectInspector()
{
    return m_Options[CLBI_OBJECT_INSPECTOR];
}
XA_CONFIG_OPTION_GETTER(NoGrayScreen, EnabledFeatureNoDeathScreen(), CLBI_NO_GRAY_SCREEN)
// Not feature-gated in the original:
bool CAssistantConfig::SaveFarObjects()
{
    return m_Options[CLBI_SAVE_FAR_OBJECTS];
}
bool CAssistantConfig::StayOnTop()
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
    const uint table[CLBI_COUNT] = {
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
        const uint &flags = table[i];

        if ((flags == 0u) || ((g_EnabledFeature & flags) != 0u))
        {
            continue;
        }

        if (m_Options[i])
        {
            m_Options[i] = false; // was OptionChanged(i, item) on the widget
        }
    }
}

// --- list queries --------------------------------------------------------------

ushort CAssistantConfig::SeekType(const string &text)
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

uint CAssistantConfig::SeekObject(const string &text)
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

CFindListEntry *CAssistantConfig::GetFindList(const string &name)
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

CIgnoreListEntry *CAssistantConfig::GetIgnoreList(const string &name)
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

bool CAssistantConfig::InFriendList(uint serial)
{
    // Was TabListsFriends::InFriendList.
    if (m_FeelPartyMembersAsFriends && g_PartyManager.Contains(serial))
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

bool CAssistantConfig::InEnemyList(uint serial)
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

void CAssistantConfig::AddType(const string &name, const string &type)
{
    // Was TabListsTypes::AddType (minus the target-request UI branch; the
    // empty-type "from target" flow is Phase 2 UI and stays there).
    const string lowerName = to_lower(name);
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

void CAssistantConfig::RemoveType(const string &name)
{
    // Was TabListsTypes::RemoveType.
    const string lowerName = to_lower(name);
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

void CAssistantConfig::AddObject(const string &name, const string &serial)
{
    // Was TabListsObjects::AddObject (minus the target-request UI branch).
    const string lowerName = to_lower(name);
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

void CAssistantConfig::RemoveObject(const string &name)
{
    // Was TabListsObjects::RemoveObject.
    const string lowerName = to_lower(name);
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

bool CAssistantConfig::PartyInviteCanBeAutoAccepted(uint serial)
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

bool CAssistantConfig::PartyInviteCanBeAutoDeclined(uint serial)
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

bool CAssistantConfig::CheckSpeechInFilter(const string &text, const ushort &color)
{
    // Was TabFiltersSpeech::CheckSpeechInFilter.
    if (!SpeechFilter())
    {
        return false;
    }
    for (const CFilterSpeechEntry &item : m_SpeechFilters)
    {
        const ushort itemColor = CAssistant::TextToGraphic(item.Color);
        if (itemColor == 0xFFFF || itemColor == color)
        {
            bool found = false;
            if (item.FullMatch)
            {
                found = (item.Text == text);
            }
            else
            {
                found = text.find(item.Text) != string::npos;
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
    const string &text,
    const uint &serial,
    const ushort &graphic,
    const uchar &messageType,
    const ushort &color,
    const ushort &font,
    const uint &lang,
    const bool &unicode)
{
    // Was TabFiltersReplaces::CheckReplaceText.
    (void)font;
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
        ushort itemColor = CAssistant::TextToGraphic(item.OriginalColor);
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
            found = text.find(item.OriginalText) != string::npos;
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
            ushort replacedColor = CAssistant::TextToGraphic(item.ReplacedColor);
            if (replacedColor == 0xFFFF)
            {
                replacedColor = color;
            }
            if (unicode)
            {
                CPacketToClientTextReplacePrintW(
                    wstr_from(item.ReplacedText),
                    serial,
                    graphic,
                    messageType,
                    replacedColor,
                    0,
                    lang)
                    .SendClient();
            }
            else
            {
                CPacketToClientTextReplacePrintA(
                    item.ReplacedText, serial, graphic, messageType, replacedColor, 3)
                    .SendClient();
            }
            return true;
        }
    }
    return false;
}

bool CAssistantConfig::CheckSoundInFilter(const ushort &index)
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

int CAssistantConfig::DistanceFromText(string text)
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

int CAssistantConfig::DelayFromText(string text)
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

int CAssistantConfig::IsCommandPrefix(const string &text) const
{
    // Was TabHotkeys::IsCommandPrefix.
    int result = 0;
    const string prefix = to_lower(m_CommandPrefix);
    if (!prefix.empty())
    {
        string head = text;
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

CDisplayItem CAssistantConfig::GetCustomDisplayItemByName(const string &name)
{
    // Was TabDisplay::GetCustomDisplayItemByName.
    for (const CDisplayItemEntry &entry : m_DisplayItems)
    {
        if (to_lower(entry.Name) == to_lower(name))
        {
            return entry.m_Item;
        }
    }
    return CDisplayItem();
}

string CAssistantConfig::GetGroupContent(const string &name)
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

CDressSetEntry *CAssistantConfig::GetDressSet(const string &name)
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

CDressSetEntry *CAssistantConfig::AddDressSet(const string &name)
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

// --- Types.xml (was TabListsTypes::LoadTypes/SaveTypes) -------------------------

void CAssistantConfig::SaveTypes()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)m_Types.size()));

    for (const CTypeEntry &entry : m_Types)
    {
        XMLElement &element = doc.Root.AddChild("type");
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("id", CAssistant::GraphicToText(entry.Type));
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "Types.xml"));
}

void CAssistantConfig::LoadTypes()
{
    m_Types.clear();

    XMLDocument doc;
    if (doc.Load(fs_path_join(GetGlobalConfigPath(), "Types.xml")))
    {
        for (const XMLElement &element : doc.Root.Children)
        {
            if (element.Name == "type" && element.HasAttribute("name") &&
                element.HasAttribute("id"))
            {
                CTypeEntry entry;
                entry.Name = element.Attribute("name");
                entry.Type = CAssistant::TextToGraphic(element.Attribute("id"));
                m_Types.push_back(entry);
            }
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

// --- FindList.xml (was TabListsFind::LoadFindList/SaveFindList) ------------------

void CAssistantConfig::SaveFindList()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    // NOTE: the original writer saved count+1 (an off-by-one); kept for
    // byte-compatible output.
    doc.Root.SetAttribute("size", std::to_string((int)m_FindLists.size() + 1));

    for (const CFindListEntry &entry : m_FindLists)
    {
        XMLElement &element = doc.Root.AddChild("findlist");
        element.SetAttribute("name", entry.Name);
        for (const CFindItem &item : entry.m_Items)
        {
            XMLElement &itemElement = element.AddChild("item");
            itemElement.SetAttribute("graphic", item.GetGraphic());
            itemElement.SetAttribute("color", item.GetColor());
            itemElement.SetAttribute("comment", item.GetComment());
        }
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "FindList.xml"));
}

void CAssistantConfig::LoadFindList()
{
    m_FindLists.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetGlobalConfigPath(), "FindList.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "findlist" && element.HasAttribute("name"))
        {
            CFindListEntry entry;
            entry.Name = element.Attribute("name");
            for (const XMLElement &item : element.Children)
            {
                if (item.Name == "item" && item.HasAttribute("graphic") &&
                    item.HasAttribute("color"))
                {
                    CFindItem fi(item.Attribute("graphic"), item.Attribute("color"));
                    if (item.HasAttribute("comment"))
                    {
                        fi.SetComment(item.Attribute("comment"));
                    }
                    entry.m_Items.push_back(fi);
                }
            }
            m_FindLists.push_back(entry);
        }
    }
}

// --- IgnoreList.xml (was TabListsIgnore::LoadIgnoreList/SaveIgnoreList) ----------

void CAssistantConfig::SaveIgnoreList()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    // NOTE: original off-by-one, kept for byte-compatible output.
    doc.Root.SetAttribute("size", std::to_string((int)m_IgnoreLists.size() + 1));

    for (const CIgnoreListEntry &entry : m_IgnoreLists)
    {
        XMLElement &element = doc.Root.AddChild("ignorelist");
        element.SetAttribute("name", entry.Name);
        for (const CIgnoreItem &item : entry.m_Items)
        {
            XMLElement &itemElement = element.AddChild("item");
            itemElement.SetAttribute("serial", item.GetSerial());
            itemElement.SetAttribute("graphic", item.GetGraphic());
            itemElement.SetAttribute("color", item.GetColor());
            itemElement.SetAttribute("comment", item.GetComment());
        }
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "IgnoreList.xml"));
}

void CAssistantConfig::LoadIgnoreList()
{
    m_IgnoreLists.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetGlobalConfigPath(), "IgnoreList.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "ignorelist" && element.HasAttribute("name"))
        {
            CIgnoreListEntry entry;
            entry.Name = element.Attribute("name");
            for (const XMLElement &item : element.Children)
            {
                if (item.Name == "item" && item.HasAttribute("serial") &&
                    item.HasAttribute("graphic") && item.HasAttribute("color"))
                {
                    CIgnoreItem ii(
                        item.Attribute("serial"),
                        item.Attribute("graphic"),
                        item.Attribute("color"));
                    if (item.HasAttribute("comment"))
                    {
                        ii.SetComment(item.Attribute("comment"));
                    }
                    entry.m_Items.push_back(ii);
                }
            }
            m_IgnoreLists.push_back(entry);
        }
    }
}

// --- SpeechFilter.xml (was TabFiltersSpeech::LoadSpeechFilter/SaveSpeechFilter) ---

void CAssistantConfig::SaveSpeechFilter()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");

    for (const CFilterSpeechEntry &entry : m_SpeechFilters)
    {
        XMLElement &element = doc.Root.AddChild("speech");
        element.SetAttribute("text", entry.Text);
        element.SetAttribute("color", entry.Color);
        element.SetAttribute("fullmatch", (entry.FullMatch ? "true" : "false"));
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "SpeechFilter.xml"));
}

void CAssistantConfig::LoadSpeechFilter()
{
    m_SpeechFilters.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetGlobalConfigPath(), "SpeechFilter.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "speech" && element.HasAttribute("text") &&
            element.HasAttribute("color") && element.HasAttribute("fullmatch"))
        {
            CFilterSpeechEntry entry;
            entry.Text = element.Attribute("text");
            entry.Color = element.Attribute("color");
            entry.FullMatch = CAssistant::RawStringToBool(element.Attribute("fullmatch"));
            m_SpeechFilters.push_back(entry);
        }
    }
}

// --- TextReplaces.xml (was TabFiltersReplaces::LoadTextReplaces/SaveTextReplaces) --

void CAssistantConfig::SaveTextReplaces()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");

    for (const CFilterReplaceEntry &entry : m_Replaces)
    {
        XMLElement &element = doc.Root.AddChild("replace");
        element.SetAttribute("originaltext", entry.OriginalText);
        element.SetAttribute("originalcolor", entry.OriginalColor);
        element.SetAttribute("replacedtext", entry.ReplacedText);
        element.SetAttribute("replacedcolor", entry.ReplacedColor);
        element.SetAttribute("type", std::to_string(entry.Type));
        element.SetAttribute("mode", std::to_string(entry.Mode));
        element.SetAttribute("fullmatch", (entry.FullMatch ? "true" : "false"));
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "TextReplaces.xml"));
}

void CAssistantConfig::LoadTextReplaces()
{
    m_Replaces.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetGlobalConfigPath(), "TextReplaces.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "replace" && element.HasAttribute("originaltext") &&
            element.HasAttribute("originalcolor") && element.HasAttribute("replacedtext") &&
            element.HasAttribute("replacedcolor") && element.HasAttribute("type") &&
            element.HasAttribute("mode") && element.HasAttribute("fullmatch"))
        {
            CFilterReplaceEntry entry;
            entry.OriginalText = element.Attribute("originaltext");
            entry.OriginalColor = element.Attribute("originalcolor");
            entry.ReplacedText = element.Attribute("replacedtext");
            entry.ReplacedColor = element.Attribute("replacedcolor");
            entry.Type = element.AttributeInt("type");
            entry.Mode = element.AttributeInt("mode");
            entry.FullMatch = CAssistant::RawStringToBool(element.Attribute("fullmatch"));
            m_Replaces.push_back(entry);
        }
    }
}

// --- SoundFilter.xml (was TabFiltersSound::LoadSoundFilter/SaveSoundFilter) --------

void CAssistantConfig::SaveSoundFilter()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");

    for (const CFilterSoundEntry &entry : m_SoundFilters)
    {
        XMLElement &element = doc.Root.AddChild("sound");
        element.SetAttribute("index", entry.Index);
        element.SetAttribute("name", entry.Name);
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "SoundFilter.xml"));
}

void CAssistantConfig::LoadSoundFilter()
{
    m_SoundFilters.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetGlobalConfigPath(), "SoundFilter.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "sound" && element.HasAttribute("index") &&
            element.HasAttribute("name"))
        {
            CFilterSoundEntry entry;
            entry.Index = element.Attribute("index");
            entry.Name = element.Attribute("name");
            m_SoundFilters.push_back(entry);
        }
    }
}

// --- ShopList.xml (was TabAgentsShop::LoadShopList/SaveShopList) --------------------

void CAssistantConfig::SaveShopList()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("show_shopping_progress_bar", (m_ShowShoppingProgressBar ? "true" : "false"));
    doc.Root.SetAttribute("max_shop_items", std::to_string(m_MaxShopItems));
    doc.Root.SetAttribute("max_sell_price", std::to_string(m_MaxSellPrice));
    doc.Root.SetAttribute("shop_delay", std::to_string(m_ShopDelay));
    doc.Root.SetAttribute("shop_timeout", std::to_string(m_ShopTimeout));

    for (const CShopListEntry &entry : m_ShopLists)
    {
        XMLElement &element = doc.Root.AddChild("shoplist");
        element.SetAttribute("name", entry.Name);
        for (const CShopItemEntry &item : entry.m_Items)
        {
            XMLElement &itemElement = element.AddChild("shopitem");
            itemElement.SetAttribute("graphic", item.Graphic);
            itemElement.SetAttribute("color", item.Color);
            itemElement.SetAttribute("name", item.Name);
            itemElement.SetAttribute("count", std::to_string(item.Count));
            itemElement.SetAttribute("price", std::to_string(item.Price));
            itemElement.SetAttribute("comment", item.Comment);
        }
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "ShopList.xml"));
}

void CAssistantConfig::LoadShopList()
{
    m_ShopLists.clear();

    XMLDocument doc;
    if (doc.Load(fs_path_join(GetGlobalConfigPath(), "ShopList.xml")))
    {
        if (doc.Root.HasAttribute("show_shopping_progress_bar"))
        {
            m_ShowShoppingProgressBar =
                CAssistant::RawStringToBool(doc.Root.Attribute("show_shopping_progress_bar"));
        }
        if (doc.Root.HasAttribute("max_shop_items"))
        {
            m_MaxShopItems = doc.Root.AttributeInt("max_shop_items");
        }
        if (doc.Root.HasAttribute("max_sell_price"))
        {
            m_MaxSellPrice = doc.Root.AttributeInt("max_sell_price");
        }
        if (doc.Root.HasAttribute("shop_delay"))
        {
            m_ShopDelay = doc.Root.AttributeInt("shop_delay");
        }
        if (doc.Root.HasAttribute("shop_timeout"))
        {
            m_ShopTimeout = doc.Root.AttributeInt("shop_timeout");
        }

        CShopListEntry *listEntry = nullptr;
        for (const XMLElement &element : doc.Root.Children)
        {
            if (element.Name == "shoplist" && element.HasAttribute("name"))
            {
                CShopListEntry entry;
                entry.Name = element.Attribute("name");
                m_ShopLists.push_back(entry);
                listEntry = &m_ShopLists.back();
            }
            else if (element.Name == "shopitem" && listEntry != nullptr)
            {
                if (element.HasAttribute("graphic") && element.HasAttribute("color") &&
                    element.HasAttribute("name") && element.HasAttribute("count") &&
                    element.HasAttribute("price") && element.HasAttribute("comment"))
                {
                    CShopItemEntry item;
                    item.Graphic = element.Attribute("graphic");
                    item.Color = element.Attribute("color");
                    item.Name = element.Attribute("name");
                    item.Count = element.AttributeInt("count");
                    item.Price = element.AttributeInt("price");
                    item.Comment = element.Attribute("comment");
                    listEntry->m_Items.push_back(item);
                }
            }
        }
    }
}

// --- DisplayItems.xml / DisplayGroups.xml (was TabDisplay) ---------------------------

void CAssistantConfig::SaveDisplayItems()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)m_DisplayItems.size()));

    for (const CDisplayItemEntry &entry : m_DisplayItems)
    {
        const CDisplayItem &di = entry.m_Item;
        XMLElement &element = doc.Root.AddChild("item");
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("type", std::to_string((int)di.GetType()));
        element.SetAttribute("text", di.GetText());
        element.SetAttribute("id", CAssistant::GraphicToText(di.GetGraphic()));
        element.SetAttribute("hue", CAssistant::GraphicToText(di.GetHue()));
        element.SetAttribute("mincolor", CAssistant::ColorToText(di.GetMinColor()));
        element.SetAttribute("midcolor", CAssistant::ColorToText(di.GetMidColor()));
        element.SetAttribute("color", CAssistant::ColorToText(di.GetColor()));
        element.SetAttribute("background", (di.GetBackground() ? "true" : "false"));
        element.SetAttribute("bgmincolor", CAssistant::ColorToText(di.GetBackgroundMinColor()));
        element.SetAttribute("bgmidcolor", CAssistant::ColorToText(di.GetBackgroundMidColor()));
        element.SetAttribute("bgcolor", CAssistant::ColorToText(di.GetBackgroundColor()));
        element.SetAttribute("minvalue", std::to_string(di.GetMinValue()));
        element.SetAttribute("midvalue", std::to_string(di.GetMidValue()));
        element.SetAttribute("percents", (di.GetPercents() ? "true" : "false"));
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "DisplayItems.xml"));
}

void CAssistantConfig::LoadDisplayItems()
{
    m_DisplayItems.clear();

    XMLDocument doc;
    if (doc.Load(fs_path_join(GetGlobalConfigPath(), "DisplayItems.xml")))
    {
        for (const XMLElement &element : doc.Root.Children)
        {
            if (element.Name != "item" || !element.HasAttribute("name"))
            {
                continue;
            }
            CDisplayItemEntry entry;
            entry.Name = element.Attribute("name");
            if (element.HasAttribute("type"))
            {
                entry.m_Item.SetType((DISPLAY_ITEM_TYPE)element.AttributeInt("type"));
            }
            if (element.HasAttribute("text"))
            {
                entry.m_Item.SetText(element.Attribute("text"));
            }
            if (element.HasAttribute("id"))
            {
                entry.m_Item.SetGraphic((ushort)strtoul(element.Attribute("id").c_str(), nullptr, 16));
            }
            if (element.HasAttribute("hue"))
            {
                entry.m_Item.SetHue((ushort)strtoul(element.Attribute("hue").c_str(), nullptr, 16));
            }
            if (element.HasAttribute("mincolor"))
            {
                entry.m_Item.SetMinColor(CAssistant::RawStringToUInt(element.Attribute("mincolor")));
            }
            if (element.HasAttribute("midcolor"))
            {
                entry.m_Item.SetMidColor(CAssistant::RawStringToUInt(element.Attribute("midcolor")));
            }
            if (element.HasAttribute("color"))
            {
                entry.m_Item.SetColor(CAssistant::RawStringToUInt(element.Attribute("color")));
            }
            if (element.HasAttribute("background"))
            {
                entry.m_Item.SetBackground(
                    CAssistant::RawStringToBool(element.Attribute("background")));
            }
            if (element.HasAttribute("bgmincolor"))
            {
                entry.m_Item.SetBackgroundMinColor(
                    CAssistant::RawStringToUInt(element.Attribute("bgmincolor")));
            }
            if (element.HasAttribute("bgmidcolor"))
            {
                entry.m_Item.SetBackgroundMidColor(
                    CAssistant::RawStringToUInt(element.Attribute("bgmidcolor")));
            }
            if (element.HasAttribute("bgcolor"))
            {
                entry.m_Item.SetBackgroundColor(
                    CAssistant::RawStringToUInt(element.Attribute("bgcolor")));
            }
            if (element.HasAttribute("minvalue"))
            {
                entry.m_Item.SetMinValue(element.AttributeInt("minvalue"));
            }
            if (element.HasAttribute("midvalue"))
            {
                entry.m_Item.SetMidValue(element.AttributeInt("midvalue"));
            }
            if (element.HasAttribute("percents"))
            {
                entry.m_Item.SetPercents(CAssistant::RawStringToBool(element.Attribute("percents")));
            }
            m_DisplayItems.push_back(entry);
        }
    }

    if (m_DisplayItems.empty())
    {
        // The original app's defaults (TabDisplay::LoadDisplayItems): one entry per
        // info command plus the reagent counters.
        for (const CDisplayInfo &info : g_DisplayManager.GetCommands())
        {
            if (info.Name.empty() || info.Command.empty())
            {
                continue;
            }
            CDisplayItem di = g_DisplayManager.GetDisplayItemByName(info.Command);
            if (di.GetType() != DIT_NONE && di.GetType() != DIT_OBJECT)
            {
                CDisplayItemEntry entry;
                entry.Name = info.Command;
                entry.m_Item = di;
                m_DisplayItems.push_back(entry);
            }
        }

        static const char *defNames[10] = { "bm", "bp", "ga", "gs", "mr",
                                            "ns", "sa", "ss", "en", "bandage" };
        for (int i = 0; i < 10; i++)
        {
            CDisplayItemEntry entry;
            entry.Name = defNames[i];
            entry.m_Item = CDisplayItem(
                DIT_OBJECT,
                "",
                CAssistant::TextToGraphic(defNames[i]),
                0,
                0x00FFFFFF,
                0x00000001,
                0x00000001,
                true,
                0x0000007F,
                0x0000FFFF,
                0x00000000,
                10,
                20,
                false);
            entry.m_Item.SetText(i < 9 ? string(defNames[i]) + ":" : string("b:"));
            m_DisplayItems.push_back(entry);
        }

        SaveDisplayItems();
    }
}

void CAssistantConfig::SaveDisplayGroups()
{
    EnsureDirectories();

    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)m_DisplayGroups.size()));

    for (const CDisplayGroupEntry &entry : m_DisplayGroups)
    {
        XMLElement &element = doc.Root.AddChild("group");
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("text", entry.TitleData);
    }

    doc.Save(fs_path_join(GetGlobalConfigPath(), "DisplayGroups.xml"));
}

void CAssistantConfig::LoadDisplayGroups()
{
    m_DisplayGroups.clear();

    XMLDocument doc;
    if (doc.Load(fs_path_join(GetGlobalConfigPath(), "DisplayGroups.xml")))
    {
        for (const XMLElement &element : doc.Root.Children)
        {
            if (element.Name == "group" && element.HasAttribute("name") &&
                element.HasAttribute("text"))
            {
                CDisplayGroupEntry entry;
                entry.Name = element.Attribute("name");
                entry.TitleData = element.Attribute("text");
                m_DisplayGroups.push_back(entry);
            }
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

// --- Objects.xml (was TabListsObjects::LoadObjects/SaveObjects) -----------------------

void CAssistantConfig::SaveObjects()
{
    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)m_Objects.size()));
    doc.Root.SetAttribute("dressbag", CAssistant::SerialToText(g_DressBag));

    for (const CObjectEntry &entry : m_Objects)
    {
        XMLElement &element = doc.Root.AddChild("object");
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("id", CAssistant::SerialToText(entry.Serial));
    }

    doc.Save(fs_path_join(GetProfilePath(), "Objects.xml"));
}

void CAssistantConfig::LoadObjects()
{
    m_Objects.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Objects.xml")))
    {
        return;
    }
    if (doc.Root.HasAttribute("dressbag"))
    {
        g_Assistant.SetDressBag(CAssistant::TextToSerial(doc.Root.Attribute("dressbag")));
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "object" && element.HasAttribute("name") && element.HasAttribute("id"))
        {
            CObjectEntry entry;
            entry.Name = element.Attribute("name");
            entry.Serial = CAssistant::TextToSerial(element.Attribute("id"));
            m_Objects.push_back(entry);
        }
    }
}

// --- Hotkeys.xml (was HotkeysForm::Load/Save) ------------------------------------------

void CAssistantConfig::SaveHotkeys()
{
    // Was HotkeysForm::Save: version "1" records with SDL key codes; the
    // original also wrote size = count + 1 (kept).
    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "1");
    doc.Root.SetAttribute("size", std::to_string((int)m_Hotkeys.size() + 1));

    for (const CHotkeyEntry &entry : m_Hotkeys)
    {
        XMLElement &element = doc.Root.AddChild("hotkey");
        element.SetAttribute("type", std::to_string(entry.Type));
        element.SetAttribute("action", std::to_string(entry.Action));
        element.SetAttribute("running_mode", (entry.RunningMode ? "true" : "false"));
        element.SetAttribute("key", std::to_string(entry.Key));
        element.SetAttribute("modifiers", std::to_string(entry.Modifiers));
        element.SetAttribute("name", entry.ScriptOrMacroName);
        element.SetAttribute("external_code", entry.ExternalCode);
    }

    doc.Save(fs_path_join(GetProfilePath(), "Hotkeys.xml"));
}

bool CAssistantConfig::LoadHotkeys()
{
    // Was HotkeysForm::Load: tolerates the legacy version 0 (VK codes +
    // per-record macro name / toggle_hotkeys) and the current version 1.
    m_Hotkeys.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Hotkeys.xml")))
    {
        return false;
    }

    const int version = doc.Root.HasAttribute("version") ? doc.Root.AttributeInt("version") : 0;

    for (const XMLElement &element : doc.Root.Children)
    {
        // v0 also stored bare `<toggle_hotkeys key= modifiers=>` records
        if (element.Name != "hotkey" &&
            !(version == 0 && element.Name == "toggle_hotkeys"))
        {
            continue;
        }

        if (version == 0)
        {
            if (element.Name == "toggle_hotkeys")
            {
                // Legacy toggle row (hotkeysform.cpp:375-398): a plain
                // key/modifiers binding of HT_TOGGLE_HOTKEYS, action 0, no
                // macro/script (the Data column renders the derived action
                // name, "Hotkeys: toggle", like the original UpdateHotkeyData).
                if (!element.HasAttribute("key") || !element.HasAttribute("modifiers"))
                {
                    continue;
                }
                CHotkeyEntry entry;
                entry.Type = HT_TOGGLE_HOTKEYS;
                entry.Action = 0;
                entry.Key = (uint)strtoul(element.Attribute("key").c_str(), nullptr, 10);
                entry.Modifiers = (uchar)strtoul(element.Attribute("modifiers").c_str(), nullptr, 10);
                m_Hotkeys.push_back(entry);
                continue;
            }
            // Legacy: converted on load to the v1 record shape (a script
            // hotkey, or the special "ExternalCode" entry).
            if (!element.HasAttribute("onescriptrunning") || !element.HasAttribute("key") ||
                !element.HasAttribute("modifiers") || !element.HasAttribute("macro"))
            {
                continue;
            }
            CHotkeyEntry entry;
            string name = element.Attribute("macro");
            entry.Type = HT_SCRIPT;
            if (name == "ExternalCode")
            {
                entry.Type = HT_EXTERNAL_CODE;
                name = "";
            }
            entry.Action = 0;
            entry.ScriptOrMacroName = name;
            entry.ExternalCode = element.Attribute("externalcode");
            entry.RunningMode = CAssistant::RawStringToBool(element.Attribute("onescriptrunning"));
            entry.Key = (uint)strtoul(element.Attribute("key").c_str(), nullptr, 10);
            entry.Modifiers = (uchar)strtoul(element.Attribute("modifiers").c_str(), nullptr, 10);
            m_Hotkeys.push_back(entry);
        }
        else if (version == 1)
        {
            if (!element.HasAttribute("key") || !element.HasAttribute("modifiers") ||
                !element.HasAttribute("type") || !element.HasAttribute("action"))
            {
                continue;
            }
            CHotkeyEntry entry;
            entry.RunningMode =
                element.HasAttribute("running_mode") ?
                    CAssistant::RawStringToBool(element.Attribute("running_mode")) :
                    false;
            entry.ScriptOrMacroName = element.Attribute("name");
            entry.ExternalCode = element.Attribute("external_code");
            // SDLK codes (see GetKeycodeFromVK in the original comment)
            entry.Key = (uint)strtoul(element.Attribute("key").c_str(), nullptr, 10);
            entry.Modifiers = (uchar)strtoul(element.Attribute("modifiers").c_str(), nullptr, 10);
            entry.Type = (uint)strtoul(element.Attribute("type").c_str(), nullptr, 10);
            entry.Action = (uint)strtoul(element.Attribute("action").c_str(), nullptr, 10);
            m_Hotkeys.push_back(entry);
        }
    }

    return true;
}

// --- Party.xml (was TabAgentsParty::LoadParty/SaveParty) ---------------------------------

void CAssistantConfig::SaveParty()
{
    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)(m_PartyAccept.size() + m_PartyDecline.size())));
    doc.Root.SetAttribute("autoacceptall", (m_PartyAutoAcceptAll ? "true" : "false"));
    doc.Root.SetAttribute("autoacceptfriends", (m_PartyAutoAcceptFriends ? "true" : "false"));
    doc.Root.SetAttribute("autodeclineall", (m_PartyAutoDeclineAll ? "true" : "false"));
    doc.Root.SetAttribute("autodeclineenemies", (m_PartyAutoDeclineEnemies ? "true" : "false"));

    for (const CWorldObjectEntry &entry : m_PartyAccept)
    {
        XMLElement &element = doc.Root.AddChild("acceptobject");
        element.SetAttribute("id", CAssistant::SerialToText(entry.Serial));
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("enabled", (entry.Enabled ? "true" : "false"));
    }

    for (const CWorldObjectEntry &entry : m_PartyDecline)
    {
        XMLElement &element = doc.Root.AddChild("declineobject");
        element.SetAttribute("id", CAssistant::SerialToText(entry.Serial));
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("enabled", (entry.Enabled ? "true" : "false"));
    }

    doc.Save(fs_path_join(GetProfilePath(), "Party.xml"));
}

void CAssistantConfig::LoadParty()
{
    m_PartyAccept.clear();
    m_PartyDecline.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Party.xml")))
    {
        return;
    }
    if (doc.Root.HasAttribute("autoacceptall"))
    {
        m_PartyAutoAcceptAll = CAssistant::RawStringToBool(doc.Root.Attribute("autoacceptall"));
    }
    if (doc.Root.HasAttribute("autoacceptfriends"))
    {
        m_PartyAutoAcceptFriends =
            CAssistant::RawStringToBool(doc.Root.Attribute("autoacceptfriends"));
    }
    if (doc.Root.HasAttribute("autodeclineall"))
    {
        m_PartyAutoDeclineAll = CAssistant::RawStringToBool(doc.Root.Attribute("autodeclineall"));
    }
    if (doc.Root.HasAttribute("autodeclineenemies"))
    {
        m_PartyAutoDeclineEnemies =
            CAssistant::RawStringToBool(doc.Root.Attribute("autodeclineenemies"));
    }

    for (const XMLElement &element : doc.Root.Children)
    {
        if (!element.HasAttribute("id") || !element.HasAttribute("name") ||
            !element.HasAttribute("enabled"))
        {
            continue;
        }
        CWorldObjectEntry entry;
        entry.Serial = CAssistant::TextToSerial(element.Attribute("id"));
        entry.Name = element.Attribute("name");
        entry.Enabled = CAssistant::RawStringToBool(element.Attribute("enabled"));
        if (element.Name == "acceptobject")
        {
            m_PartyAccept.push_back(entry);
        }
        else if (element.Name == "declineobject")
        {
            m_PartyDecline.push_back(entry);
        }
    }
}

// --- Friends.xml (was TabListsFriends::LoadFriends/SaveFriends) ---------------------------

void CAssistantConfig::SaveFriends()
{
    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)m_Friends.size()));
    doc.Root.SetAttribute("feel_party_members_as_friends", (m_FeelPartyMembersAsFriends ? "true" : "false"));

    for (const CWorldObjectEntry &entry : m_Friends)
    {
        XMLElement &element = doc.Root.AddChild("friend");
        element.SetAttribute("id", CAssistant::SerialToText(entry.Serial));
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("enabled", (entry.Enabled ? "true" : "false"));
    }

    doc.Save(fs_path_join(GetProfilePath(), "Friends.xml"));
}

void CAssistantConfig::LoadFriends()
{
    m_Friends.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Friends.xml")))
    {
        return;
    }
    if (doc.Root.HasAttribute("feel_party_members_as_friends"))
    {
        m_FeelPartyMembersAsFriends =
            CAssistant::RawStringToBool(doc.Root.Attribute("feel_party_members_as_friends"));
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "friend" && element.HasAttribute("id") &&
            element.HasAttribute("name") && element.HasAttribute("enabled"))
        {
            CWorldObjectEntry entry;
            entry.Serial = CAssistant::TextToSerial(element.Attribute("id"));
            entry.Name = element.Attribute("name");
            entry.Enabled = CAssistant::RawStringToBool(element.Attribute("enabled"));
            m_Friends.push_back(entry);
        }
    }
}

// --- Enemies.xml (was TabListsEnemies::LoadEnemies/SaveEnemies) ----------------------------

void CAssistantConfig::SaveEnemies()
{
    XMLDocument doc;
    doc.Root = XMLElement("data");
    doc.Root.SetAttribute("version", "0");
    doc.Root.SetAttribute("size", std::to_string((int)m_Enemies.size()));

    for (const CWorldObjectEntry &entry : m_Enemies)
    {
        XMLElement &element = doc.Root.AddChild("enemy");
        element.SetAttribute("id", CAssistant::SerialToText(entry.Serial));
        element.SetAttribute("name", entry.Name);
        element.SetAttribute("enabled", (entry.Enabled ? "true" : "false"));
    }

    doc.Save(fs_path_join(GetProfilePath(), "Enemies.xml"));
}

void CAssistantConfig::LoadEnemies()
{
    m_Enemies.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Enemies.xml")))
    {
        return;
    }
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "enemy" && element.HasAttribute("id") && element.HasAttribute("name") &&
            element.HasAttribute("enabled"))
        {
            CWorldObjectEntry entry;
            entry.Serial = CAssistant::TextToSerial(element.Attribute("id"));
            entry.Name = element.Attribute("name");
            entry.Enabled = CAssistant::RawStringToBool(element.Attribute("enabled"));
            m_Enemies.push_back(entry);
        }
    }
}

// --- Macros.xml (was TabMacros::Load/Save) ---------------------------------------------------

void CAssistantConfig::SaveMacros()
{
    XMLDocument doc;
    doc.Root = XMLElement("data");

    for (const CMacroEntry &entry : m_Macros)
    {
        XMLElement &element = doc.Root.AddChild("macro");
        element.SetAttribute("name", entry.Name);
        for (CMacro *macro : entry.m_Items)
        {
            XMLElement &action = element.AddChild("action");
            macro->Save(action);
        }
    }

    doc.Save(fs_path_join(GetProfilePath(), "Macros.xml"));
}

bool CAssistantConfig::LoadMacros()
{
    // Was TabMacros::Load.
    for (CMacroEntry &entry : m_Macros)
    {
        entry.m_Items.clear();
    }
    m_Macros.clear();

    XMLDocument doc;
    if (!doc.Load(fs_path_join(GetProfilePath(), "Macros.xml")))
    {
        return false;
    }

    CMacroEntry *macro = nullptr;
    for (const XMLElement &element : doc.Root.Children)
    {
        if (element.Name == "macro" && element.HasAttribute("name"))
        {
            CMacroEntry entry;
            entry.Name = element.Attribute("name");
            m_Macros.push_back(entry);
            macro = &m_Macros.back();
        }
        else if (element.Name == "action" && macro != nullptr)
        {
            CMacro *action = CMacro::Load(element);
            if (action != nullptr)
            {
                macro->m_Items.push_back(action);
            }
        }
    }

    return true;
}

// --- Dress.coa (was TabAgentsDress::LoadDress/SaveDress) --------------------------------------
//
// Custom little-endian binary format, ported on top of the namespace-local
// CDataWriter (the original app used its file-based CFileObject). Layout:
//   uint32 version (=0), int32 setsCount, uint8 undressBefore,
//   per set: string name, uint8 layerCount, per layer { uint8 layer, uint32 serial },
//   uint8 doubleClickForEquipment.
// CFileObject::WriteString wrote: int8 length (= utf8 length + 1), the utf8
// bytes, one 0x00 byte. The original reader's ReadString read a full int32
// length (asymmetric with the writer - an original bug); the reader below
// reproduces that exact tolerance so both legacy files and our files behave
// identically (names that fail the bounds check come back empty and the set
// is skipped, like the original `continue`).

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
        const string utf8 = entry.Name;
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
            const uint serial = entry.m_Items[layer];
            if (serial != 0u)
            {
                writer.WriteUInt8((uint8_t)layer);
                writer.WriteUInt32LE(serial);
            }
        }
    }

    writer.WriteUInt8(m_DressDoubleClickForEquipment ? 1 : 0);

    const vector<uint8_t> bytes = writer.Data();
    // Dress.coa goes through the same crash-safe temp+rename path as the XML
    // writers (plan §8.1)
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
        string name;
        if (!reader.IsEOF())
        {
            const int len = reader.ReadInt32LE();
            const size_t remaining = size - (reader.GetPtr() - reader.GetStart());
            if (len > 0 && (size_t)len <= remaining)
            {
                name = string((const char *)reader.GetPtr(), len);
            }
        }

        if (name.empty())
        {
            continue;
        }

        CDressSetEntry entry;
        entry.Name = name;

        const uchar layerCount = reader.ReadUInt8();
        for (int j = 0; j < layerCount && !reader.IsEOF(); j++)
        {
            const uchar layer = reader.ReadUInt8();
            const uint serial = reader.ReadUInt32LE();
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
    XMLSelfTest(); // config round-trip self-test (all builds, logs on failure)
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
