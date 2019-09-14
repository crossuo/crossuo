// MIT License
// Copyright (C) November 2017 Hotride

#pragma once

#include <xuocore/mappedfile.h>

class CPluginPacket : public CDataWriter
{
public:
    CPluginPacket();
    CPluginPacket(int size, bool autoResize = false);

    void SendToPlugin();
};

class CPluginPacketSkillsList : public CPluginPacket
{
public:
    CPluginPacketSkillsList();
};

class CPluginPacketSpellsList : public CPluginPacket
{
public:
    CPluginPacketSpellsList();
};

class CPluginPacketMacrosList : public CPluginPacket
{
public:
    CPluginPacketMacrosList();
};

class CPluginPacketFileInfo : public CPluginPacket
{
public:
    CPluginPacketFileInfo(int index, uint64_t address, uint64_t size);
};

class CPluginPacketFileInfoLocalized : public CPluginPacket
{
public:
    CPluginPacketFileInfoLocalized(
        int index, uint64_t address, uint64_t size, const std::string &language);
};

class CPluginPacketStaticArtGraphicDataInfo : public CPluginPacket
{
public:
    CPluginPacketStaticArtGraphicDataInfo(
        uint16_t graphic, uint64_t address, uint64_t size, uint64_t compressedSize);
};

class CPluginPacketGumpArtGraphicDataInfo : public CPluginPacket
{
public:
    CPluginPacketGumpArtGraphicDataInfo(
        uint16_t graphic,
        uint64_t address,
        uint64_t size,
        uint64_t compressedSize,
        uint16_t width,
        uint16_t height);
};

class CPluginPacketFilesTransfered : public CPluginPacket
{
public:
    CPluginPacketFilesTransfered();
};

class CPluginPacketOpenMap : public CPluginPacket
{
public:
    CPluginPacketOpenMap();
};
