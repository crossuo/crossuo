// MIT License
// Copyright (C) August 2016 Hotride
// Copyright (c) 2019 CrossUO Team

#pragma once

enum GAME_STATE
{
    GS_MAIN = 0,          // The main gump (the client just started)
    GS_MAIN_CONNECT,      // Gump connect to the authorization server
    GS_SERVER,            // Server list gump
    GS_SERVER_CONNECT,    // Connection to the game server
    GS_CHARACTER,         // Enchantment Gump
    GS_DELETE,            // Character removal gump
    GS_PROFESSION_SELECT, // Gump choice of profession
    GS_CREATE,            // Character creation gump
    GS_SELECT_TOWN,       // Gump selection of the starting city
    GS_GAME_CONNECT,      // Game entry gump
    GS_GAME,              // Game gump (world mapping and other things)
    GS_GAME_BLOCKED       /*!Modification of the game board, everything is displayed, but blocked
    for clicks, movements and changes of anything other than a gump
        caused this stage render */
};

enum RENDER_OBJECT_TYPE
{
    ROT_NO_OBJECT = 0, // Nothing selected
    ROT_GAME_OBJECT,   // Game object
    ROT_STATIC_OBJECT, // Static
    ROT_LAND_OBJECT,   // Landscape
    ROT_MULTI_OBJECT,  // Multi
    ROT_EFFECT         // Effect
};

enum SCAN_TYPE_OBJECT
{
    STO_HOSTILE = 0,
    STO_PARTY,
    STO_FOLLOWERS,
    STO_OBJECTS,
    STO_MOBILES
};

enum SCAN_MODE_OBJECT
{
    SMO_NEXT = 0,
    SMO_PREV,
    SMO_NEAREST
};

enum EFFECT_TYPE
{
    EF_MOVING = 0,     // Moving
    EF_LIGHTING,       // Lightning Strike
    EF_STAY_AT_POS,    // Standing on position
    EF_STAY_AT_SOURCE, // Tied to the source
    EF_DRAG            // Item movement animation
};

enum CHARACTER_SPEED_TYPE
{
    CST_NORMAL = 0,   // Normal speed
    CST_FAST_UNMOUNT, // Acceleration of the character's movement when it moves without a mount
    CST_CANT_RUN,     // Only walk, cannot run with or without a mount
    CST_FAST_UNMOUNT_AND_CANT_RUN // Combine both previous: char cannot run and accelerate only if without mount
};

enum ENCRYPTION_TYPE
{
    ET_NOCRYPT = 0,
    ET_OLD_BFISH,
    ET_1_25_36, // Client specific: 1.25.36 (BlowFish, login encryption algo changed)
    ET_BFISH,
    ET_203,  // Special for client 2.0.3 (BlowFish + TwoFish without MD5)
    ET_TFISH // TwoFish + MD5
};

// A useful list with specific features on specific client versions here:
// https://github.com/Sphereserver/Source-experimental/blob/addff8a9ba29d737ea281544a9bd7878dd8fa8cc/src/common/sphereproto.h#L1006

#define VERSION(a, b, c, d)                                                                        \
    (((a & 0xff) << 24) | ((b & 0xff) << 16) | ((c & 0xff) << 8) | (d & 0xff))

enum CLIENT_VERSION
{
    CV_OLD = VERSION(0, 0, 0, 0),    // CF_T2A, Standard (<2.0.0)
    CV_200 = VERSION(2, 0, 0, 0),    // CF_RE, Packet sent with screen dimensions
    CV_200X = VERSION(2, 0, 0, 'x'), // Special Crypto keys
    //CV_204C, // Introduced *.def files
    CV_300 = VERSION(3, 0, 0, 0), // CF_TD
    // Using cliloc. Numer of slots in list of characters is equal to number of characters
    CV_305D = VERSION(3, 0, 5, 'd'),
    // Introduced packet with client type (0xBF subcmd 0x0F), uses mp3 instead of midi
    CV_306E = VERSION(3, 0, 6, 'e'),
    CV_308 = VERSION(3, 0, 8, 0),    // CF_LBR
    CV_308D = VERSION(3, 0, 8, 'd'), // Added "Maximum Stats" to the statusbar
    CV_308J = VERSION(3, 0, 8, 'j'), // Added "Followers" to the statsubar
    // Added classes: paladin, necromancer; custom houses, 5 resistances, changed professions choice screen, removed "Save Password" checkbox
    CV_308Z = VERSION(3, 0, 8, 'z'),
    CV_400B = VERSION(4, 0, 0, 'b'),   // Removed tooltips
    CV_405A = VERSION(4, 0, 5, 'a'),   // CF_SE, Added classes: ninja, samurai
    CV_4011D = VERSION(4, 0, 11, 'd'), // Changed the character creation screen. Added elf race.
    // Paperdoll buttons replaced: Journal->Quests; Chat->Guild. Using Mega Cliloc, removed loading of Verdata.mul
    CV_500A = VERSION(5, 0, 0, 'a'),
    CV_5020 = VERSION(5, 0, 2, 0), // Added buffs gump
    CV_5090 = VERSION(5, 0, 9, 0), //
    // Added colors guild / alli chat, chat ignore. Options for a new system target, diplaying object properties, object handles
    CV_6000 = VERSION(6, 0, 0, 0),
    CV_6013 = VERSION(6, 0, 1, 3),   //
    CV_6017 = VERSION(6, 0, 1, 7),   //
    CV_6040 = VERSION(6, 0, 4, 0),   // Increased number of character slots
    CV_6060 = VERSION(6, 0, 6, 0),   //
    CV_60142 = VERSION(6, 0, 14, 2), //
    // CF_SA, Changed the character creation screen. Added gargoyle race
    CV_60144 = VERSION(6, 0, 14, 4),
    CV_7000 = VERSION(7, 0, 0, 0),   //
    CV_7090 = VERSION(7, 0, 9, 0),   //
    CV_70130 = VERSION(7, 0, 13, 0), //
    CV_70160 = VERSION(7, 0, 16, 0), //
    CV_70180 = VERSION(7, 0, 18, 0), //
    CV_70240 = VERSION(7, 0, 24, 0), // *.mul -> *.uop
    CV_70331 = VERSION(7, 0, 33, 1), //
    CV_LATEST = CV_70331,
};

enum CONNECTION_SCREEN_TYPE
{
    CST_LOGIN = 0,
    CST_CHARACTER_LIST,
    CST_CONLOST,
    CST_SELECT_PROFESSOIN,
    CST_GAME_LOGIN,
    CST_GAME
};

enum GENDER : uint8_t
{
    GENDER_MALE = 0,
    GENDER_FEMALE,
};

enum RACE_TYPE
{
    RT_HUMAN = 1,
    RT_ELF,
    RT_GARGOYLE
};

