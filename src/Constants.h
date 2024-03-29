// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>

const int MIN_FPS_LIMIT = 12;

const int MAX_FPS_LIMIT = 300;

const int DEFAULT_FPS = MAX_FPS_LIMIT;

const int CPU_USAGE_DELAY = 2;

const int DRAG_ITEMS_DISTANCE = 3;

const int MAX_ART_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR = 20;

const int MAX_GUMP_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR = 20;

const int MAX_SOUND_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR = 20;

const int MAX_MAP_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR = 50;

const int TRANSLUCENT_ALPHA = 0xB2; //0.7

const int FOLIAGE_ALPHA = 0x4C; //0.3

const int ALPHA_STEP = 25;

const int CONTEXT_MENU_FONT = 1;

const int KEEP_CHARACTERS_IN_REMOVE_LIST_DELAY = 100;

const int MAX_STEPS_COUNT = 5;

const int FRAME_DELAY_ACTIVE_WINDOW = 30;

const int FRAME_DELAY_INACTIVE_WINDOW = (FRAME_DELAY_ACTIVE_WINDOW + 1) * 7;

const int MIN_VIEW_RANGE = 5;

const int MAX_VIEW_RANGE_OLD = 18;

const int MAX_VIEW_RANGE_NEW = 24;

const int DAMAGE_TEXT_NORMAL_DELAY = 1500;

const int DAMAGE_TEXT_TRANSPARENT_DELAY = 400;

const int DAMAGE_TEXT_MOVE_DELAY = 50;

const int DAMAGE_TEXT_STEP = 2;

const int DAMAGE_TEXT_ALPHA_STEP = (DAMAGE_TEXT_TRANSPARENT_DELAY / FRAME_DELAY_ACTIVE_WINDOW);

const int DRAG_PIXEL_RANGE = 1;

const int DRAG_ITEMS_PIXEL_RANGE = 7;

const int DRAG_PIXEL_RANGE_WITH_TARGET = 3;

const int DEATH_MUSIC_INDEX = 42;

const int ORIGINAL_ITEMS_ANIMATION_DELAY = 80;
const int XUO_ITEMS_ANIMATION_DELAY = 50;

const int ORIGINAL_CHARACTERS_ANIMATION_DELAY = 100;
const int XUO_CHARACTERS_ANIMATION_DELAY = 70;

const uint16_t FIELD_REPLACE_GRAPHIC = 0x1826;

const int MAX_LIGHT_SOURCES = 100;

const int MAX_OBJECT_HANDLES = 200;

const int MAX_FAST_WALK_STACK_SIZE = 5;

const int MAX_FILE_BUFFER_SIZE = 0x1000;

const int SIZE_VARIABLE = 0;

const uint16_t SPECTRAL_COLOR_FLAG = 0x4000;

const uint16_t SPECTRAL_COLOR_SPECIAL = 0x4666;

const int UNICODE_SPACE_WIDTH = 8;

const float ITALIC_FONT_KOEFFICIENT = 3.3f;

const int MAX_HTML_TEXT_HEIGHT = 18;

// maximum number of gump textentry characters we are allowed to send to the server,
// on unmodified derivative servers, sending more than this will cause a forced disconnection
const int MAX_TEXTENTRY_LENGTH = 239;

const int GAME_FIGURE_GUMP_OFFSET = 11369;

const int TEXT_MESSAGE_MAX_WIDTH = 200;

const int TEXT_SYSTEM_MESSAGE_MAX_WIDTH = 320;

const uint32_t DEATH_SCREEN_DELAY = 3000;

const uint16_t SELECT_LAND_COLOR = 0x0044;

const uint16_t SELECT_STATIC_COLOR = 0x0077;

const uint16_t SELECT_MULTI_COLOR = 0x0066;

const int MALE_GUMP_OFFSET = 50000;

const int FEMALE_GUMP_OFFSET = 60000;

const int REMOVE_CONTAINER_GUMP_RANGE = 3;

const int DEFAULT_CHARACTER_HEIGHT = 16;

const int DEFAULT_BLOCK_HEIGHT = 16;

const int STEP_DELAY_MOUNT_RUN = 100;

const int STEP_DELAY_MOUNT_WALK = 200;

const int STEP_DELAY_RUN = 200;

const int STEP_DELAY_WALK = 400;

const int TURN_DELAY = 100;

const int TURN_DELAY_FAST = 45;

const int WALKING_DELAY = 750;

const int PLAYER_WALKING_DELAY = 150;

