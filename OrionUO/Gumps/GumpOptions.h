// MIT License
// Copyright (C) August 2016 Hotride

#pragma once
#include "Input.h"

class CGumpOptions : public CGump
{
private:
    static const char *m_HotkeyText[0x100];

    uint32_t m_LastChangeMacroTime{ 0 };

    static const int m_MacroListMaxCount = 20;

    void DrawPage1();  //Sound and Music
    void DrawPage2();  //Pop-up Help
    void DrawPage3();  //Language
    void DrawPage4();  //Chat
    void DrawPage5();  //Macro Options
    void DrawPage6();  //Interface
    void DrawPage7();  //Display
    void DrawPage8();  //Reputation System
    void DrawPage9();  //Miscellaneous
    void DrawPage10(); //Filter Options

    void ApplyPageChanges();

    static const int ID_GO_PAGE_1 = 1;
    static const int ID_GO_PAGE_2 = 2;
    static const int ID_GO_PAGE_3 = 3;
    static const int ID_GO_PAGE_4 = 4;
    static const int ID_GO_PAGE_5 = 5;
    static const int ID_GO_PAGE_6 = 6;
    static const int ID_GO_PAGE_7 = 7;
    static const int ID_GO_PAGE_8 = 8;
    static const int ID_GO_PAGE_9 = 9;
    static const int ID_GO_PAGE_10 = 10;
    static const int ID_GO_CANCEL = 11;
    static const int ID_GO_APPLY = 12;
    static const int ID_GO_DEFAULT = 13;
    static const int ID_GO_OKAY = 14;

    static const int ID_GO_P1_SOUND_ON_OFF = 100;
    static const int ID_GO_P1_MUSIC_ON_OFF = 101;
    static const int ID_GO_P1_PLAY_FOOTSTEP_SOUNDS = 102;
    static const int ID_GO_P1_PLAY_COMBAT_MUSIC = 103;
    static const int ID_GO_P1_SOUND_VOLUME = 104;
    static const int ID_GO_P1_MUSIC_VOLUME = 105;
    static const int ID_GO_P1_BACKGROUND_SOUND = 106;