enum PROFESSION_TYPE
{
    PT_NO_PROF = 0,
    PT_CATEGORY,
    PT_PROFESSION
};

enum CLIENT_FLAG
{
    CF_T2A = 0x00,
    CF_RE = 0x01,
    CF_TD = 0x02,
    CF_LBR = 0x04,
    CF_AOS = 0x08,
    CF_SE = 0x10,
    CF_SA = 0x20,
    CF_UO3D = 0x40,
    CF_RESERVED = 0x80,
    CF_3D = 0x100,
    CF_UNDEFINED = 0xFFFF,
};

enum CHARACTER_LIST_FLAG
{
    CLF_UNKNOWN = 0x01,
    CLF_OWERWRITE_CONFIGURATION_BUTTON = 0x02,
    CLF_ONE_CHARACTER_SLOT = 0x04,
    CLF_CONTEXT_MENU = 0x08,
    CLF_LIMIT_CHARACTER_SLOTS = 0x10,
    CLF_PALADIN_NECROMANCER_TOOLTIPS = 0x20,
    CLF_6_CHARACTER_SLOT = 0x40,
    CLF_SAMURAI_NINJA = 0x80,
    CLF_ELVEN_RACE = 0x100,
    CLF_UNKNOWN_1 = 0x200,
    CLF_UO3D = 0x400,
    CLF_UNKNOWN_2 = 0x800,
    CLF_7_CHARACTER_SLOT = 0x1000,
    CLF_UNKNOWN_3 = 0x2000,
    CLF_NEW_MOVEMENT_SYSTEM = 0x4000,
    CLF_UNLOCK_FELUCCA_AREAS = 0x8000
};

enum LOCKED_FEATURE_FLAG
{
    LFF_T2A = 0x01,
    LFF_RE = 0x02,
    LFF_TD = 0x04,
    LFF_LBR = 0x08,
    LFF_AOS = 0x10,
    LFF_6_SLOT = 0x20,
    LFF_SE = 0x40,
    LFF_ML = 0x80,
    LFF_8_AGE = 0x100,
    LFF_9_AGE = 0x200,
    LFF_10_AGE = 0x400,
    LFF_HOUSING = 0x800,
    LFF_7_SLOT = 0x1000,
    LFF_KR = 0x2000,
    LFF_TRIAL_ACC = 0x4000,
    LFF_11_AGE = 0x8000,
    LFF_SA = 0x10000,
    LFF_HSA = 0x20000,
    LFF_GOTHIC_HOUSING = 0x40000,
    LFF_RUSTIC_HOUSING = 0x80000
};

enum SPELLBOOK_TYPE
{
    ST_MAGE = 0,
    ST_NECRO,
    ST_PALADIN,
    ST_BUSHIDO = 4,
    ST_NINJITSU,
    ST_SPELL_WEAVING,
    ST_MYSTICISM
};

enum SPELLBOOK_OFFSET
{
    SO_MAGE = 1,
    SO_NECRO = 101,
    SO_PALADIN = 201,
    SO_BUSHIDO = 301,
    SO_NINJITSU = 401,
    SO_SPELL_WEAVING = 501,
    SO_MYSTICISM = 601
};

enum SCREEN_EFFECT_MODE
{
    SEM_NONE = 0,
    SEM_SUNRISE,
    SEM_SUNSET
};

enum SCREEN_EFFECT_TYPE
{
    SET_TO_BLACK,
    SET_TO_WHITE,
    SET_TO_WHITE_FAST,
    SET_TO_WHITE_THEN_BLACK,
    SET_TO_BLACK_VERY_FAST
};

enum WEATHER_TYPE
{
    WT_RAIN = 0,
    WT_FIERCE_STORM,
    WT_SNOW,
    WT_STORM
};

enum GUMP_TYPE
{
    GT_NONE = 0,              // No gump
    GT_PAPERDOLL,             // Paperdoll 100%
    GT_STATUSBAR,             // Status bar (all types) 100%
    GT_DYE,                   // Color selection gump 100% (directly depends on GT_SELECT_COLOR)
    GT_OPTIONS,               // Options 90%
    GT_GENERIC,               // Gump from the server 100%
    GT_MENU,                  // Menu from the server 100%
    GT_GRAY_MENU,             // "Gray" menu from the server (blocking) 100%
    GT_TRADE,                 // Trade Gump 100%
    GT_SHOP,                  // Purchase / sale gumps 100% / 100%
    GT_MINIMAP,               // Minimap 100%
    GT_MAP,                   // Map (cities / treasures) 100%
    GT_SKILLS,                // Skills 100%
    GT_JOURNAL,               // Magazine 100%
    GT_CONTAINER,             // Containers 100%
    GT_SPELLBOOK,             // Spellbook 100%
    GT_SPELL,                 // Spell icon 100%
    GT_DRAG,                  // Separation of stackable items (more than 1 in the stack) 100%
    GT_TEXT_ENTRY_DIALOG,     // Server input text input (blocking) 100%
    GT_QUESTION,              // Question gump (Blocking) 100%
    GT_SELECT_COLOR,          // Color selection gump (for options) 100%
    GT_SELECT_FONT,           // Font selection gump (for options) 100%
    GT_NOTIFY,                // Message gump (blocking) 100%
    GT_BOOK,                  // Gump books 10%
    GT_TIP,                   // Tip window 90%
    GT_PARTY_MANIFEST,        // Party manifesto gump 100%
    GT_MAIL,                  // Gump mail 0%
    GT_PROFILE,               // Profile gump 100%
    GT_MENUBAR,               // Gump menu bar 90%
    GT_BUFF,                  // Gump buffs 100%
    GT_CHAT,                  // Gump chat 0%
    GT_BULLETIN_BOARD,        // Gump message boards 100%
    GT_BULLETIN_BOARD_ITEM,   // Item Board Bulletin 100%
    GT_WORLD_MAP,             // Hamp World Maps 100%
    GT_PLUGIN,                // Plugins 0%
    GT_TARGET_SYSTEM,         // Gump system target (statusbar) 100%
    GT_POPUP_MENU,            // Gump pop-up menu 100%
    GT_CONSOLE_TYPE,          // Gump console type 100%
    GT_SKILL,                 // Gump skill 100%
    GT_COMBAT_BOOK,           // Gump books abilok 100%
    GT_ABILITY,               // Gump skills 100%
    GT_RACIAL_ABILITIES_BOOK, // Gump books of race abilities 100%
    GT_RACIAL_ABILITY,        // Gump racial abilities 100%
    GT_CUSTOM_HOUSE,
    GT_PROPERTY,
    GT_PROPERTY_ICON
};

