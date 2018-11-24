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

#define MAX_PATH 256
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
#define UOMSG_END_MACRO_PAYING USER_MESSAGE_ID + 672
#define UOMSG_UPDATE_REMOVE_POS USER_MESSAGE_ID + 673

typedef uint32_t WINDOW_PROC(WindowHandle, uint32_t, void *, void *);
typedef bool __cdecl PACKET_PROC(uint8_t *buf, size_t size);
typedef void __cdecl VOID_PROC();
typedef bool __cdecl WORLD_MAP_DRAW_PROC();

#pragma pack(push, 1)
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

    WINDOW_PROC *WindowProc;
    PACKET_PROC *OnRecv;
    PACKET_PROC *OnSend;
    VOID_PROC *OnDisconnect;
    VOID_PROC *OnWorldDraw;
    VOID_PROC *OnSceneDraw;
    WORLD_MAP_DRAW_PROC *OnWorldMapDraw;
};

typedef void __cdecl dllFunc(PLUGIN_INTERFACE *);
typedef void (*pfInstall)(PLUGIN_INTERFACE *intr);
typedef int (*pfcm)(int, char **);
typedef void (*pfri)(void *);

struct REVERSE_PLUGIN_INTERFACE
{
    pfInstall Install;
};
#pragma pack(pop)

extern REVERSE_PLUGIN_INTERFACE g_oaReverse;
#define ORIONUO_CLIENT "OrionUO/orion.so"
