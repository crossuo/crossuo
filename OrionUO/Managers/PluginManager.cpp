// MIT License
// Copyright (C) September 2016 Hotride

#include "stdafx.h"
#if defined(ORION_LINUX)
#include <stdint.h>
#define CDECL
#else
#include <Windows.h>
#endif

CPluginManager g_PluginManager;

bool CDECL PluginRecvFunction(uint8_t *buf, const int &size)
{
    DEBUG_TRACE_FUNCTION;
    //SendMessage(g_OrionWindow.Handle, UOMSG_RECV, (WPARAM)buf, size);
    PUSH_EVENT(UOMSG_RECV, buf, size);
    g_PacketManager.SavePluginReceivePacket(buf, size);
    return true;
}

bool CDECL PluginSendFunction(uint8_t *buf, const int &size)
{
    DEBUG_TRACE_FUNCTION;

    //SendMessage(g_OrionWindow.Handle, UOMSG_SEND, (WPARAM)buf, size);
    PUSH_EVENT(UOMSG_SEND, buf, size);

    uint32_t ticks = g_Ticks;
    g_TotalSendSize += size;

    CPacketInfo &type = g_PacketManager.GetInfo(*buf);
    LOG("--- ^(%d) s(+%d => %d) Plugin->Server:: %s\n",
        ticks - g_LastPacketTime,
        size,
        g_TotalSendSize,
        type.Name);

    g_LastPacketTime = ticks;
    g_LastSendTime = ticks;

    if (*buf == 0x80 || *buf == 0x91)
    {
        LOG_DUMP(buf, 1);
        SAFE_LOG_DUMP(buf, size);
        LOG("**** ACCOUNT AND PASSWORD CENSORED ****\n");
    }
    else
    {
        LOG_DUMP(buf, size);
    }

    g_ConnectionManager.Send(buf, size);
    return true;
}

CPlugin::CPlugin(uint32_t flags)
    : CBaseQueueItem()
    , m_Flags(flags)
{
    DEBUG_TRACE_FUNCTION;
    m_PPS = new PLUGIN_INTERFACE();
    memset(m_PPS, 0, sizeof(PLUGIN_INTERFACE));

    m_PPS->Handle = g_OrionWindow.Handle;
    m_PPS->ClientVersion = g_PacketManager.GetClientVersion();
    m_PPS->ClientFlags = (g_FileManager.UseVerdata ? 0x01 : 0);
}

CPlugin::~CPlugin()
{
    DEBUG_TRACE_FUNCTION;
    if (m_PPS != nullptr)
    {
        delete m_PPS;
        m_PPS = nullptr;
    }
}

CPluginManager::CPluginManager()
    : CBaseQueue()
{
}

LRESULT CPluginManager::WindowProc(WindowHandle hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    DEBUG_TRACE_FUNCTION;
    LRESULT result = 0;

    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanWindowProc() && plugin->m_PPS->WindowProc != nullptr)
        {
            result = plugin->m_PPS->WindowProc(hWnd, msg, wparam, lparam);
            /*bool funRet = plugin->m_PPS->WindowProc(hWnd, msg, wparam, lparam);

			if (result)
				result = funRet;*/
        }
    }

    return result;
}

bool CPluginManager::PacketRecv(uint8_t *buf, int size)
{
    DEBUG_TRACE_FUNCTION;

    bool result = true;
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanParseRecv() && plugin->m_PPS->OnRecv != nullptr)
        {
            bool funRet = plugin->m_PPS->OnRecv(buf, size);

            if (result)
                result = funRet;
        }
    }

    return result;
}

bool CPluginManager::PacketSend(uint8_t *buf, int size)
{
    DEBUG_TRACE_FUNCTION;

    bool result = true;
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanParseSend() && plugin->m_PPS->OnSend != nullptr)
        {
            bool funRet = plugin->m_PPS->OnSend(buf, size);

            if (result)
                result = funRet;
        }
    }

    return result;
}

void CPluginManager::Disconnect()
{
    DEBUG_TRACE_FUNCTION;

    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->m_PPS->OnDisconnect != nullptr)
            plugin->m_PPS->OnDisconnect();
    }
}

void CPluginManager::WorldDraw()
{
    DEBUG_TRACE_FUNCTION;

    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEnterWorldRender() && plugin->m_PPS->OnWorldDraw != nullptr)
            plugin->m_PPS->OnWorldDraw();
    }
}

void CPluginManager::SceneDraw()
{
    DEBUG_TRACE_FUNCTION;

    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEnterSceneRender() && plugin->m_PPS->OnSceneDraw != nullptr)
            plugin->m_PPS->OnSceneDraw();
    }
}

void CPluginManager::WorldMapDraw()
{
    DEBUG_TRACE_FUNCTION;
    
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEnterWorldMapRender() && plugin->m_PPS->OnWorldMapDraw != nullptr)
            plugin->m_PPS->OnWorldMapDraw();
    }
}