enum GUMP_OBJECT_TYPE
{
    GOT_NONE = 0,        // No type
    GOT_PAGE,            // Page ID
    GOT_GROUP,           // Group ID (for radio buttons)
    GOT_MASTERGUMP,      // ?
    GOT_RESIZEPIC,       // Background (molded from 9 pieces)
    GOT_SCOPE,           // Frame (molded from 4 pieces)
    GOT_CHECKTRANS,      // Translucent zone on the gump
    GOT_SCISSOR,         // Scissors
    GOT_COLOREDPOLYGONE, // Colored rectangle
    GOT_LINE,            // Line
    GOT_SLIDER,          // Slider
    GOT_MINMAXBUTTONS,   // Adjustment buttons for min / max values
    GOT_SHADER,          // Shader program
    GOT_HITBOX,          // Selection area (for text)
    GOT_DATABOX, // Objects for drawing (actual use in the guards of the paperdoll, container, etc., containing objects of the world)
    GOT_BUTTON,  // Button
    GOT_RESIZEBUTTON,        // Resize button
    GOT_BUTTONTILEART,       // Button with a picture
    GOT_BLENDING,            // Color blending
    GOT_GLOBAL_COLOR,        // Global color
    GOT_BUFF,                // Buff icon / debuff
    GOT_SHOPITEM,            // Store item
    GOT_SHOPRESULT,          // Store item (result)
    GOT_SKILLITEM,           // Skill Object
    GOT_SKILLGROUP,          // Skill group object
    GOT_BB_OBJECT,           // Bulletin Board Object
    GOT_SCROLLBACKGROUND,    // Background as a scroll
    GOT_MENUOBJECT,          // Menu object
    GOT_COMBOBOX,            // Combo Box
    GOT_CHECKBOX,            // Checkbox
    GOT_RADIO,               // Radio button
    GOT_EXTERNALTEXTURE,     // External texture
    GOT_TILEPIC,             // Static picture
    GOT_TILEPICHIGHTLIGHTED, // Static picture, color backlit with serial
    GOT_GUMPPIC,             // Picture from the gump
    GOT_GUMPPICTILED,        // Picture from the gump (repeated)
    GOT_GUMPPICHIGHTLIGHTED, // Picture from a gump, color backlit with serial
    GOT_TEXT,                // Text
    GOT_HTMLTEXT,            // Text with HTML markup support
    GOT_TEXTENTRY,           // Enter text
    GOT_HTMLGUMP,            // CTML Gump
    GOT_XFMHTMLGUMP,         // XTML clump gump (color ibid)
    GOT_XFMHTMLTOKEN,        //
    GOT_TOOLTIP,             //
    GOT_VIRTUE_GUMP          //
};

enum SLIDER_TEXT_POSITION
{
    STP_TOP = 0,
    STP_BOTTOM,
    STP_LEFT,
    STP_RIGHT,
    STP_TOP_CENTER,
    STP_BOTTOM_CENTER,
    STP_LEFT_CENTER,
    STP_RIGHT_CENTER
};

enum SELECT_COLOR_GUMP_STATE
{
    //SCGS_OPT_POPUP_TEXT = 0,
    SCGS_OPT_TOOLTIP_TEXT = 0,
    SCGS_OPT_CHAT_INPUT_TEXT,
    SCGS_OPT_CHAT_MENU_OPTION,
    SCGS_OPT_CHAT_PLAYER_IN_MEMBER_LIST,
    SCGS_OPT_CHAT_TEXT,
    SCGS_OPT_CHAT_PLAYER_WITHOUT_PRIV,
    SCGS_OPT_CHAT_MUTED_TEXT,
    SCGS_OPT_CHAT_CHAN_MODER_NAME,
    SCGS_OPT_CHAT_CHAN_MODER_TEXT,
    SCGS_OPT_CHAT_MY_NAME,
    SCGS_OPT_CHAT_MY_TEXT,
    SCGS_OPT_CHAT_SYSTEM_MESSAGE,
    SCGS_OPT_CHAT_BG_OUTPUT,
    SCGS_OPT_CHAT_BG_INPUT,
    SCGS_OPT_CHAT_BG_USER_LIST,
    SCGS_OPT_CHAT_BG_CONF_LIST,
    SCGS_OPT_CHAT_BG_COMMAND_LIST,
    SCGS_OPT_DISPLAY_SPEECH,
    SCGS_OPT_DISPLAY_EMOTE,
    SCGS_OPT_DISPLAY_PARTY_MESSAGE,
    SCGS_OPT_DISPLAY_GUILD_MESSAGE,
    SCGS_OPT_DISPLAY_ALLIANCE_MESSAGE,
    SCGS_OPT_REPSYS_INNOCENT,
    SCGS_OPT_REPSYS_FRIENDLY,
    SCGS_OPT_REPSYS_SOMEONE,
    SCGS_OPT_REPSYS_CRIMINAL,
    SCGS_OPT_REPSYS_ENEMY,
    SCGS_OPT_REPSYS_MURDERER
};

enum SELECT_FONT_GUMP_STATE
{
    SFGS_OPT_POPUP = 1,
    SFGS_OPT_TOOLTIP,
    SFGS_OPT_CHAT,
    SFGS_OPT_MISCELLANEOUS
};

enum MAP_MESSAGE
{
    MM_ADD = 1,
    MM_INSERT,
    MM_MOVE,
    MM_REMOVE,
    MM_CLEAR,
    MM_EDIT,
    MM_EDIT_RESPONSE
};

enum UPDATE_GAME_OBJECT_TYPE
{
    UGOT_ITEM = 0,
    UGOT_NEW_ITEM = 1,
    UGOT_MULTI = 2,
};

enum XUO_COMMAND_TYPE
{
    OCT_RESERVED = 1,
    OCT_XUO_FEATURES = 50,
    OCT_XUO_IGNORE_TILES_IN_FILTER = 51,
    OCT_XUO_VERSION = 52,
    OCT_CLOSE_GENERIC_GUMP_WITHOUT_RESPONSE = 100,
    OCT_SELECT_MENU = 101,
    OCT_CAST_SPELL_REQUEST = 102,
    OCT_USE_SKILL_REQUEST = 103,
    OCT_DRAW_STATUSBAR = 104,
    OCT_CLOSE_STATUSBAR = 105,
    OCT_SECURE_TRADE_CHECK = 106,
    OCT_SECURE_TRADE_CLOSE = 107,
    OCT_UNICODE_SPEECH_REQUEST = 108,
    OCT_RENAME_MOUNT_REQUEST = 109,
    OCT_RECONNECT = 110,
    OCT_PLAY_MACRO = 111,
    OCT_MOVE_PAPERDOLL = 112,
    OCT_USE_ABILITY = 113,
    OCT_OPEN_DOOR = 114
};