const int CHARACTER_ANIMATION_DELAY_TABLE[2][2] = {
    { STEP_DELAY_WALK, STEP_DELAY_RUN }, { STEP_DELAY_MOUNT_WALK, STEP_DELAY_MOUNT_RUN }
};

const int DCLICK_DELAY = 350;

const int SCROLL_LISTING_DELAY = 150;

const int CHANGE_SHOP_COUNT_DELAY = 50;

const int CHANGE_MACRO_DELAY = 100;

const int CHANGE_MACRO_LIST_DELAY = 500;

const int SEND_TIMEOUT_DELAY = 55000;

const int GUMP_MENU_PIXEL_STEP = 5;

const int GUMP_SCROLLING_PIXEL_STEP = 10;

const int CLEAR_TEXTURES_DELAY = 3000;

const int WEATHER_TIMER = (6 * 60 * 1000);

const int WAIT_FOR_TARGET_DELAY = 5000;

static const bool LAYER_UNSAFE[30] = { false, //0
                                       true,  true, true,  true,  true,  true,  true,  true,
                                       true,  true, false, false, true,  true,  false, false,
                                       true,  true, true,  true,
                                       false, //0x15
                                       true,  true, true,  false, false, false, false, false };

struct TREE_UNIONS
{
    uint16_t GraphicStart;
    uint16_t GraphicEnd;
};

const int TREE_COUNT = 9;

// from client 7.0.45.0
static const TREE_UNIONS TREE_INFO[TREE_COUNT] = { { 0x0D45, 0x0D4C }, { 0x0D4D, 0x0D53 },
                                                   { 0x0D5C, 0x0D62 }, { 0x0D63, 0x0D69 },
                                                   { 0x0D73, 0x0D79 }, { 0x0D7A, 0x0D7F },
                                                   { 0x0D87, 0x0D8B }, { 0x0D8C, 0x0D90 },
                                                   { 0x12BE, 0x12C7 } };

const int SITTING_OFFSET_X = 8;

struct SITTING_INFO_DATA
{
    uint16_t Graphic;

    // Anim/Character direction
    // 7, 0
    uint8_t Direction1; //0
    // 1, 2
    uint8_t Direction2; //2
    // 3, 4
    uint8_t Direction3; //4
    // 5, 6
    uint8_t Direction4; //6

    int8_t OffsetY;
    int8_t MirrorOffsetY;
    bool DrawBack;
};

const uint8_t INVALID_DIRECTION = 0xff;

#define SITTING_N 0, 0, 0, 0
#define SITTING_E 2, 2, 2, 2
#define SITTING_S 4, 4, 4, 4
#define SITTING_W 6, 6, 6, 6
#define SITTING_N_S 0, INVALID_DIRECTION, 4, INVALID_DIRECTION
#define SITTING_E_W INVALID_DIRECTION, 2, INVALID_DIRECTION, 6
#define SITTING_ALL 0, 2, 4, 6
#define SITTING_UNKNOWN 0, 2, 4, 6

