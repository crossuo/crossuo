// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CConfigManager
{
protected:
    bool m_Sound = true;
    bool m_Music = false;
    uint8_t m_SoundVolume = 255;
    uint8_t m_MusicVolume = 255;
    uint8_t m_ClientFPS = 32;
    bool m_UseScaling = false;
    uint8_t m_DrawStatusState = 0;
    bool m_DrawStumps = false;
    bool m_MarkingCaves = false;
    bool m_NoVegetation = false;
    bool m_NoAnimateFields = false;
    bool m_ReduceFPSUnactiveWindow = false;
    bool m_ConsoleNeedEnter = false;
    uint8_t m_SpellIconAlpha = 0;
    bool m_OldStyleStatusbar = false;
    bool m_OriginalPartyStatusbar = false;
    bool m_ApplyStateColorOnCharacters = false;
    bool m_ChangeFieldsGraphic = false;
    bool m_PaperdollSlots = false;
    uint8_t m_DrawAuraState = 0;
    bool m_ScaleImagesInPaperdollSlots = false;
    bool m_UseGlobalMapLayer = false;
    bool m_NoDrawRoofs = false;
    uint8_t m_PingTimer = 10;
    uint8_t m_ItemPropertiesMode = OPM_FOLLOW_MOUSE;
    bool m_ItemPropertiesIcon = false;
    uint8_t m_CharacterBackpackStyle = CBS_DEFAULT;

public:
    bool FootstepsSound = true;
    bool CombatMusic = true;
    bool BackgroundSound = false;
    bool RemoveTextWithBlending = false;
    uint8_t DrawStatusConditionState = 0;
    uint8_t DrawStatusConditionValue = 0;
    bool StandartCharactersAnimationDelay = false;
    bool StandartItemsAnimationDelay = false;
    bool LockGumpsMoving = false;
    uint8_t HiddenCharactersRenderMode = 0;
    uint8_t HiddenAlpha = 0;
    bool UseHiddenModeOnlyForSelf = false;
    uint8_t TransparentSpellIcons = 0;
    bool RemoveStatusbarsWithoutObjects = false;
    bool ShowDefaultConsoleEntryMode = false;
    bool DrawAuraWithCtrlPressed = false;
    uint8_t ScreenshotFormat = 0;
    bool RemoveOrCreateObjectsWithBlending = false;
    bool DrawHelmetsOnShroud = false;
    bool HighlightTargetByType = true;
    bool AutoDisplayWorldMap = false;
    bool DisableMacroInChat = false;
    bool CheckPing = true;
    bool CancelNewTargetSystemOnShiftEsc = false;
    bool DrawStatusForHumanoids = true;

    bool UseToolTips = false;
    uint16_t ToolTipsTextColor = 0;
    uint16_t ToolTipsTextFont = 0;
    uint16_t ToolTipsDelay = 0;

    uint16_t ChatColorInputText = 0;
    uint16_t ChatColorMenuOption = 0;
    uint16_t ChatColorPlayerInMemberList = 0;
    uint16_t ChatColorText = 0;
    uint16_t ChatColorPlayerNameWithout = 0;
    uint16_t ChatColorMuted = 0;
    uint16_t ChatColorChannelModeratorName = 0;
    uint16_t ChatColorChannelModeratorText = 0;
    uint16_t ChatColorMyName = 0;
    uint16_t ChatColorMyText = 0;
    uint16_t ChatColorSystemMessage = 0;
    uint16_t ChatFont = 0;
    uint16_t ChatColorBGOutputText = 0;
    uint16_t ChatColorBGInputText = 0;
    uint16_t ChatColorBGUserList = 0;
    uint16_t ChatColorBGConfList = 0;
    uint16_t ChatColorBGCommandList = 0;

    bool EnablePathfind = false;
    bool HoldTabForCombat = false;
    bool OffsetInterfaceWindows = false;
    bool AutoArrange = false;
    bool AlwaysRun = false;
    bool DisableMenubar = false;
    bool GrayOutOfRangeObjects = false;
    bool DisableNewTargetSystem = false;
    bool ObjectHandles = false;
    bool HoldShiftForContextMenus = false;
    bool HoldShiftForEnablePathfind = false;
    int GameWindowWidth = 0;
    int GameWindowHeight = 0;
    uint16_t SpeechDelay = 0;
    bool ScaleSpeechDelay = false;
    uint16_t SpeechColor = 0;
    uint16_t EmoteColor = 0;
    uint16_t PartyMessageColor = 0;
    uint16_t GuildMessageColor = 0;
    uint16_t AllianceMessageColor = 0;
    bool IgnoreGuildMessage = false;
    bool IgnoreAllianceMessage = false;
    bool DarkNights = false;
    bool ColoredLighting = false;
    bool LockResizingGameWindow = false;

    uint16_t InnocentColor = 0;
    uint16_t FriendlyColor = 0;
    uint16_t SomeoneColor = 0;
    uint16_t CriminalColor = 0;
    uint16_t EnemyColor = 0;
    uint16_t MurdererColor = 0;
    bool CriminalActionsQuery = false;

    bool ShowIncomingNames = false;
    bool UseCircleTrans = false;
    bool StatReport = false;
    uint8_t CircleTransRadius = 0;
    uint8_t SkillReport = 0;
    uint16_t SpeechFont = 0;

    bool FilterPWOn = false;
    bool ObscenityFilter = false;
    string FilterPassword = "";

    bool ToggleBufficonWindow = false;
    int GameWindowX = 0;
    int GameWindowY = 0;
    int UpdateRange = g_MaxViewRange;

public:
    CConfigManager();
    ~CConfigManager() {}

    bool GetSound() { return m_Sound; };
    void SetSound(bool val);

    bool GetMusic() { return m_Music; };
    void SetMusic(bool val);

    uint8_t GetSoundVolume() { return m_SoundVolume; };
    void SetSoundVolume(uint8_t val);

    uint8_t GetMusicVolume() { return m_MusicVolume; };
    void SetMusicVolume(uint8_t val);

    uint8_t GetClientFPS() { return m_ClientFPS; };
    void SetClientFPS(uint8_t val);

    bool GetUseScaling() { return m_UseScaling; };
    void SetUseScaling(bool val);

    uint8_t GetDrawStatusState() { return m_DrawStatusState; };
    void SetDrawStatusState(uint8_t val);

    bool GetDrawStumps() { return m_DrawStumps; };
    void SetDrawStumps(bool val);

    bool GetMarkingCaves() { return m_MarkingCaves; };
    void SetMarkingCaves(bool val);

    bool GetNoVegetation() { return m_NoVegetation; };
    void SetNoVegetation(bool val);

    bool GetNoAnimateFields() { return m_NoAnimateFields; };
    void SetNoAnimateFields(bool val);

    bool GetReduceFPSUnactiveWindow() { return m_ReduceFPSUnactiveWindow; };
    void SetReduceFPSUnactiveWindow(bool val);

    bool GetConsoleNeedEnter() { return m_ConsoleNeedEnter; };
    void SetConsoleNeedEnter(bool val);

    uint8_t GetSpellIconAlpha() { return m_SpellIconAlpha; };
    void SetSpellIconAlpha(uint8_t val);

    bool GetOldStyleStatusbar() { return m_OldStyleStatusbar; };
    void SetOldStyleStatusbar(bool val);

    bool GetOriginalPartyStatusbar() { return m_OriginalPartyStatusbar; };
    void SetOriginalPartyStatusbar(bool val);

    bool GetApplyStateColorOnCharacters() { return m_ApplyStateColorOnCharacters; };
    void SetApplyStateColorOnCharacters(bool val);

    bool GetChangeFieldsGraphic() { return m_ChangeFieldsGraphic; };
    void SetChangeFieldsGraphic(bool val);

    bool GetPaperdollSlots() { return m_PaperdollSlots; };
    void SetPaperdollSlots(bool val);

    uint8_t GetDrawAuraState() { return m_DrawAuraState; };
    void SetDrawAuraState(uint8_t val);

    bool GetScaleImagesInPaperdollSlots() { return m_ScaleImagesInPaperdollSlots; };
    void SetScaleImagesInPaperdollSlots(bool val);

    bool GetUseGlobalMapLayer() { return m_UseGlobalMapLayer; };
    void SetUseGlobalMapLayer(bool val);

    bool GetNoDrawRoofs() { return m_NoDrawRoofs; };
    void SetNoDrawRoofs(bool val);

    uint8_t GetPingTimer() { return m_PingTimer; };
    void SetPingTimer(uint8_t val);

    bool GetItemPropertiesIcon() { return m_ItemPropertiesIcon; };
    void SetItemPropertiesIcon(bool val);

    uint8_t GetItemPropertiesMode() { return m_ItemPropertiesMode; };
    void SetItemPropertiesMode(uint8_t val);

    uint8_t GetCharacterBackpackStyle() { return m_CharacterBackpackStyle; };
    void SetCharacterBackpackStyle(uint8_t val);

    void Init();

    //!Проставление значений по-умолчанию
    void DefaultPage1();
    void DefaultPage2();
    void DefaultPage3();
    void DefaultPage4();
    //5 нету
    void DefaultPage6();
    void DefaultPage7();
    void DefaultPage8();
    void DefaultPage9();
    //10 нету

    void UpdateFeatures();
    uint16_t GetColorByNotoriety(uint8_t notoriety);
    bool LoadBin(const os_path &path);
    int GetConfigKeyCode(const string &key);

    bool Load(const os_path &path);
    void Save(const os_path &path);
};

extern CConfigManager g_ConfigManager;
extern CConfigManager g_OptionsConfig;