enum VALUE_KEY_INT
{
    VKI_SOUND = 0,
    VKI_SOUND_VALUE,
    VKI_MUSIC,
    VKI_MUSIC_VALUE,
    VKI_USE_TOOLTIPS,
    VKI_ALWAYS_RUN,
    VKI_NEW_TARGET_SYSTEM,
    VKI_OBJECT_HANDLES,
    VKI_SCALE_SPEECH_DELAY,
    VKI_SPEECH_DELAY,
    VKI_IGNORE_GUILD_MESSAGES,
    VKI_IGNORE_ALLIANCE_MESSAGES,
    VKI_DARK_NIGHTS,
    VKI_COLORED_LIGHTING,
    VKI_CRIMINAL_ACTIONS_QUERY,
    VKI_CIRCLETRANS,
    VKI_CIRCLETRANS_VALUE,
    VKI_LOCK_RESIZING_GAME_WINDOW,
    VKI_CLIENT_FPS_VALUE,
    VKI_USE_SCALING_GAME_WINDOW,
    VKI_DRAW_STATUS_STATE,
    VKI_DRAW_STUMPS,
    VKI_MARKING_CAVES,
    VKI_NO_VEGETATION,
    VKI_NO_ANIMATE_FIELDS,
    VKI_STANDARD_CHARACTERS_DELAY,
    VKI_STANDARD_ITEMS_DELAY,
    VKI_LOCK_GUMPS_MOVING,
    VKI_CONSOLE_NEED_ENTER,
    VKI_HIDDEN_CHARACTERS_MODE,
    VKI_HIDDEN_CHARACTERS_ALPHA,
    VKI_HIDDEN_CHARACTERS_MODE_ONLY_FOR_SELF,
    VKI_TRANSPARENT_SPELL_ICONS,
    VKI_SPELL_ICONS_ALPHA,
    VKI_SKILLS_COUNT,
    VKI_SKILL_CAN_BE_USED,
    VKI_STATIC_ART_ADDRESS,
    VKI_USED_LAYER,
    VKI_SPELLBOOK_COUNT,
    VKI_BLOCK_MOVING,
    VKI_SET_PLAYER_GRAPHIC,
    VKI_FAST_ROTATION,
    VKI_IGNORE_STAMINA_CHECK,
    VKI_LAST_TARGET,
    VKI_LAST_ATTACK,
    VKI_NEW_TARGET_SYSTEM_SERIAL,
    VKI_GET_MAP_SIZE,
    VKI_GET_MAP_BLOCK_SIZE,
    VKI_MAP_MUL_ADDRESS,
    VKI_STATIC_IDX_ADDRESS,
    VKI_STATIC_MUL_ADDRESS,
    VKI_MAP_DIFL_ADDRESS,
    VKI_MAP_DIF_ADDRESS,
    VKI_STATIC_DIFL_ADDRESS,
    VKI_STATIC_DIFI_ADDRESS,
    VKI_STATIC_DIF_ADDRESS,
    VKI_VERDATA_ADDRESS,
    VKI_MAP_MUL_SIZE,
    VKI_STATIC_IDX_SIZE,
    VKI_STATIC_MUL_SIZE,
    VKI_MAP_DIFL_SIZE,
    VKI_MAP_DIF_SIZE,
    VKI_STATIC_DIFL_SIZE,
    VKI_STATIC_DIFI_SIZE,
    VKI_STATIC_DIF_SIZE,
    VKI_VERDATA_SIZE,
    VKI_MAP_UOP_ADDRESS,
    VKI_MAP_UOP_SIZE,
    VKI_MAP_X_UOP_ADDRESS,
    VKI_MAP_X_UOP_SIZE,
    VKI_CLILOC_ENU_ADDRESS,
    VKI_CLILOC_ENU_SIZE,
    VKI_GUMP_ART_ADDRESS,
    VKI_VIEW_RANGE,
    VKI_SET_PVPCALLER
};

enum VALUE_KEY_STRING
{
    VKS_SKILL_NAME = 0,
    VKS_SERVER_NAME,
    VKS_CHARACTER_NAME,
    VKS_SPELLBOOK_1_SPELL_NAME,
    VKS_SPELLBOOK_2_SPELL_NAME,
    VKS_SPELLBOOK_3_SPELL_NAME,
    VKS_SPELLBOOK_4_SPELL_NAME,
    VKS_SPELLBOOK_5_SPELL_NAME,
    VKS_SPELLBOOK_6_SPELL_NAME,
    VKS_SPELLBOOK_7_SPELL_NAME
};

enum XUO_FILE_INDEX
{
    OFI_MAP_0_MUL = 1,
    OFI_MAP_1_MUL,
    OFI_MAP_2_MUL,
    OFI_MAP_3_MUL,
    OFI_MAP_4_MUL,
    OFI_MAP_5_MUL,
    OFI_MAP_0_UOP,
    OFI_MAP_1_UOP,
    OFI_MAP_2_UOP,
    OFI_MAP_3_UOP,
    OFI_MAP_4_UOP,
    OFI_MAP_5_UOP,
    OFI_MAPX_0_UOP,
    OFI_MAPX_1_UOP,
    OFI_MAPX_2_UOP,
    OFI_MAPX_3_UOP,
    OFI_MAPX_4_UOP,
    OFI_MAPX_5_UOP,
    OFI_STAIDX_0_MUL,
    OFI_STAIDX_1_MUL,
    OFI_STAIDX_2_MUL,
    OFI_STAIDX_3_MUL,
    OFI_STAIDX_4_MUL,
    OFI_STAIDX_5_MUL,
    OFI_STATICS_0_MUL,
    OFI_STATICS_1_MUL,
    OFI_STATICS_2_MUL,
    OFI_STATICS_3_MUL,
    OFI_STATICS_4_MUL,
    OFI_STATICS_5_MUL,
    OFI_MAP_DIF_0_MUL,
    OFI_MAP_DIF_1_MUL,
    OFI_MAP_DIF_2_MUL,
    OFI_MAP_DIF_3_MUL,
    OFI_MAP_DIF_4_MUL,
    OFI_MAP_DIF_5_MUL,
    OFI_MAP_DIFL_0_MUL,
    OFI_MAP_DIFL_1_MUL,
    OFI_MAP_DIFL_2_MUL,
    OFI_MAP_DIFL_3_MUL,
    OFI_MAP_DIFL_4_MUL,
    OFI_MAP_DIFL_5_MUL,
    OFI_STA_DIF_0_MUL,
    OFI_STA_DIF_1_MUL,
    OFI_STA_DIF_2_MUL,
    OFI_STA_DIF_3_MUL,
    OFI_STA_DIF_4_MUL,
    OFI_STA_DIF_5_MUL,
    OFI_STA_DIFI_0_MUL,
    OFI_STA_DIFI_1_MUL,
    OFI_STA_DIFI_2_MUL,
    OFI_STA_DIFI_3_MUL,
    OFI_STA_DIFI_4_MUL,
    OFI_STA_DIFI_5_MUL,
    OFI_STA_DIFL_0_MUL,
    OFI_STA_DIFL_1_MUL,
    OFI_STA_DIFL_2_MUL,
    OFI_STA_DIFL_3_MUL,
    OFI_STA_DIFL_4_MUL,
    OFI_STA_DIFL_5_MUL,
    OFI_TILEDATA_MUL,
    OFI_MULTI_IDX,
    OFI_MULTI_MUL,
    OFI_MULTI_UOP,
    OFI_HUES_MUL,
    OFI_VERDATA_MUL,
    OFI_CLILOC_MUL,
    OFI_RADARCOL_MUL,
    OFI_FILES_COUNT
};

