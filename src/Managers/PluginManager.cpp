// MIT License
// Copyright (C) September 2016 Hotride

#include "PluginManager.h"
#include "../Platform.h"
#include <common/utils.h>
#include <xuocore/commoninterfaces.h>

#include "PacketManager.h"
#include "ConnectionManager.h"
#include "../Config.h"
#include "../GameWindow.h"
#include "../Globals.h"

CPluginManager g_PluginManager;

bool CDECL PluginRecvFunction(uint8_t *buf, size_t size)
{
    auto owned = (uint8_t *)malloc(size);
    memcpy(owned, buf, size);
    PUSH_EVENT(UOMSG_RECV, owned, size);
    return true;
}

bool CDECL PluginSendFunction(uint8_t *buf, size_t size)
{
    auto owned = (uint8_t *)malloc(size);
    memcpy(owned, buf, size);
    PUSH_EVENT(UOMSG_SEND, owned, size);
    uint32_t ticks = g_Ticks;
    g_TotalSendSize += checked_cast<int>(size);
    CPacketInfo &_type = g_PacketManager.GetInfo(*buf);
    (void)_type;
    DEBUG(
        Plugin,
        "--- ^(%d) s(+%zd => %d) Plugin->Server:: %s",
        ticks - g_LastPacketTime,
        size,
        g_TotalSendSize,
        _type.Name);

    g_LastPacketTime = ticks;
    g_LastSendTime = ticks;
    if (*buf == 0x80 || *buf == 0x91)
    {
        DEBUG_DUMP(Plugin, "SEND:", buf, 1);
        SAFE_DEBUG_DUMP(Plugin, "SEND:", buf, int(size));
        DEBUG(Plugin, "**** ACCOUNT AND PASSWORD CENSORED ****");
    }
    else
    {
        DEBUG_DUMP(Plugin, "SEND:", buf, int(size));
    }

    g_ConnectionManager.Send(buf, checked_cast<int>(size));
    return true;
}

CPlugin::CPlugin(uint32_t flags)
    : m_Flags(flags)
{
    m_PPS = new PLUGIN_INTERFACE();
    memset(m_PPS, 0, sizeof(PLUGIN_INTERFACE));
    m_PPS->Handle = g_GameWindow.Handle;
    m_PPS->ClientVersion = g_Config.ProtocolClientVersion; // CHECK
    m_PPS->ClientFlags = (g_Config.UseVerdata ? 0x01 : 0);
}

CPlugin::~CPlugin()
{
    if (m_PPS != nullptr)
    {
        delete m_PPS;
        m_PPS = nullptr;
    }
}

CPluginManager::CPluginManager()
{
}

uint32_t CPluginManager::OnEvent(uint32_t msg, const void *data)
{
    uint32_t result = 0;
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEvent() && plugin->m_PPS->OnEvent != nullptr)
        {
            result = plugin->m_PPS->OnEvent(msg, data);
        }
    }
    return result;
}

bool CPluginManager::PacketRecv(uint8_t *buf, size_t size)
{
    bool result = true;
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanParseRecv() && plugin->m_PPS->OnRecv != nullptr)
        {
            bool funRet = plugin->m_PPS->OnRecv(buf, size);
            if (result)
            {
                result = funRet;
            }
        }
    }
    return result;
}

bool CPluginManager::PacketSend(uint8_t *buf, size_t size)
{
    bool result = true;
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanParseSend() && plugin->m_PPS->OnSend != nullptr)
        {
            bool funRet = plugin->m_PPS->OnSend(buf, size);
            if (result)
            {
                result = funRet;
            }
        }
    }
    return result;
}

void CPluginManager::Disconnect()
{
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->m_PPS->OnDisconnect != nullptr)
        {
            plugin->m_PPS->OnDisconnect();
        }
    }
}

void CPluginManager::WorldDraw()
{
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEnterWorldRender() && plugin->m_PPS->OnWorldDraw != nullptr)
        {
            plugin->m_PPS->OnWorldDraw();
        }
    }
}

void CPluginManager::SceneDraw()
{
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEnterSceneRender() && plugin->m_PPS->OnSceneDraw != nullptr)
        {
            plugin->m_PPS->OnSceneDraw();
        }
    }
}

void CPluginManager::WorldMapDraw()
{
    QFOR(plugin, m_Items, CPlugin *)
    {
        if (plugin->CanEnterWorldMapRender() && plugin->m_PPS->OnWorldMapDraw != nullptr)
        {
            plugin->m_PPS->OnWorldMapDraw();
        }
    }
}