    static const int ID_GO_P2_CLIENT_FPS = 200;
    static const int ID_GO_P2_REDUCE_FPS_UNACTIVE_WINDOW = 201;
    static const int ID_GO_P2_CHARACTERS_ANIMATION_DELAY = 202;
    static const int ID_GO_P2_ITEMS_ANIMATION_DELAY = 203;
    static const int ID_GO_P2_ENABLE_SCALING = 204;
    static const int ID_GO_P2_REMOVE_TEXT_WITH_BLENDING = 205;
    static const int ID_GO_P2_NO_DRAW_CHARACTERS_STATUS = 206;
    static const int ID_GO_P2_DRAW_CHARACTERS_STATUS_TOP = 207;
    static const int ID_GO_P2_DRAW_CHARACTERS_STATUS_BOTTOM = 208;
    static const int ID_GO_P2_DRAW_STUMPS = 209;
    static const int ID_GO_P2_MARKING_CAVES = 210;
    static const int ID_GO_P2_NO_VEGETATION = 211;
    static const int ID_GO_P2_NO_ANIMATE_FIELDS = 212;
    static const int ID_GO_P2_LOCK_GUMP_MOVING = 213;
    static const int ID_GO_P2_CONSOLE_ENTER = 214;
    static const int ID_GO_P2_HIDDEN_CHARACTES_MODE_1 = 215;
    static const int ID_GO_P2_HIDDEN_CHARACTES_MODE_2 = 216;
    static const int ID_GO_P2_HIDDEN_CHARACTES_MODE_3 = 217;
    static const int ID_GO_P2_HIDDEN_CHARACTES_MODE_4 = 218;
    static const int ID_GO_P2_HIDDEN_ALPHA = 219;
    static const int ID_GO_P2_USE_HIDDEN_MODE_ONLY_FOR_SELF = 220;
    static const int ID_GO_P2_TRANSPARENT_SPELL_ICONS = 221;
    static const int ID_GO_P2_SPELL_ICONS_ALPHA = 222;
    static const int ID_GO_P2_OLD_STYLE_STATUSBAR = 223;
    static const int ID_GO_P2_ORIGINAL_PARTY_STATUSBAR = 224;
    static const int ID_GO_P2_APPLY_STATE_COLOR_ON_CHARACTERS = 225;
    static const int ID_GO_P2_CHANGE_FIELDS_GRAPHIC = 226;
    static const int ID_GO_P2_PAPERDOLL_SLOTS = 227;
    static const int ID_GO_P2_DRAW_CHARACTER_BARS_ALWAYS = 228;
    static const int ID_GO_P2_DRAW_CHARACTER_BARS_NOT_MAX = 229;
    static const int ID_GO_P2_DRAW_CHARACTER_BARS_LOWER = 230;
    static const int ID_GO_P2_DRAW_CHARACTER_BARS_LOWER_VALUE = 231;
    static const int ID_GO_P2_REMOVE_STATUSBARS_WITHOUT_OBJECTS = 232;
    static const int ID_GO_P2_SHOW_CONSOLE_ENTRY_MODE = 233;
    static const int ID_GO_P2_DRAW_AURA_NEVER = 234;
    static const int ID_GO_P2_DRAW_AURA_IN_WARMODE = 235;
    static const int ID_GO_P2_DRAW_AURA_ALWAYS = 236;
    static const int ID_GO_P2_DRAW_AURA_WITH_CTRL_PRESSED = 237;
    static const int ID_GO_P2_SCREENSHOT_FORMAT_BMP = 238;
    static const int ID_GO_P2_SCREENSHOT_FORMAT_PNG = 239;
    static const int ID_GO_P2_SCREENSHOT_FORMAT_TIFF = 240;
    static const int ID_GO_P2_SCREENSHOT_FORMAT_JPEG = 241;
    static const int ID_GO_P2_SCALE_IMAGES_IN_PAPERDOLL_SLOTS = 242;
    static const int ID_GO_P2_REMOVE_OR_CREATE_OBJECTS_WITH_BLENDING = 243;
    static const int ID_GO_P2_DRAW_HELMETS_ON_SHROUD = 244;
    static const int ID_GO_P2_USE_GLOBAL_MAP_LAYER = 245;
    static const int ID_GO_P2_NO_DRAW_ROOFS = 246;
    static const int ID_GO_P2_HIGHLIGHT_TARGET_BY_TYPE = 247;
    static const int ID_GO_P2_AUTO_DISPLAY_WORLD_MAP = 248;
    static const int ID_GO_P2_USE_GL_LISTS_FOR_INTERFACE = 249;
    static const int ID_GO_P2_CHECK_PING = 250;
    static const int ID_GO_P2_PING_TIMER = 251;
    static const int ID_GO_P2_CANCEL_NEW_TARGET_SYSTEM_ON_SHIFT_ESC = 252;
    static const int ID_GO_P2_DRAW_STATUS_FOR_HUMANOIDS = 253;
    static const int ID_GO_P2_DEV_MODE_1 = 290;
    static const int ID_GO_P2_DEV_MODE_2 = 291;
    static const int ID_GO_P2_DEV_MODE_3 = 292;

    static const int ID_GO_P3_USE_TOOLTIP = 300;
    static const int ID_GO_P3_TEXT_COLOR = 301;
    static const int ID_GO_P3_TEXT_FONT = 302;
    static const int ID_GO_P3_DELAY_BEFORE_TOOLTIP = 303;

    static const int ID_GO_P4_TEXT_FONT = 400;
    static const int ID_GO_P4_TEXT_COLOR = 401;

    static const int ID_GO_P5_BUTTON_ADD = 500;
    static const int ID_GO_P5_BUTTON_DELETE = 501;
    static const int ID_GO_P5_BUTTON_PREVEOUS = 502;
    static const int ID_GO_P5_BUTTON_NEXT = 503;
    static const int ID_GO_P5_KEY_BOX = 504;
    static const int ID_GO_P5_BUTTON_SHIFT = 505;
    static const int ID_GO_P5_BUTTON_ALT = 506;
    static const int ID_GO_P5_BUTTON_CTRL = 507;
    static const int ID_GO_P5_BUTTON_UP = 508;
    static const int ID_GO_P5_BUTTON_DOWN = 509;
    static const int ID_GO_P5_LEFT_BOX = 510;
    static const int ID_GO_P5_RIGHT_BOX = 520;
    static const int ID_GO_P5_EMPTY_BOX = 530;
    ///////////////////////////////////////////
    static const int ID_GO_P5_MACRO_SELECTION = 2000;
    static const int ID_GO_P5_ACTION_SELECTION = 10000;

