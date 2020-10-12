// MIT License
// Copyright (C) November 2017 Hotride

#include "PluginPackets.h"
#include "../Macro.h"
#include "../Managers/SkillsManager.h"
#include "../Managers/PluginManager.h"
#include "../Gumps/GumpSpellbook.h"
#include <common/utils.h> // countof

CPluginPacket::CPluginPacket()
{
    WriteUInt8(0xFC);
    WriteUInt16BE(0); //size reserved
}

CPluginPacket::CPluginPacket(int size, bool autoResize)
    : CDataWriter(size, autoResize)
{
}

void CPluginPacket::SendToPlugin()
{
    if (m_Data.size() >= 5)
    {
        pack16(&m_Data[1], (uint16_t)m_Data.size());
        g_PluginManager.PacketRecv(&m_Data[0], (int)m_Data.size());
    }
}

CPluginPacketSkillsList::CPluginPacketSkillsList()
    : CPluginPacket()
{
    int count = g_SkillsManager.Count;

    WriteUInt16BE(OIPMT_SKILL_LIST);
    WriteUInt16BE(count);

    for (int i = 0; i < count; i++)
    {
        CSkill *skill = g_SkillsManager.Get((uint32_t)i);

        if (skill == nullptr)
        {
            continue;
        }

        WriteUInt8(static_cast<uint8_t>(skill->Button));
        WriteString(skill->Name);
    }
}

CPluginPacketSpellsList::CPluginPacketSpellsList()
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_SPELL_LIST);
    WriteUInt16BE(7);

    for (int i = ST_FIRST; i < ST_COUNT; i++)
    {
        const auto book = GetSpellbook(i);
        WriteUInt16BE(book.SpellCount);
        for (int s = 0; s < book.SpellCount; s++)
        {
            WriteString(book.Spells[s]->Name);
        }
    }
}

CPluginPacketMacrosList::CPluginPacketMacrosList()
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_MACRO_LIST);
    WriteUInt16BE(countof(s_MacroActionName));
    for (int i = 0; i < countof(s_MacroActionName); i++)
    {
        WriteString(s_MacroActionName[i]);
        int count = 0;
        int offset = 0;
        CMacro::GetBoundByCode((MACRO_CODE)i, count, offset);
        WriteUInt16BE(count);
        for (int j = 0; j < count; j++)
        {
            WriteString(s_MacroAction[j + offset]);
        }
    }
}

CPluginPacketFileInfo::CPluginPacketFileInfo(int index, uint64_t address, uint64_t size)
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_FILE_INFO);
    WriteUInt16BE(index);
    WriteUInt64BE(address);
    WriteUInt64BE(size);
}

CPluginPacketFileInfoLocalized::CPluginPacketFileInfoLocalized(
    int index, uint64_t address, uint64_t size, const astr_t &language)
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_FILE_INFO_LOCALIZED);
    WriteUInt16BE(index);
    WriteUInt64BE(address);
    WriteUInt64BE(size);
    WriteString(language);
}

CPluginPacketStaticArtGraphicDataInfo::CPluginPacketStaticArtGraphicDataInfo(
    uint16_t graphic, uint64_t address, uint64_t size, uint64_t compressedSize)
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_GRAPHIC_DATA_INFO);
    WriteUInt8(OGDT_STATIC_ART);
    WriteUInt16BE(graphic);
    WriteUInt64BE(address);
    WriteUInt64BE(size);
    WriteUInt64BE(compressedSize);
}

CPluginPacketGumpArtGraphicDataInfo::CPluginPacketGumpArtGraphicDataInfo(
    uint16_t graphic,
    uint64_t address,
    uint64_t size,
    uint64_t compressedSize,
    uint16_t width,
    uint16_t height)
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_GRAPHIC_DATA_INFO);
    WriteUInt8(OGDT_GUMP_ART);
    WriteUInt16BE(graphic);
    WriteUInt64BE(address);
    WriteUInt64BE(size);
    WriteUInt64BE(compressedSize);
    WriteUInt16BE(width);
    WriteUInt16BE(height);
}

CPluginPacketFilesTransfered::CPluginPacketFilesTransfered()
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_FILES_TRANSFERED);
}

CPluginPacketOpenMap::CPluginPacketOpenMap()
    : CPluginPacket()
{
    WriteUInt16BE(OIPMT_OPEN_MAP);
}
