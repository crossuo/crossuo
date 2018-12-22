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

    enum CONFIG_MANAGER_KEY_CODE
    {
        CMKC_SOUND = 0,
        CMKC_SOUND_VOLUME,
        CMKC_MUSIC,
        CMKC_MUSIC_VOLUME,
        CMKC_FOOTSTEPS_SOUND,
        CMKC_COMBAT_MUSIC,
        CMKC_BACKGROUND_SOUND,
        CMKC_CLIENT_FPS,
        CMKC_USE_SCALING,
        CMKC_REMOVE_TEXT_WITH_BLENDING,
        CMKC_DRAW_STATUS_STATE,
        CMKC_DRAW_STUMPS,
        CMKC_MARKING_CAVES,
        CMKC_NO_ANIMATE_FIELDS,
        CMKC_NO_VEGETATION,
        CMKC_HIDDEN_CHARACTERS_RENDER_MODE,
        CMKC_HIDDEN_ALPHA,
        CMKC_USE_HIDDEN_MODE_ONLY_FOR_SELF,
        CMKC_TRANSPARENT_SPELL_ICONS,
        CMKC_SPELL_ICON_ALPHA,
        CMKC_OLD_STYLE_STATUSBAR,
        CMKC_ORIGINAL_PARTY_STATUSBAR,
        CMKC_APPLY_STATE_COLOR_ON_CHARACTERS,
        CMKC_CHANGE_FIELDS_GRAPHIC,
        CMKC_PAPERDOLL_SLOTS,
        CMKC_DRAW_STATUS_CONDITION_STATE,
        CMKC_DRAW_STATUS_CONDITION_VALUE,
        CMKC_REMOVE_STATUSBARS_WITHOUT_OBJECTS,
        CMKC_SHOW_DEFAULT_CONSOLE_ENTRY_MODE,
        CMKC_DRAW_AURA_STATE,
        CMKC_DRAW_AURA_WITH_CTRL_PRESSED,
        CMKC_SCREENSHOT_FORMAT,
        CMKC_SCALE_IMAGES_IN_PAPERDOLL_SLOTS,
        CMKC_REMOVE_OR_CREATE_OBJECTS_WITH_BLENDING,
        CMKC_DRAW_HELMETS_ON_SHROUD,
        CMKC_USE_GLOBAL_MAP_LAYER,
        CMKC_NO_DRAW_ROOFS,
        CMKC_HIGHLIGHT_TARGET_BY_TYPE,
        CMKC_AUTO_DISPLAY_WORLD_MAP,
        CMKC_UNUSED, // WAS GL DISPLAY LIST
        CMKC_USE_TOOLTIPS,
        CMKC_TOOLTIPS_TEXT_COLOR,
        CMKC_TOOLTIPS_TEXT_FONT,
        CMKC_TOOLTIPS_DELAY,
        CMKC_CHAT_COLOR_INPUT_TEXT,
        CMKC_CHAT_COLOR_MENU_OPTION,
        CMKC_CHAT_COLOR_PLAYER_IN_MEMBER_LIST,
        CMKC_CHAT_COLOR_TEXT,
        CMKC_CHAT_COLOR_PLAYER_NAME_WITHOUT,
        CMKC_CHAT_COLOR_MUTED,
        CMKC_CHAT_COLOR_CHANNEL_MODERATOR_NAME,
        CMKC_CHAT_COLOR_CHANNEL_MODERATOR_TEXT,
        CMKC_CHAT_COLOR_MY_NAME,
        CMKC_CHAT_COLOR_MY_TEXT,
        CMKC_CHAT_COLOR_SYSTEM_MESSAGE,
        CMKC_CHAT_FONT,
        CMKC_CHAT_COLOR_BG_OUTPUT_TEXT,
        CMKC_CHAT_COLOR_BG_INPUT_TEXT,
        CMKC_CHAT_COLOR_BG_USER_LIST,
        CMKC_CHAT_COLOR_BG_CONF_LIST,
        CMKC_CHAT_COLOR_BG_COMMAND_LIST,
        CMKC_ENABLE_PATHFIND,
        CMKC_HOLD_TAB_FOR_COMBAT,
        CMKC_OFFSET_INTERFACE_WINDOWS,
        CMKC_AUTO_ARRANGE,
        CMKC_ALWAYS_RUN,
        CMKC_DISABLE_MENUBAR,
        CMKC_GRAY_OUT_OF_RANGE_OBJECTS,
        CMKC_DISABLE_NEW_TARGET_SYSTEM,
        CMKC_ITEMP_ROPERTIES_MODE,
        CMKC_ITEMP_ROPERTIES_ICON,
        CMKC_OBJECT_HANDLES,
        CMKC_REDUCE_FPS_UNACTIVE_WINDOW,
        CMKC_HOLD_SHIFT_FOR_CONTEXT_MENUS,
        CMKC_HOLD_SHIFT_FOR_ENABLE_PATHFIND,
        CMKC_CONTAINER_DEFAULT_X,
        CMKC_CONTAINER_DEFAULT_Y,
        CMKC_GAME_WINDOW_WIDTH,
        CMKC_GAME_WINDOW_HEIGHT,
        CMKC_SPEECH_DELAY,
        CMKC_SCALE_SPEECH_DELAY,
        CMKC_SPEECH_COLOR,
        CMKC_EMOTE_COLOR,
        CMKC_PARTY_MESSAGE_COLOR,
        CMKC_GUILD_MESSAGE_COLOR,
        CMKC_ALLIANCE_MESSAGE_COLOR,
        CMKC_IGNORE_GUILD_MESSAGE,
        CMKC_IGNORE_ALLIANCE_MESSAGE,
        CMKC_DARK_NIGHTS,
        CMKC_COLORED_LIGHTING,
        CMKC_STANDART_CHARACTERS_ANIMATION_DELAY,
        CMKC_STANDART_ITEMS_ANIMATION_DELAY,
        CMKC_LOCK_RESIZING_GAME_WINDOW,
        CMKC_LOCK_GUMPS_MOVING,
        CMKC_INNOCENT_COLOR,
        CMKC_FRIENDLY_COLOR,
        CMKC_SOMEONE_COLOR,
        CMKC_CRIMINAL_COLOR,
        CMKC_ENEMY_COLOR,
        CMKC_MURDERER_COLOR,
        CMKC_CRIMINAL_ACTIONS_QUERY,
        CMKC_SHOW_INCOMING_NAMES,
        CMKC_USE_CIRCLE_TRANS,
        CMKC_STAT_REPORT,
        CMKC_CONSOLE_NEED_ENTER,
        CMKC_CIRCLE_TRANS_RADIUS,
        CMKC_SKILL_REPORT,
        CMKC_SPEECH_FONT,
        CMKC_GAME_WINDOW_X,
        CMKC_GAME_WINDOW_Y,
        CMKC_ZOOMED,
        CMKC_REAL_X,
        CMKC_REAL_Y,
        CMKC_REAL_WIDTH,
        CMKC_REAL_HEIGHT,
        CMKC_TOGGLE_BUFFICON_WINDOW,
        CMKC_DEVELOPER_MODE,
        CMKC_LAST_SERVER,
        CMKC_LAST_CHARACTER,
        CMKC_CHARACTER_BACKPACK_STYLE,
        CMKC_CHECK_PING,
        CMKC_PING_TIMER,
        CMKC_CANCEL_NEW_TARGET_SYSTEM_ON_SHIFT_ESC,
        CMKC_DRAW_STATUS_FOR_HUMANOIDS,
        CMKC_COUNT
    };

    bool Load(const os_path &path);

    void Save(const os_path &path);
};

extern CConfigManager g_ConfigManager;
extern CConfigManager g_OptionsConfig;