enum XUO_GRAPHIC_DATA_TYPE
{
    OGDT_STATIC_ART = 1,
    OGDT_GUMP_ART
};

enum XUO_FEATURE_FLAGS
{
    OFF_DRAW_CHARACTERS_STATUS_IN_WORLD = 0x00000001,
    OFF_CHANGE_TREES_TO_STUMPS = 0x00000002,
    OFF_MARKING_CAVES = 0x00000004,
    OFF_NO_VEGETATION = 0x00000008,
    OFF_NO_FIELDS_ANIMATION = 0x00000010,
    OFF_COLORED_CHARACTERS_STATE = 0x00000020,
    OFF_TILED_FIELDS = 0x00000040,
    OFF_DRAW_AURA = 0x00000080,
    OFF_NO_DRAW_ROOFS = 0x00000100,

    OFF_ALL_FLAGS = 0xFFFFFFFF
};

enum OBJECT_LAYERS
{
    OL_NONE = 0,    //0
    OL_1_HAND,      //1
    OL_2_HAND,      //2
    OL_SHOES,       //3
    OL_PANTS,       //4
    OL_SHIRT,       //5
    OL_HELMET,      //6
    OL_GLOVES,      //7
    OL_RING,        //8
    OL_TALISMAN,    //9
    OL_NECKLACE,    //10
    OL_HAIR,        //11
    OL_WAIST,       //12
    OL_TORSO,       //13
    OL_BRACELET,    //14
    OL_15,          //15
    OL_BEARD,       //16
    OL_TUNIC,       //17
    OL_EARRINGS,    //18
    OL_ARMS,        //19
    OL_CLOAK,       //20
    OL_BACKPACK,    //21
    OL_ROBE,        //22
    OL_SKIRT,       //23
    OL_LEGS,        //24
    OL_MOUNT,       //25
    OL_BUY_RESTOCK, //26
    OL_BUY,         //27
    OL_SELL,        //28
    OL_BANK         //29
};

enum SPEECH_TYPE
{
    ST_NORMAL = 0,
    ST_BROADCAST,                //System
    ST_EMOTE = 0x02,             //Emote
    ST_SYSTEM = 0x06,            //System / Lower Corner
    ST_SYSTEM_WHITE_NAME = 0x07, //Message / Corner With Name
    ST_WHISPER = 0x08,           //Whisper
    ST_YELL = 0x09,              //Yell
    ST_SPELL = 0x0A,             //Spell
    ST_GUILD_CHAT = 0x0D,        //Guild Chat
    ST_ALLIANCE_CHAT = 0x0E,     //Alliance Chat
    ST_COMMAND_PROMT = 0x0F,     //Command Prompts
    ST_ENCODED_COMMAND = 0xC0,   //Encoded Commands
};

enum TEXT_TYPE
{
    TT_CLIENT = 0,
    TT_SYSTEM,
    TT_OBJECT
};

enum TEXT_ALIGN_TYPE
{
    TS_LEFT = 0,
    TS_CENTER,
    TS_RIGHT
};

enum HTML_TAG_TYPE
{
    HTT_NONE = 0,
    HTT_B,
    HTT_I,
    HTT_A,
    HTT_U,
    HTT_P,
    HTT_BIG,
    HTT_SMALL,
    HTT_BODY,
    HTT_BASEFONT,
    HTT_H1,
    HTT_H2,
    HTT_H3,
    HTT_H4,
    HTT_H5,
    HTT_H6,
    HTT_BR,
    HTT_BQ,
    HTT_LEFT,
    HTT_CENTER,
    HTT_RIGHT,
    HTT_DIV
};

enum PROMPT_TYPE
{
    PT_NONE = 0,
    PT_ASCII,
    PT_UNICODE
};

enum MOUSE_WHEEL_STATE
{
    MWS_UP = 0,
    MWS_DOWN,
    MWS_DOUBLE_CLICK,
    MWS_SCROLL_UP,
    MWS_SCROLL_DOWN
};

enum DRAW_CHARACTERS_STATUS_STATE
{
    DCSS_NO_DRAW = 0,
    DCSS_ABOVE,
    DCSS_UNDER
};

enum DRAW_CHARACTERS_STATUS_CONDITION_STATE
{
    DCSCS_ALWAYS = 0,
    DCSCS_NOT_MAX,
    DCSCS_LOWER
};

enum HIDDEN_CHARACTERS_RENDER_MODE
{
    HCRM_ORIGINAL = 0,
    HCRM_ALPHA_BLENDING,
    HCRM_SPECTRAL_COLOR,
    HCRM_SPECIAL_SPECTRAL_COLOR
};

enum DRAW_AURA_STATE
{
    DAS_NEVER = 0,
    DAS_IN_WARMODE,
    DAS_ALWAYS
};

enum SCREENSHOT_FORMAT
{
    SF_BMP = 0,
    SF_PNG,
    SF_TGA,
    SF_JPG
};

enum OBJECT_PROPERTIES_MODE
{
    OPM_AT_ICON = 0,
    OPM_ALWAYS_UP,
    OPM_FOLLOW_MOUSE,
    OPM_SINGLE_CLICK
};

enum CHARACTER_BACKPACK_STYLE
{
    CBS_DEFAULT = 0,
    CBS_SUEDE,
    CBS_POLAR_BEAR,
    CBS_GHOUL_SKIN
};

enum SHADER_DRAW_MODE
{
    SDM_NO_COLOR = 0,
    SDM_COLORED = 1,
    SDM_PARTIAL_HUE = 2,
    SDM_TEXT_COLORED_NO_BLACK = 3,
    SDM_TEXT_COLORED = 4,
    SDM_LAND = 6,
    SDM_LAND_COLORED = 7,
    SDM_SPECTRAL = 10,
    SDM_SPECIAL_SPECTRAL = 11,
    SDM_SHADOW = 12
};

