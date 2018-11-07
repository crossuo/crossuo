/***********************************************************************************
**
** PluginInterface.h
**
** Copyright (C) September 2016 Hotride
**
************************************************************************************
*/

#pragma once

#if defined(ORION_LINUX)
#include "stdafx.h"
#define CDECL
#else
#include <Windows.h>
#endif

#pragma pack(push, 1)
typedef struct PLUGIN_INFO
{
    char FileName[MAX_PATH];
    char FunctionName[100];
    uint64_t Flags;
} * PPLUGIN_INFO;

#pragma pack(pop)

typedef uint32_t WINDOW_PROC(WindowHandle, uint32_t, void *, void *);
typedef bool CDECL PACKET_PROC(unsigned char *, const int &);
typedef void CDECL VOID_PROC();
typedef bool CDECL WORLD_MAP_DRAW_PROC();

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

#pragma pack(push, 1)
typedef struct UOI_PLAYER_XYZ_DATA
{
    int X;
    int Y;
    int Z;
} * PUOI_PLAYER_XYZ_DATA;

typedef struct UOI_SELECTED_TILE
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
} * PUOI_SELECTED_TILE;

typedef struct UOI_MENU_RESPONSE
{
    unsigned int Serial;
    unsigned int ID;
    int Code;
} * PUOI_MENU_RESPONSE;

bool CDECL PluginRecvFunction(unsigned char *buf, const int &size);
bool CDECL PluginSendFunction(unsigned char *buf, const int &size);

typedef struct PLUGIN_INTERFACE
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
} * PPLUGIN_INTERFACE;
#pragma pack(pop)