// from client 7.0.45.0
static const SITTING_INFO_DATA SITTING_INFO[] = {
    { 0x0459, SITTING_N_S, 2, 2, false },
    { 0x045A, SITTING_E_W, 2, 2, false },
    { 0x045B, SITTING_N_S, 2, 2, false },
    { 0x045C, SITTING_E_W, 2, 2, false },
    { 0x0A2A, SITTING_ALL, -4, -4, false },
    { 0x0A2B, SITTING_ALL, -8, -8, false },
    { 0x0B2C, SITTING_E_W, 2, 2, false },
    { 0x0B2D, SITTING_N_S, 2, 2, false },
    { 0x0B2E, SITTING_S, 0, 0, false },
    { 0x0B2F, SITTING_E, 6, 6, false },
    { 0x0B30, SITTING_W, -8, 8, true },
    { 0x0B31, SITTING_N, 0, 4, true },
    { 0x0B32, SITTING_S, 0, 0, false },
    { 0x0B33, SITTING_E, 0, 0, false },
    { 0x0B4E, SITTING_E, 0, 0, false },
    { 0x0B4F, SITTING_S, 0, 0, false },
    { 0x0B50, SITTING_N, 0, 0, true },
    { 0x0B51, SITTING_W, 0, 0, true },
    { 0x0B52, SITTING_E, 0, 0, false },
    { 0x0B53, SITTING_S, 0, 0, false },
    { 0x0B54, SITTING_N, 0, 0, true },
    { 0x0B55, SITTING_W, 0, 0, true },
    { 0x0B56, SITTING_E, 4, 4, false },
    { 0x0B57, SITTING_S, 4, 4, false },
    { 0x0B58, SITTING_W, 0, 8, true },
    { 0x0B59, SITTING_N, 0, 8, true },
    { 0x0B5A, SITTING_E, 8, 8, false },
    { 0x0B5B, SITTING_S, 8, 8, false },
    { 0x0B5C, SITTING_N, 0, 8, true },
    { 0x0B5D, SITTING_W, 0, 8, true },
    { 0x0B5E, SITTING_ALL, -8, -8, false },
    { 0x0B5F, SITTING_E_W, 3, 14, false },
    { 0x0B60, SITTING_E_W, 3, 14, false },
    { 0x0B61, SITTING_E_W, 3, 14, false },
    { 0x0B62, SITTING_E_W, 3, 10, false },
    { 0x0B63, SITTING_E_W, 3, 10, false },
    { 0x0B64, SITTING_E_W, 3, 10, false },
    { 0x0B65, SITTING_N_S, 3, 10, false },
    { 0x0B66, SITTING_N_S, 3, 10, false },
    { 0x0B67, SITTING_N_S, 3, 10, false },
    { 0x0B68, SITTING_N_S, 3, 10, false },
    { 0x0B69, SITTING_N_S, 3, 10, false },
    { 0x0B6A, SITTING_N_S, 3, 10, false },
    { 0x0B91, SITTING_S, 6, 6, false },
    { 0x0B92, SITTING_S, 6, 6, false },
    { 0x0B93, SITTING_E, 6, 6, false },
    { 0x0B94, SITTING_E, 6, 6, false },
    { 0x0CF3, SITTING_E_W, 2, 8, false },
    { 0x0CF4, SITTING_E_W, 2, 8, false },
    { 0x0CF6, SITTING_N_S, 2, 8, false },
    { 0x0CF7, SITTING_N_S, 2, 8, false },
    { 0x11FC, SITTING_ALL, 2, 7, false },
    { 0x1218, SITTING_S, 4, 4, false },
    { 0x1219, SITTING_E, 4, 4, false },
    { 0x121A, SITTING_N, 0, 8, true },
    { 0x121B, SITTING_W, 0, 8, true },
    { 0x1527, SITTING_E, 0, 0, false },
    { 0x1771, SITTING_ALL, 0, 0, false },
    { 0x1776, SITTING_ALL, 0, 0, false },
    { 0x1779, SITTING_ALL, 0, 0, false },
    { 0x1DC7, SITTING_E_W, 3, 10, false },
    { 0x1DC8, SITTING_E_W, 3, 10, false },
    { 0x1DC9, SITTING_E_W, 3, 10, false },
    { 0x1DCA, SITTING_N_S, 3, 10, false },
    { 0x1DCB, SITTING_N_S, 3, 10, false },
    { 0x1DCC, SITTING_N_S, 3, 10, false },
    { 0x1DCD, SITTING_E_W, 3, 10, false },
    { 0x1DCE, SITTING_E_W, 3, 10, false },
    { 0x1DCF, SITTING_E_W, 3, 10, false },
    { 0x1DD0, SITTING_N_S, 3, 10, false },
    { 0x1DD1, SITTING_N_S, 3, 10, false },
    { 0x1DD2, SITTING_E_W, 3, 10, false },

    { 0x2A58, SITTING_S, 0, 0, false },
    { 0x2A59, SITTING_E, 0, 0, false },
    { 0x2A5A, SITTING_ALL, 0, 0, false },
    { 0x2A5B, SITTING_ALL, 10, 10, false },
    { 0x2A7F, SITTING_ALL, 0, 0, false },
    { 0x2A80, SITTING_ALL, 0, 0, false },
    { 0x2DDF, SITTING_ALL, 2, 2, false },
    { 0x2DE0, SITTING_ALL, 2, 2, false },
    { 0x2DE3, SITTING_E, 4, 4, false },
    { 0x2DE4, SITTING_S, 4, 4, false },
    { 0x2DE5, SITTING_W, 4, 4, false },
    { 0x2DE6, SITTING_N, 4, 4, false },
    { 0x2DEB, SITTING_N, 4, 4, false },
    { 0x2DEC, SITTING_S, 4, 4, false },
    { 0x2DED, SITTING_E, 4, 4, false },
    { 0x2DEE, SITTING_W, 4, 4, false },
    { 0x2DF5, SITTING_ALL, 4, 4, false },
    { 0x2DF6, SITTING_ALL, 4, 4, false },
    { 0x3088, SITTING_ALL, 4, 4, false },
    { 0x3089, SITTING_ALL, 4, 4, false },
    { 0x308A, SITTING_ALL, 4, 4, false },
    { 0x308B, SITTING_ALL, 4, 4, false },
    { 0x35ED, SITTING_ALL, 0, 0, false },
    { 0x35EE, SITTING_ALL, 0, 0, false },

    { 0x3DFF, SITTING_N_S, 2, 2, false },
    { 0x3E00, SITTING_E_W, 2, 2, false },

    // TODO: new expansion
    { 0x4C8D, SITTING_S, 4, 4, false },
    { 0x4C8E, SITTING_S, 4, 4, false },
    { 0x4C8F, SITTING_S, 4, 4, false },

    { 0x4C1E, SITTING_E, 6, 6, false },
    //{ 0x4C1F, SITTING_E, 6, 6, false },

    { 0x4C8B, SITTING_E, 0, 0, false },
    { 0x4C8C, SITTING_E, 0, 0, false },
    { 0x4C8D, SITTING_S, 4, 4, false },
    { 0x4C8E, SITTING_S, 4, 4, false },
    { 0x4C8F, SITTING_S, 4, 4, false },

    { 0x4C1E, SITTING_E, 6, 6, false },
    { 0x4C80, SITTING_S, 4, 4, false },
    { 0x4C81, SITTING_E, 0, 0, false },
    { 0x4C82, SITTING_S, 4, 4, false },
    { 0x4C83, SITTING_S, 4, 4, false },
    { 0x4C84, SITTING_E, 0, 0, false },
    { 0x4C85, SITTING_E, 0, 0, false },
    { 0x4C86, SITTING_S, 4, 4, false },
    { 0x4C87, SITTING_S, 4, 4, false },
    { 0x4C88, SITTING_E, 0, 0, false },
    { 0x4C89, SITTING_E, 0, 0, false },
    { 0x4C8A, SITTING_E, 0, 0, false },
    { 0x4C8B, SITTING_E, 0, 0, false },
    { 0x4C8C, SITTING_E, 0, 0, false },
    { 0x4C8D, SITTING_S, 4, 4, false },
    { 0x4C8E, SITTING_S, 4, 4, false },
    { 0x4C8F, SITTING_S, 4, 4, false },
    //{ 0x4C1F, SITTING_E, 6, 6, false },

    // thanks Tony!
    { 0xA05F, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0xA05E, SITTING_N, 4, 4, false }, // NORTH ONLY
    { 0xA05D, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0xA05C, SITTING_W, 6, 4, false }, // WEST ONLY

    { 0x9EA2, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x9EA1, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0x9E9F, SITTING_N, 4, 4, false }, // NORTH ONLY
    { 0x9EA0, SITTING_W, 6, 4, false }, // WEST ONLY

    { 0x9E91, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0x9E90, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x9E8F, SITTING_W, 6, 4, false }, // WEST ONLY
    { 0x9E8E, SITTING_N, 4, 4, false }, // NORTH ONLY

    { 0x9C62, SITTING_N, 4, 4, false }, // NORTH ONLY
    { 0x9C61, SITTING_N, 4, 4, false }, // NORTH ONLY
    { 0x9C60, SITTING_N, 4, 4, false }, // NORTH ONLY

    { 0x9C5F, SITTING_W, 6, 4, false }, // WEST ONLY
    { 0x9C5E, SITTING_W, 6, 4, false }, // WEST ONLY
    { 0x9C5D, SITTING_W, 6, 4, false }, // WEST ONLY

    { 0x9C5A, SITTING_N, 4, 4, false }, // NORTH ONLY
    { 0x9C59, SITTING_N, 4, 4, false }, // NORTH ONLY

    { 0x9C58, SITTING_W, 6, 4, false }, // WEST ONLY
    { 0x9C57, SITTING_W, 6, 4, false }, // WEST ONLY

    { 0x402A, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x4029, SITTING_E, 0, 0, false }, // EAST ONLY

    { 0x4028, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0x4027, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0x4023, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0x4024, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x4C1E, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x4C1B, SITTING_S, 4, 4, false }, // SOUTH ONLY
    { 0x7132, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x71C2, SITTING_S, 4, 4, false }, // SOUTH ONLY

    { 0x9977, SITTING_E, 0, 0, false }, // EAST ONLY
    { 0x996C, SITTING_S, 4, 4, false }, // SOUTH ONLY
};