enum STATIC_TILES_FILTER_FLAGS
{
    STFF_CAVE = 0x01,
    STFF_STUMP = 0x02,
    STFF_STUMP_HATCHED = 0x04,
    STFF_VEGETATION = 0x08,
    STFF_WATER = 0x10
};

enum XUO_INTERNAL_PACKET_MESSAGE_TYPE
{
    OIPMT_FILES_TRANSFERED = 1,
    OIPMT_FILE_INFO = 2,
    OIPMT_FILE_INFO_LOCALIZED = 3,
    OIPMT_GRAPHIC_DATA_INFO = 4,
    OIPMT_SKILL_LIST = 100,
    OIPMT_SPELL_LIST = 101,
    OIPMT_MACRO_LIST = 102,
    OIPMT_OPEN_MAP = 103
};

enum DIRECTION_TYPE
{
    DT_N = 0,
    DT_NE,
    DT_E,
    DT_SE,
    DT_S,
    DT_SW,
    DT_W,
    DT_NW
};

enum ANIMATION_GROUPS_TYPE
{
    AGT_MONSTER = 0,
    AGT_SEA_MONSTER,
    AGT_ANIMAL,
    AGT_HUMAN,
    AGT_EQUIPMENT,
    AGT_UNKNOWN
};

enum ANIMATION_GROUPS
{
    AG_NONE = 0,
    AG_LOW,
    AG_HIGHT,
    AG_PEOPLE
};

enum LOW_ANIMATION_GROUP
{
    LAG_WALK = 0,
    LAG_RUN,
    LAG_STAND,
    LAG_EAT,
    LAG_UNKNOWN,
    LAG_ATTACK_1,
    LAG_ATTACK_2,
    LAG_ATTACK_3,
    LAG_DIE_1,
    LAG_FIDGET_1,
    LAG_FIDGET_2,
    LAG_LIE_DOWN,
    LAG_DIE_2,

    LAG_ANIMATION_COUNT
};

enum HIGHT_ANIMATION_GROUP
{
    HAG_WALK = 0,
    HAG_STAND,
    HAG_DIE_1,
    HAG_DIE_2,
    HAG_ATTACK_1,
    HAG_ATTACK_2,
    HAG_ATTACK_3,
    HAG_MISC_1,
    HAG_MISC_2,
    HAG_MISC_3,
    HAG_STUMBLE,
    HAG_SLAP_GROUND,
    HAG_CAST,
    HAG_GET_HIT_1,
    HAG_MISC_4,
    HAG_GET_HIT_2,
    HAG_GET_HIT_3,
    HAG_FIDGET_1,
    HAG_FIDGET_2,
    HAG_FLY,
    HAG_LAND,
    HAG_DIE_IN_FLIGHT,

    HAG_ANIMATION_COUNT
};

enum PEOPLE_ANIMATION_GROUP
{
    PAG_WALK_UNARMED = 0,
    PAG_WALK_ARMED,
    PAG_RUN_UNARMED,
    PAG_RUN_ARMED,
    PAG_STAND,
    PAG_FIDGET_1,
    PAG_FIDGET_2,
    PAG_STAND_ONEHANDED_ATTACK,
    PAG_STAND_TWOHANDED_ATTACK,
    PAG_ATTACK_ONEHANDED,
    PAG_ATTACK_UNARMED_1,
    PAG_ATTACK_UNARMED_2,
    PAG_ATTACK_TWOHANDED_DOWN,
    PAG_ATTACK_TWOHANDED_WIDE,
    PAG_ATTACK_TWOHANDED_JAB,
    PAG_WALK_WARMODE,
    PAG_CAST_DIRECTED,
    PAG_CAST_AREA,
    PAG_ATTACK_BOW,
    PAG_ATTACK_CROSSBOW,
    PAG_GET_HIT,
    PAG_DIE_1,
    PAG_DIE_2,
    PAG_ONMOUNT_RIDE_SLOW,
    PAG_ONMOUNT_RIDE_FAST,
    PAG_ONMOUNT_STAND,
    PAG_ONMOUNT_ATTACK,
    PAG_ONMOUNT_ATTACK_BOW,
    PAG_ONMOUNT_ATTACK_CROSSBOW,
    PAG_ONMOUNT_SLAP_HORSE,
    PAG_TURN,
    PAG_ATTACK_UNARMED_AND_WALK,
    PAG_EMOTE_BOW,
    PAG_EMOTE_SALUTE,
    PAG_FIDGET_3,

    PAG_ANIMATION_COUNT
};

enum NOTORIETY_TYPE
{
    NT_NONE = 0,
    NT_INNOCENT,
    NT_FRIENDLY,
    NT_SOMEONE_GRAY,
    NT_CRIMINAL,
    NT_ENEMY,
    NT_MURDERER,
    NT_INVULNERABLE
};

enum CLICK_OBJECT_TYPE
{
    COT_NONE = 0,
    COT_GAME_OBJECT,
    COT_STATIC,
    COT_GUMP
};

enum CUSTOM_HOUSE_UPDATE_TYPE
{
    CHUT_UPDATE = 1,
    CHUT_REMOVE,
    CHUT_UPDATE_MULTI_POS,
    CHUT_CONSTRUCT_BEGIN,
    CHUT_CONSTRUCT_END
};

enum MACRO_CODE
{
    MC_NONE = 0,
    MC_SAY,
    MC_EMOTE,
    MC_WHISPER,
    MC_YELL,
    MC_WALK,
    MC_WAR_PEACE,
    MC_PASTE,
    MC_OPEN,
    MC_CLOSE,
    MC_MINIMIZE,
    MC_MAXIMIZE,
    MC_OPEN_DOOR,
    MC_USE_SKILL,
    MC_LAST_SKILL,
    MC_CAST_SPELL,
    MC_LAST_SPELL,
    MC_LAST_OBJECT,
    MC_BOW,
    MC_SALUTE,
    MC_QUIT_GAME,
    MC_ALL_NAMES,
    MC_LAST_TARGET,
    MC_TARGET_SELF,
    MC_ARM_DISARM,
    MC_WAIT_FOR_TARGET,
    MC_TARGET_NEXT,
    MC_ATTACK_LAST,
    MC_DELAY,
    MC_CIRCLE_TRANS,
    MC_CLOSE_GUMPS,
    MC_ALWAYS_RUN,
    MC_SAVE_DESKTOP,
    MC_KILL_GUMP_OPEN,
    MC_PRIMARY_ABILITY,
    MC_SECONDARY_ABILITY,
    MC_EQUIP_LAST_WEAPON,
    MC_SET_UPDATE_RANGE,
    MC_MODIFY_UPDATE_RANGE,
    MC_INCREASE_UPDATE_RANGE,
    MC_DECREASE_UPDATE_RANGE,
    MC_MAX_UPDATE_RANGE,
    MC_MIN_UPDATE_RANGE,
    MC_DEFAULT_UPDATE_RANGE,
    MC_UPDATE_RANGE_INFO,
    MC_ENABLE_RANGE_COLOR,
    MC_DISABLE_RANGE_COLOR,
    MC_TOGGLE_RANGE_COLOR,
    MC_INVOKE_VIRTUE,
    MC_SELECT_NEXT,
    MC_SELECT_PREVIOUS,
    MC_SELECT_NEAREST,
    MC_ATTACK_SELECTED_TARGET,
    MC_USE_SELECTED_TARGET,
    MC_CURRENT_TARGET,
    MC_TARGET_SYSTEM_ON_OFF,
    MC_TOGGLE_BUICON_WINDOW,
    MC_BANDAGE_SELF,
    MC_BANDAGE_TARGET,
    MC_TOGGLE_GARGOYLE_FLYING
};

