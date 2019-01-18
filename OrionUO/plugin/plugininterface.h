// MIT License
// Copyright (C) September 2016 Hotride

#pragma once

#include <stdlib.h>
#include <stdint.h>

#if _WIN32
#include <Windows.h>
#define USER_MESSAGE_ID WM_USER
#define WindowHandle HWND
#else
#define USER_MESSAGE_ID 0x0400
#define WindowHandle void *
#endif

#ifndef MAX_PATH
#define MAX_PATH 256
#endif
#define UOMSG_SET_SERVER_NAME USER_MESSAGE_ID + 660
#define UOMSG_SET_PLAYER_NAME USER_MESSAGE_ID + 661
#define UOMSG_UPDATE_PLAYER_XYZ USER_MESSAGE_ID + 662
#define UOMSG_UPDATE_PLAYER_DIR USER_MESSAGE_ID + 663
#define UOMSG_RECV USER_MESSAGE_ID + 664
#define UOMSG_SEND USER_MESSAGE_ID + 665
#define UOMSG_IP_SEND USER_MESSAGE_ID + 666
#define UOMSG_PATHFINDING USER_MESSAGE_ID + 667
#define UOMSG_WALK USER_MESSAGE_ID + 668
#define UOMSG_MENU_RESPONSE USER_MESSAGE_ID + 669
#define UOMSG_STATUS_REQUEST USER_MESSAGE_ID + 670
#define UOMSG_SELECTED_TILE USER_MESSAGE_ID + 671
#define UOMSG_END_MACRO_PLAYING USER_MESSAGE_ID + 672
#define UOMSG_UPDATE_REMOVE_POS USER_MESSAGE_ID + 673

#define UOMSG_WIN_CLOSE USER_MESSAGE_ID + 1454
#define UOMSG_WIN_SHOW USER_MESSAGE_ID + 1455
#define UOMSG_WIN_PAINT USER_MESSAGE_ID + 1456
#define UOMSG_WIN_ACTIVATE USER_MESSAGE_ID + 1457
#define UOMSG_WIN_SETTEXT USER_MESSAGE_ID + 1458

#define UOMSG_INPUT_KEYDOWN USER_MESSAGE_ID + 1754
#define UOMSG_INPUT_KEYUP USER_MESSAGE_ID + 1755
#define UOMSG_INPUT_MOUSEWHEEL USER_MESSAGE_ID + 1756
#define UOMSG_INPUT_MBUTTONDOWN USER_MESSAGE_ID + 1757
#define UOMSG_INPUT_XBUTTONDOWN USER_MESSAGE_ID + 1758
#define UOMSG_INPUT_CHAR USER_MESSAGE_ID + 1759

typedef uint32_t EVENT_PROC(uint32_t, const void *);
typedef bool PACKET_PROC(uint8_t *, size_t);
typedef void VOID_PROC();
typedef bool WORLD_MAP_DRAW_PROC();

#pragma pack(push, 1)
struct PluginEvent
{
    void *data1 = nullptr;
    void *data2 = nullptr;
};

#if defined(_SDL_H) || defined(SDL_h_)
#define USING_SDL_STRUCTS
#endif

#ifndef USING_SDL_STRUCTS
// Because we share events with Orion Assistant

typedef uint32_t Keycode;
typedef uint16_t Keymod;

struct TextEvent
{
    // FIXME: this is not utf8 compatible, also SDL gives 32 bytes
    // this is a hack to keep compatibility with previous implementation
    char ch = 0;
};

struct KeyEvent
{
    // Subset from SDL_KeyboardEvent
    int8_t repeat = 0; // non-zero if this is a key repeat
    Keymod mod = 0;    // key modifiers
    // SDL virtual key representation (https://wiki.libsdl.org/SDL_Keycode)
    Keycode keycode = 0;
    // SDL physical key code (https://wiki.libsdl.org/SDL_Scancode)
    // values from: http://www.usb.org/developers/hidpage/Hut1_12v2.pdf
    // should be avoided
    int32_t scancode = 0;
    // For compatibility purposes with old win32 api:
    // keycode = wparam
    // scancode = lparam
};
#endif // USING_SDL_STRUCTS

struct PLUGIN_INFO
{
    char FileName[MAX_PATH];
    char FunctionName[100];
    uint64_t Flags;
};

struct UOI_PLAYER_XYZ_DATA
{
    int X;
    int Y;
    int Z;
};

struct UOI_SELECTED_TILE
{
    unsigned int Serial;
    unsigned short Graphic;
    unsigned short Color;
    int X;
    int Y;
    int Z;
    unsigned short LandGraphic;
    int LandX;
    int LandY;
    int LandZ;
};

struct UOI_MENU_RESPONSE
{
    unsigned int Serial;
    unsigned int ID;
    int Code;
};

struct PLUGIN_INTERFACE
{
    int InterfaceVersion;
    int Size;
    struct PLUGIN_CLIENT_INTERFACE *Client;

    WindowHandle Handle;
    int ClientVersion;
    int ClientFlags;

    PACKET_PROC *Recv;
    PACKET_PROC *Send;

    class CPlugin *Owner;

    EVENT_PROC *OnEvent;
    PACKET_PROC *OnRecv;
    PACKET_PROC *OnSend;
    VOID_PROC *OnDisconnect;
    VOID_PROC *OnWorldDraw;
    VOID_PROC *OnSceneDraw;
    WORLD_MAP_DRAW_PROC *OnWorldMapDraw;
};

typedef void PluginEntry(PLUGIN_INTERFACE *);
typedef void (*pfInstall)(PLUGIN_INTERFACE *);
typedef int (*pfcm)(int, char **);
typedef void (*pfri)(void *);

struct REVERSE_PLUGIN_INTERFACE
{
    pfInstall Install;
};
#pragma pack(pop)

extern REVERSE_PLUGIN_INTERFACE g_oaReverse;
#define ORIONUO_CLIENT "./orion.so"