    static const int ID_GO_P6_ENABLE_PATHFINDING = 600;
    static const int ID_GO_P6_HOLD_TAB_FOR_COMBAT = 601;
    static const int ID_GO_P6_OFFSET_INTERFACE_WINDOWS = 602;
    static const int ID_GO_P6_AUTO_ARRANGE_MINIMIZED_WINDOWS = 603;
    static const int ID_GO_P6_ALWAYS_RUN = 604;
    static const int ID_GO_P6_DISABLE_MENUBAR = 605;
    static const int ID_GO_P6_GRAY_OUT_OF_RANGE_OBJECTS = 606;
    static const int ID_GO_P6_DISABLE_NEW_TARGET_SYSTEM = 607;
    static const int ID_GO_P6_OBJECT_HANDLES = 608;
    static const int ID_GO_P6_DISPLAY_ITEM_PROPERTIES_MODE_AT_ICON = 609;
    static const int ID_GO_P6_DISPLAY_ITEM_PROPERTIES_MODE_ALWAYS_UP = 610;
    static const int ID_GO_P6_DISPLAY_ITEM_PROPERTIES_MODE_FOLLOW_MOUSE = 611;
    static const int ID_GO_P6_DISPLAY_ITEM_PROPERTIES_MODE_SINGLE_CLICK = 612;
    static const int ID_GO_P6_DISPLAY_ITEM_PROPERTIES_ICON = 613;
    static const int ID_GO_P6_HOLD_SHIFT_FOR_CONTEXT_MENUS = 614;
    static const int ID_GO_P6_HOLD_SHIFT_FOR_ENABLE_PATHFINDING = 615;
    static const int ID_GO_P6_CONTAINER_OFFSET_X = 616;
    static const int ID_GO_P6_CONTAINER_OFFSET_Y = 617;
    static const int ID_GO_P6_CHARACTER_BACKPACK_STYLE_DEFAULT = 618;
    static const int ID_GO_P6_CHARACTER_BACKPACK_STYLE_SUEDE = 619;
    static const int ID_GO_P6_CHARACTER_BACKPACK_STYLE_POLAR_BEAR = 620;
    static const int ID_GO_P6_CHARACTER_BACKPACK_STYLE_GHOUL_SKIN = 621;

    static const int ID_GO_P7_SCALE_SPEECH_DURATION = 700;
    static const int ID_GO_P7_SPEECH_COLOR = 701;
    static const int ID_GO_P7_EMOTE_COLOR = 702;
    static const int ID_GO_P7_PARTY_MESSAGE_COLOR = 703;
    static const int ID_GO_P7_GUILD_MESSAGE_COLOR = 704;
    static const int ID_GO_P7_ALLIANCE_MESSAGE_COLOR = 705;
    static const int ID_GO_P7_IGNORE_GUILD_MESSAGE = 706;
    static const int ID_GO_P7_IGNORE_ALLIANCE_MESSAGE = 707;
    static const int ID_GO_P7_DARK_NIGHTS = 708;
    static const int ID_GO_P7_COLORED_LIGHTING = 709;
    static const int ID_GO_P7_AJUST_LONG_SPEECH = 710;
    static const int ID_GO_P7_GAME_WINDOW_WIDTH = 711;
    static const int ID_GO_P7_GAME_WINDOW_HEIGHT = 712;
    static const int ID_GO_P7_LOCK_GAME_WINDOW_RESIZING = 713;

    static const int ID_GO_P8_INNOCENT_COLOR = 800;
    static const int ID_GO_P8_FRIENDLY_COLOR = 801;
    static const int ID_GO_P8_SOMEONE_COLOR = 802;
    static const int ID_GO_P8_CRIMINAL_COLOR = 803;
    static const int ID_GO_P8_ENEMY_COLOR = 804;
    static const int ID_GO_P8_MURDERER_COLOR = 805;
    static const int ID_GO_P8_QUERY_CRIMINAL_ACTIONS = 806;

    static const int ID_GO_P9_SHOW_APPROACHING_NAMES = 900;
    static const int ID_GO_P9_USE_CIRCLE_OF_TRANSPARENCY = 901;
    static const int ID_GO_P9_INFORM_STATS = 902;
    static const int ID_GO_P9_SPEECH_FONT = 903;
    static const int ID_GO_P9_TRANSPARENCY_RADIUS = 904;
    static const int ID_GO_P9_INFORM_SKILLS = 905;

    class CMacro *m_MacroPointer{ nullptr };
    class CMacroObject *m_MacroObjectPointer{ nullptr };