enum MACRO_SUB_CODE
{
    MSC_NONE = 0,
    MSC_G1_NW, //Walk group
    MSC_G1_N,
    MSC_G1_NE,
    MSC_G1_E,
    MSC_G1_SE,
    MSC_G1_S,
    MSC_G1_SW,
    MSC_G1_W,
    MSC_G2_CONFIGURATION, //Open/Close/Minimize/Maximize group
    MSC_G2_PAPERDOLL,
    MSC_G2_STATUS,
    MSC_G2_JOURNAL,
    MSC_G2_SKILLS,
    MSC_G2_MAGE_SPELLBOOK,
    MSC_G2_CHAT,
    MSC_G2_BACKPACK,
    MSC_G2_OWERVIEW,
    MSC_G2_WORLD_MAP,
    MSC_G2_MAIL,
    MSC_G2_PARTY_MANIFEST,
    MSC_G2_PARTY_CHAT,
    MSC_G2_NECRO_SPELLBOOK,
    MSC_G2_PALADIN_SPELLBOOK,
    MSC_G2_COMBAT_BOOK,
    MSC_G2_BUSHIDO_SPELLBOOK,
    MSC_G2_NINJITSU_SPELLBOOK,
    MSC_G2_GUILD,
    MSC_G2_SPELL_WEAVING_SPELLBOOK,
    MSC_G2_QUEST_LOG,
    MSC_G2_MYSTICISM_SPELLBOOK,
    MSC_G2_RACIAL_ABILITIES_BOOK,
    MSC_G2_BARD_SPELLBOOK,
    MSC_G3_ANATOMY, //Skills group
    MSC_G3_ANIMAL_LORE,
    MSC_G3_ANIMAL_TAMING,
    MSC_G3_ARMS_LORE,
    MSC_G3_BEGGING,
    MSC_G3_CARTOGRAPHY,
    MSC_G3_DETECTING_HIDDEN,
    MSC_G3_ENTICEMENT,
    MSC_G3_EVALUATING_INTELLIGENCE,
    MSC_G3_FORENSIC_EVALUATION,
    MSC_G3_HIDING,
    MSC_G3_IMBUING,
    MSC_G3_INSCRIPTION,
    MSC_G3_ITEM_IDENTIFICATION,
    MSC_G3_MEDITATION,
    MSC_G3_PEACEMAKING,
    MSC_G3_POISONING,
    MSC_G3_PROVOCATION,
    MSC_G3_REMOVE_TRAP,
    MSC_G3_SPIRIT_SPEAK,
    MSC_G3_STEALING,
    MSC_G3_STEALTH,
    MSC_G3_TASTE_IDENTIFICATION,
    MSC_G3_TRACKING,
    MSC_G4_LEFT_HAND, ///Arm/Disarm group
    MSC_G4_RIGHT_HAND,
    MSC_G5_HONOR, //Invoke Virtue group
    MSC_G5_SACRIFICE,
    MSC_G5_VALOR,
    MSC_G6_CLUMSY, //Cast Spell group
    MSC_G6_CREATE_FOOD,
    MSC_G6_FEEBLEMIND,
    MSC_G6_HEAL,
    MSC_G6_MAGIC_ARROW,
    MSC_G6_NIGHT_SIGHT,
    MSC_G6_REACTIVE_ARMOR,
    MSC_G6_WEAKEN,
    MSC_G6_AGILITY,
    MSC_G6_CUNNING,
    MSC_G6_CURE,
    MSC_G6_HARM,
    MSC_G6_MAGIC_TRAP,
    MSC_G6_MAGIC_UNTRAP,
    MSC_G6_PROTECTION,
    MSC_G6_STRENGTH,
    MSC_G6_BLESS,
    MSC_G6_FIREBALL,
    MSC_G6_MAGIC_LOCK,
    MSC_G6_POISON,
    MSC_G6_TELEKINESIS,
    MSC_G6_TELEPORT,
    MSC_G6_UNLOCK,
    MSC_G6_WALL_OF_STONE,
    MSC_G6_ARCH_CURE,
    MSC_G6_ARCH_PROTECTION,
    MSC_G6_CURSE,
    MSC_G6_FIRE_FIELD,
    MSC_G6_GREATER_HEAL,
    MSC_G6_LIGHTNING,
    MSC_G6_MANA_DRAIN,
    MSC_G6_RECALL,
    MSC_G6_BLADE_SPIRITS,
    MSC_G6_DISPELL_FIELD,
    MSC_G6_INCOGNITO,
    MSC_G6_MAGIC_REFLECTION,
    MSC_G6_MIND_BLAST,
    MSC_G6_PARALYZE,
    MSC_G6_POISON_FIELD,
    MSC_G6_SUMMON_CREATURE,
    MSC_G6_DISPEL,
    MSC_G6_ENERGY_BOLT,
    MSC_G6_EXPLOSION,
    MSC_G6_INVISIBILITY,
    MSC_G6_MARK,
    MSC_G6_MASS_CURSE,
    MSC_G6_PARALYZE_FIELD,
    MSC_G6_REVEAL,
    MSC_G6_CHAIN_LIGHTNING,
    MSC_G6_ENERGY_FIELD,
    MSC_G6_FLAME_STRIKE,
    MSC_G6_GATE_TRAVEL,
    MSC_G6_MANA_VAMPIRE,
    MSC_G6_MASS_DISPEL,
    MSC_G6_METEOR_SWARM,
    MSC_G6_POLYMORPH,
    MSC_G6_EARTHQUAKE,
    MSC_G6_ENERGY_VORTEX,
    MSC_G6_RESURRECTION,
    MSC_G6_AIR_ELEMENTAL,
    MSC_G6_SUMMON_DAEMON,
    MSC_G6_EARTH_ELEMENTAL,
    MSC_G6_FIRE_ELEMENTAL,
    MSC_G6_WATER_ELEMENTAL,
    MSC_G6_ANIMATE_DEAD,
    MSC_G6_BLOOD_OATH,
    MSC_G6_CORPSE_SKIN,
    MSC_G6_CURSE_WEAPON,
    MSC_G6_EVIL_OMEN,
    MSC_G6_HORRIFIC_BEAST,
    MSC_G6_LICH_FORM,
    MSC_G6_MIND_ROT,
    MSC_G6_PAIN_SPIKE,
    MSC_G6_POISON_STRIKE,
    MSC_G6_STRANGLE,
    MSC_G6_SUMMON_FAMILAR,
    MSC_G6_VAMPIRIC_EMBRACE,
    MSC_G6_VENGEFUL_SPIRIT,
    MSC_G6_WITHER,
    MSC_G6_WRAITH_FORM,
    MSC_G6_EXORCISM,
    MSC_G6_CLEANCE_BY_FIRE,
    MSC_G6_CLOSE_WOUNDS,
    MSC_G6_CONSECRATE_WEAPON,
    MSC_G6_DISPEL_EVIL,
    MSC_G6_DIVINE_FURY,
    MSC_G6_ENEMY_OF_ONE,
    MSC_G6_HOLY_LIGHT,
    MSC_G6_NOBLE_SACRIFICE,
    MSC_G6_REMOVE_CURSE,
    MSC_G6_SACRED_JOURNEY,
    MSC_G6_HONORABLE_EXECUTION,
    MSC_G6_CONFIDENCE,
    MSC_G6_EVASION,
    MSC_G6_COUNTER_ATTACK,
    MSC_G6_LIGHTING_STRIKE,
    MSC_G6_MOMENTUM_STRIKE,
    MSC_G6_FOCUS_ATTACK,
    MSC_G6_DEATH_STRIKE,
    MSC_G6_ANIMAL_FORM,
    MSC_G6_KI_ATTACK,
    MSC_G6_SURPRICE_ATTACK,
    MSC_G6_BACKSTAB,
    MSC_G6_SHADOWJUMP,
    MSC_G6_MIRROR_IMAGE,
    MSC_G6_ARCANE_CIRCLE,
    MSC_G6_GIFT_OF_RENEWAL,
    MSC_G6_IMMOLATING_WEAPON,
    MSC_G6_ATTUNEMENT,
    MSC_G6_THUNDERSTORM,
    MSC_G6_NATURES_FURY,
    MSC_G6_SUMMON_FEY,
    MSC_G6_SUMMON_FIEND,
    MSC_G6_REAPER_FORM,
    MSC_G6_WILDFIRE,
    MSC_G6_ESSENCE_OF_WIND,
    MSC_G6_DRYAD_ALLURE,
    MSC_G6_ETHEREAL_VOYAGE,
    MSC_G6_WORD_OF_DEATH,
    MSC_G6_GIFT_OF_LIFE,
    MSC_G6_ARCANE_EMPOWERMEN,
    MSC_G6_NETHER_BOLT,
    MSC_G6_HEALING_STONE,
    MSC_G6_PURGE_MAGIC,
    MSC_G6_ENCHANT,
    MSC_G6_SLEEP,
    MSC_G6_EAGLE_STRIKE,
    MSC_G6_ANIMATED_WEAPON,
    MSC_G6_STONE_FORM,
    MSC_G6_SPELL_TRIGGER,
    MSC_G6_MASS_SLEEP,
    MSC_G6_CLEANSING_WINDS,
    MSC_G6_BOMBARD,
    MSC_G6_SPELL_PLAGUE,
    MSC_G6_HAIL_STORM,
    MSC_G6_NETHER_CYCLONE,
    MSC_G6_RISING_COLOSSUS,
    MSC_G6_INSPIRE,
    MSC_G6_INVIGORATE,
    MSC_G6_RESILIENCE,
    MSC_G6_PERSEVERANCE,
    MSC_G6_TRIBULATION,
    MSC_G6_DESPAIR,
    MSC_G7_HOSTILE, //Select Next/Previous/Nearest group
    MSC_G7_PARTY,
    MSC_G7_FOLLOWER,
    MSC_G7_OBJECT,
    MSC_G7_MOBILE,
    MSC_TOTAL_COUNT
};

