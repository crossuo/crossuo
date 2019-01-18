// MIT License
// Copyright (C) September 2016 Hotride

#pragma once

#include "../BaseQueue.h"
#include "../DefinitionMacro.h"

class CPlugin : public CBaseQueueItem
{
private:
    uint32_t m_Flags{ 0 };

public:
    CPlugin(uint32_t flags);
    virtual ~CPlugin();

    bool CanParseRecv() { return (m_Flags & PLUGIN_FLAGS_PARSE_RECV) != 0; }
    bool CanParseSend() { return (m_Flags & PLUGIN_FLAGS_PARSE_SEND) != 0; }
    bool CanRecv() { return (m_Flags & PLUGIN_FLAGS_RECV) != 0; }
    bool CanSend() { return (m_Flags & PLUGIN_FLAGS_SEND) != 0; }
    bool CanEvent() { return (m_Flags & PLUGIN_FLAGS_EVENT_PROC) != 0; }
    bool CanClientAccess() { return (m_Flags & PLUGIN_FLAGS_CLIENT_ACCESS) != 0; }
    bool CanEnterWorldRender() { return (m_Flags & PLUGIN_FLAGS_GAME_WORLD_DRAW) != 0; }
    bool CanEnterSceneRender() { return (m_Flags & PLUGIN_FLAGS_SCENE_DRAW) != 0; }
    bool CanEnterWorldMapRender() { return (m_Flags & PLUGIN_FLAGS_WORLD_MAP_DRAW) != 0; }

    PLUGIN_INTERFACE *m_PPS;
};

class CPluginManager : public CBaseQueue
{
public:
    CPluginManager();
    virtual ~CPluginManager() {}

    uint32_t OnEvent(uint32_t msg, const void *data);
    bool PacketRecv(uint8_t *buf, size_t size);
    bool PacketSend(uint8_t *buf, size_t size);
    void Disconnect();
    void WorldDraw();
    void SceneDraw();
    void WorldMapDraw();
};

extern CPluginManager g_PluginManager;