    CGUISlider *m_SliderSound{ nullptr };
    CGUISlider *m_SliderMusic{ nullptr };
    CGUISlider *m_SliderClientFPS{ nullptr };
    CGUISlider *m_SliderHiddenAlpha{ nullptr };
    CGUISlider *m_SliderSpellIconsAlpha{ nullptr };
    CGUISlider *m_SliderDrawStatusConditionValue{ nullptr };
    CGUISlider *m_SliderTooltipDelay{ nullptr };
    CGUISlider *m_SliderSpeechDuration{ nullptr };
    CGUISlider *m_SliderCircleTransparency{ nullptr };
    CGUISlider *m_SliderInformSkills{ nullptr };
    CGUISlider *m_SliderPingTimer{ nullptr };

    CGUIColoredPolygone *m_ColorTooltipText{ nullptr };

    CGUIColoredPolygone *m_ColorInputText{ nullptr };
    CGUIColoredPolygone *m_ColorMenuOption{ nullptr };
    CGUIColoredPolygone *m_ColorPlayerColorInMemberList{ nullptr };
    CGUIColoredPolygone *m_ColorChatText{ nullptr };
    CGUIColoredPolygone *m_ColorPlayerNameWithoutSpeakingPrivileges{ nullptr };
    CGUIColoredPolygone *m_ColorMutedText{ nullptr };
    CGUIColoredPolygone *m_ColorChannelModeratorName{ nullptr };
    CGUIColoredPolygone *m_ColorChannelModeratorText{ nullptr };
    CGUIColoredPolygone *m_ColorMyName{ nullptr };
    CGUIColoredPolygone *m_ColorMyText{ nullptr };
    CGUIColoredPolygone *m_ColorSystemMessage{ nullptr };
    CGUIColoredPolygone *m_ColorTextOutputBackground{ nullptr };
    CGUIColoredPolygone *m_ColorTextInputBackground{ nullptr };
    CGUIColoredPolygone *m_ColorUserListBackground{ nullptr };
    CGUIColoredPolygone *m_ColorConferenceListBackground{ nullptr };
    CGUIColoredPolygone *m_ColorCommandListBackground{ nullptr };

    CGUIColoredPolygone *m_ColorSpeech{ nullptr };
    CGUIColoredPolygone *m_ColorEmote{ nullptr };
    CGUIColoredPolygone *m_ColorPartyMessage{ nullptr };
    CGUIColoredPolygone *m_ColorGuildMessage{ nullptr };
    CGUIColoredPolygone *m_ColorAllianceMessage{ nullptr };

    CGUIColoredPolygone *m_ColorInnocent{ nullptr };
    CGUIColoredPolygone *m_ColorFriendly{ nullptr };
    CGUIColoredPolygone *m_ColorSomeone{ nullptr };
    CGUIColoredPolygone *m_ColorCriminal{ nullptr };
    CGUIColoredPolygone *m_ColorEnemy{ nullptr };
    CGUIColoredPolygone *m_ColorMurderer{ nullptr };

    CGUIDataBox *m_MacroDataBox{ nullptr };
    CGUICheckbox *m_MacroCheckboxShift{ nullptr };
    CGUICheckbox *m_MacroCheckboxAlt{ nullptr };
    CGUICheckbox *m_MacroCheckboxCtrl{ nullptr };

    CGUITextEntry *m_ContainerOffsetX{ nullptr };
    CGUITextEntry *m_ContainerOffsetY{ nullptr };

    bool m_WantRedrawMacroData{ true };
    void RedrawMacroData();

protected:
    virtual void CalculateGumpState();

public:
    CGumpOptions(short x, short y);
    virtual ~CGumpOptions();

    CGUITextEntry *m_GameWindowWidth{ nullptr };
    CGUITextEntry *m_GameWindowHeight{ nullptr };
    CGUITextEntry *m_MacroKey{ nullptr };

    void UpdateColor(const SELECT_COLOR_GUMP_STATE &state, uint16_t color);
    virtual void InitToolTip();
    virtual void PrepareContent();
    virtual void UpdateContent();
    void Init();

    GUMP_BUTTON_EVENT_H;
    GUMP_CHECKBOX_EVENT_H;
    GUMP_RADIO_EVENT_H;
    GUMP_COMBOBOX_SELECTION_EVENT_H;
    GUMP_SLIDER_CLICK_EVENT_H;
    GUMP_SLIDER_MOVE_EVENT_H;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};