enum MACRO_RETURN_CODE
{
    MRC_PARSE_NEXT = 0,
    MRC_BREAK_PARSER,
    MRC_STOP
};

enum GAME_CONSOLE_TEXT_TYPE
{
    GCTT_NORMAL = 0,
    GCTT_YELL,
    GCTT_WHISPER,
    GCTT_EMOTE,
    GCTT_C,
    GCTT_BROADCAST,
    GCTT_PARTY,
    GCTT_GUILD,
    GCTT_ALLIANCE,
    GCTT_PARTY_ACCEPT,
    GCTT_PARTY_DECLINE,
    GCTT_PARTY_ADD,
    GCTT_PARTY_LEAVE
};

enum SEASON_TYPE
{
    ST_SPRING = 0,
    ST_SUMMER,
    ST_FALL,
    ST_WINTER,
    ST_DESOLATION
};

enum ABILITY_TYPE
{
    AT_NONE = 0,
    AT_ARMOR_IGNORE,
    AT_BLEED_ATTACK,
    AT_CONCUSSION_BLOW,
    AT_CRUSHING_BLOW,
    AT_DISARM,
    AT_DISMOUNT,
    AT_DOUBLE_STRIKE,
    AT_INFECTING,
    AT_MORTAL_STRIKE,
    AT_MOVING_SHOT,
    AT_PARALYZING_BLOW,
    AT_SHADOW_STRIKE,
    AT_WHIRLWIND_ATTACK,
    AT_RIDING_SWIPE,
    AT_FRENZIED_WHIRLWIND,
    AT_BLOCK,
    AT_DEFENSE_MASTERY,
    AT_NERVE_STRIKE,
    AT_TALON_STRIKE,
    AT_FEINT,
    AT_DUAL_WIELD,
    AT_DOUBLE_SHOT,
    AT_ARMOR_PIERCE,
    AT_BLADEWEAVE,
    AT_FORCE_ARROW,
    AT_LIGHTNING_ARROW,
    AT_PSYCHIC_ATTACK,
    AT_SERPENT_ARROW,
    AT_FORCE_OF_NATURE,
    AT_INFUSED_THROW,
    AT_MYSTIC_ARC,
    AT_DISROBE
};

enum SECURE_TRADE_TYPE
{
    // SecureTrade Action types.
    SECURE_TRADE_OPEN = 0,
    SECURE_TRADE_CLOSE = 1,
    SECURE_TRADE_CHANGE = 2,
    SECURE_TRADE_UPDATEGOLD = 3,
    SECURE_TRADE_UPDATELEDGER = 4
};
