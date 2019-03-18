// MIT License
// Copyright (C) August 2016 Hotride

#include "FileManager.h"
#include "AnimationManager.h"
#include "../CrossUO.h"
#include "../Application.h"
#include "../FileSystem.h"
#include "../Config.h"
#include "../Network/PluginPackets.h"

#define MINIZ_IMPLEMENTATION
#include <miniz.h>

// FIXME: g_App / g_Config / g_AnimationManager / (g_ClilocManager)
// Remove these dependencies, file manager should be standalone

string g_dumpUopFile;
CFileManager g_FileManager;
Data g_Data;

uint64_t CreateAssetHash(const char *s)
{
    const auto l = (uint32_t)strlen(s);
    uint32_t eax, ecx, edx, ebx, esi, edi;

    eax = ecx = edx = ebx = esi = edi = 0;
    ebx = edi = esi = l + 0xDEADBEEF;

    uint32_t i = 0;
    for (i = 0; i + 12 < l; i += 12)
    {
        edi = (uint32_t)((s[i + 7] << 24) | (s[i + 6] << 16) | (s[i + 5] << 8) | s[i + 4]) + edi;
        esi = (uint32_t)((s[i + 11] << 24) | (s[i + 10] << 16) | (s[i + 9] << 8) | s[i + 8]) + esi;
        edx = (uint32_t)((s[i + 3] << 24) | (s[i + 2] << 16) | (s[i + 1] << 8) | s[i]) - esi;

        edx = (edx + ebx) ^ (esi >> 28) ^ (esi << 4);
        esi += edi;
        edi = (edi - edx) ^ (edx >> 26) ^ (edx << 6);
        edx += esi;
        esi = (esi - edi) ^ (edi >> 24) ^ (edi << 8);
        edi += edx;
        ebx = (edx - esi) ^ (esi >> 16) ^ (esi << 16);
        esi += edi;
        edi = (edi - ebx) ^ (ebx >> 13) ^ (ebx << 19);
        ebx += esi;
        esi = (esi - edi) ^ (edi >> 28) ^ (edi << 4);
        edi += ebx;
    }

    if (l - i > 0)
    {
        switch (l - i)
        {
            case 12:
                esi += static_cast<uint32_t>(s[i + 11]) << 24;
                goto case_11;
                break;
            case 11:
            case_11:
                esi += static_cast<uint32_t>(s[i + 10]) << 16;
                goto case_10;
                break;
            case 10:
            case_10:
                esi += static_cast<uint32_t>(s[i + 9]) << 8;
                goto case_9;
                break;
            case 9:
            case_9:
                esi += s[i + 8];
                goto case_8;
                break;
            case 8:
            case_8:
                edi += static_cast<uint32_t>(s[i + 7]) << 24;
                goto case_7;
                break;
            case 7:
            case_7:
                edi += static_cast<uint32_t>(s[i + 6]) << 16;
                goto case_6;
                break;
            case 6:
            case_6:
                edi += static_cast<uint32_t>(s[i + 5]) << 8;
                goto case_5;
                break;
            case 5:
            case_5:
                edi += s[i + 4];
                goto case_4;
                break;
            case 4:
            case_4:
                ebx += static_cast<uint32_t>(s[i + 3]) << 24;
                goto case_3;
                break;
            case 3:
            case_3:
                ebx += static_cast<uint32_t>(s[i + 2]) << 16;
                goto case_2;
                break;
            case 2:
            case_2:
                ebx += static_cast<uint32_t>(s[i + 1]) << 8;
                goto case_1;
            case 1:
            case_1:
                ebx += s[i];
                break;
        }

        esi = (esi ^ edi) - ((edi >> 18) ^ (edi << 14));
        ecx = (esi ^ ebx) - ((esi >> 21) ^ (esi << 11));
        edi = (edi ^ ecx) - ((ecx >> 7) ^ (ecx << 25));
        esi = (esi ^ edi) - ((edi >> 16) ^ (edi << 16));
        edx = (esi ^ ecx) - ((esi >> 28) ^ (esi << 4));
        edi = (edi ^ edx) - ((edx >> 18) ^ (edx << 14));
        eax = (esi ^ edi) - ((edi >> 8) ^ (edi << 24));

        return (static_cast<uint64_t>(edi) << 32) | eax;
    }

    return (static_cast<uint64_t>(esi) << 32) | eax;
}

static uint32_t CRC32Table[256];
static uint32_t reflect(uint32_t source, int c)
{
    uint32_t value = 0;
    for (int i = 1; i < c + 1; i++)
    {
        if ((source & 0x1) != 0u)
        {
            value |= (1 << (c - i));
        }
        source >>= 1;
    }
    return value;
}

void InitChecksum32()
{
    for (int i = 0; i < 256; i++)
    {
        CRC32Table[i] = reflect((int)i, 8) << 24;
        for (int j = 0; j < 8; j++)
        {
            CRC32Table[i] =
                (CRC32Table[i] << 1) ^ ((CRC32Table[i] & (1 << 31)) != 0u ? 0x04C11DB7 : 0);
        }
        CRC32Table[i] = reflect(CRC32Table[i], 32);
    }
}

uint32_t Checksum32(uint8_t *ptr, size_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    while (size > 0)
    {
        crc = (crc >> 8) ^ CRC32Table[(crc & 0xFF) ^ *ptr];
        ptr++;
        size--;
    }
    return (crc & 0xFFFFFFFF);
}

void CUopMappedFile::Add(uint64_t hash, const UopBlockHeader *item)
{
    m_Map[hash] = item;
}

bool CUopMappedFile::HasAsset(uint64_t hash) const
{
    return m_Map.find(hash) != m_Map.end();
}

const UopBlockHeader *CUopMappedFile::GetBlock(uint64_t hash)
{
    DEBUG_TRACE_FUNCTION;
    auto found = m_Map.find(hash);
    if (found != m_Map.end())
    {
        return found->second;
    }

    return nullptr;
}

static bool DecompressBlock(const UopBlockHeader &block, uint8_t *dst, uint8_t *src)
{
    DEBUG_TRACE_FUNCTION;
    uLongf cLen = block.CompressedSize;
    uLongf dLen = block.DecompressedSize;
    if (cLen == 0 || block.Flags == 0)
    {
        dst = src;
        return true;
    }

    auto p = reinterpret_cast<unsigned char const *>(src);
    int z_err = mz_uncompress(dst, &dLen, p, cLen);
    if (z_err != Z_OK)
    {
        Error(Data, "decompression failed %d", z_err);
        return false;
    }
    return true;
}

vector<uint8_t> CUopMappedFile::GetData(const UopBlockHeader *block)
{
    DEBUG_TRACE_FUNCTION;
    assert(block);
    uint8_t *src = Start + block->Offset + block->HeaderSize;
    vector<uint8_t> dst(block->DecompressedSize, 0);
    if (DecompressBlock(*block, dst.data(), src))
    {
        return dst;
    }
    dst.clear();
    return dst;
}

// static
bool CFileManager::UopDecompressBlock(const UopBlockHeader &block, uint8_t *dst, int fileId)
{
    assert(fileId >= 0 && fileId <= countof(g_FileManager.m_AnimationFrame));
    uint8_t *src = g_FileManager.m_AnimationFrame[fileId].Start + block.Offset + block.HeaderSize;
    return DecompressBlock(block, dst, src);
}

bool CFileManager::Load()
{
    DEBUG_TRACE_FUNCTION;
    if (g_Config.ClientVersion >= CV_7000 && UopLoadFile(m_MainMisc, "MainMisc.uop"))
    {
        return LoadWithUop();
    }
    if (!m_ArtIdx.Load(g_App.UOFilesPath("artidx.mul")))
    {
        return false;
    }
    if (!m_ArtMul.Load(g_App.UOFilesPath("art.mul")))
    {
        return false;
    }
    if (!m_GumpIdx.Load(g_App.UOFilesPath("gumpidx.mul")))
    {
        return false;
    }
    if (!m_GumpMul.Load(g_App.UOFilesPath("gumpart.mul")))
    {
        return false;
    }
    if (!m_SoundIdx.Load(g_App.UOFilesPath("soundidx.mul")))
    {
        return false;
    }
    if (!m_SoundMul.Load(g_App.UOFilesPath("sound.mul")))
    {
        return false;
    }
    if (!m_AnimIdx[0].Load(g_App.UOFilesPath("anim.idx")))
    {
        return false;
    }
    if (!m_LightIdx.Load(g_App.UOFilesPath("lightidx.mul")))
    {
        return false;
    }
    if (!m_MultiIdx.Load(g_App.UOFilesPath("multi.idx")))
    {
        return false;
    }
    if (!m_SkillsIdx.Load(g_App.UOFilesPath("Skills.idx")))
    {
        return false;
    }
    if (!m_MultiMap.Load(g_App.UOFilesPath("Multimap.rle")))
    {
        return false;
    }
    if (!m_TextureIdx.Load(g_App.UOFilesPath("texidx.mul")))
    {
        return false;
    }
    if (!MulLoadFile(m_AnimMul[0], g_App.UOFilesPath("anim.mul")))
    {
        return false;
    }
    if (!m_AnimdataMul.Load(g_App.UOFilesPath("animdata.mul")))
    {
        return false;
    }
    if (!m_HuesMul.Load(g_App.UOFilesPath("hues.mul")))
    {
        return false;
    }
    if (!m_LightMul.Load(g_App.UOFilesPath("light.mul")))
    {
        return false;
    }
    if (!m_MultiMul.Load(g_App.UOFilesPath("multi.mul")))
    {
        return false;
    }
    if (!m_RadarcolMul.Load(g_App.UOFilesPath("radarcol.mul")))
    {
        return false;
    }
    if (!m_SkillsMul.Load(g_App.UOFilesPath("skills.mul")))
    {
        return false;
    }
    if (!m_TextureMul.Load(g_App.UOFilesPath("texmaps.mul")))
    {
        return false;
    }
    if (!m_TiledataMul.Load(g_App.UOFilesPath("tiledata.mul")))
    {
        return false;
    }

    m_SpeechMul.Load(g_App.UOFilesPath("speech.mul"));
    m_LangcodeIff.Load(g_App.UOFilesPath("Langcode.iff"));
    for (int i = 0; i < countof(m_AnimMul); i++)
    {
        if (i > 1)
        {
            m_AnimIdx[i].Load(g_App.UOFilesPath("anim%i.idx", i));
            MulLoadFile(m_AnimMul[i], g_App.UOFilesPath("anim%i.mul", i));
        }

        m_MapMul[i].Load(g_App.UOFilesPath("map%i.mul", i));

        m_StaticIdx[i].Load(g_App.UOFilesPath("staidx%i.mul", i));
        m_StaticMul[i].Load(g_App.UOFilesPath("statics%i.mul", i));
        m_FacetMul[i].Load(g_App.UOFilesPath("facet0%i.mul", i));

        m_MapDifl[i].Load(g_App.UOFilesPath("mapdifl%i.mul", i));
        m_MapDif[i].Load(g_App.UOFilesPath("mapdif%i.mul", i));

        m_StaDifl[i].Load(g_App.UOFilesPath("stadifl%i.mul", i));
        m_StaDifi[i].Load(g_App.UOFilesPath("stadifi%i.mul", i));
        m_StaDif[i].Load(g_App.UOFilesPath("stadif%i.mul", i));
    }

    for (int i = 0; i < countof(m_UnifontMul); i++)
    {
        auto s = i != 0 ? g_App.UOFilesPath("unifont%i.mul", i) : g_App.UOFilesPath("unifont.mul");
        if (m_UnifontMul[i].Load(s))
        {
            UnicodeFontsCount++;
        }
    }

    if (g_Config.UseVerdata && !m_VerdataMul.Load(g_App.UOFilesPath("verdata.mul")))
    {
        g_Config.UseVerdata = false;
    }

    return true;
}

bool CFileManager::LoadWithUop()
{
    DEBUG_TRACE_FUNCTION;
    //Try to use map uop files first, if we can, we will use them.
    if (!UopLoadFile(m_ArtLegacyMUL, "artLegacyMUL.uop"))
    {
        if (!m_ArtIdx.Load(g_App.UOFilesPath("artidx.mul")))
        {
            return false;
        }
        if (!m_ArtMul.Load(g_App.UOFilesPath("art.mul")))
        {
            return false;
        }
    }

    if (!UopLoadFile(m_GumpartLegacyMUL, "gumpartLegacyMUL.uop"))
    {
        if (!m_GumpIdx.Load(g_App.UOFilesPath("gumpidx.mul")))
        {
            return false;
        }
        if (!m_GumpMul.Load(g_App.UOFilesPath("gumpart.mul")))
        {
            return false;
        }

        UseUOPGumps = false;
    }
    else
    {
        UseUOPGumps = true;
    }

    if (!UopLoadFile(m_SoundLegacyMUL, "soundLegacyMUL.uop"))
    {
        if (!m_SoundIdx.Load(g_App.UOFilesPath("soundidx.mul")))
        {
            return false;
        }
        if (!m_SoundMul.Load(g_App.UOFilesPath("sound.mul")))
        {
            return false;
        }
    }

    if (!UopLoadFile(m_MultiCollection, "MultiCollection.uop"))
    {
        if (!m_MultiIdx.Load(g_App.UOFilesPath("multi.idx")))
        {
            return false;
        }
        if (!m_MultiMul.Load(g_App.UOFilesPath("multi.mul")))
        {
            return false;
        }
    }

    //UopLoadFile(m_AnimationSequence, "AnimationSequence.uop");
    UopLoadFile(m_Tileart, "tileart.uop");

    /* Эти файлы не используются самой последней версией клиента 7.0.52.2
	if (!m_tileart.Load(g_App.UOFilesPath("tileart.uop")))
	return false;
	if (!m_AnimationSequence.Load(g_App.UOFilesPath("AnimationSequence.uop")))
	return false;
	*/

    if (!m_AnimIdx[0].Load(g_App.UOFilesPath("anim.idx")))
    {
        return false;
    }
    if (!m_LightIdx.Load(g_App.UOFilesPath("lightidx.mul")))
    {
        return false;
    }
    if (!m_SkillsIdx.Load(g_App.UOFilesPath("Skills.idx")))
    {
        return false;
    }
    if (!m_MultiMap.Load(g_App.UOFilesPath("Multimap.rle")))
    {
        return false;
    }
    if (!m_TextureIdx.Load(g_App.UOFilesPath("texidx.mul")))
    {
        return false;
    }
    if (!MulLoadFile(m_AnimMul[0], g_App.UOFilesPath("anim.mul")))
    {
        return false;
    }
    if (!m_AnimdataMul.Load(g_App.UOFilesPath("animdata.mul")))
    {
        return false;
    }
    if (!m_HuesMul.Load(g_App.UOFilesPath("hues.mul")))
    {
        return false;
    }
    if (!m_LightMul.Load(g_App.UOFilesPath("light.mul")))
    {
        return false;
    }
    if (!m_RadarcolMul.Load(g_App.UOFilesPath("radarcol.mul")))
    {
        return false;
    }
    if (!m_SkillsMul.Load(g_App.UOFilesPath("skills.mul")))
    {
        return false;
    }
    if (!m_TextureMul.Load(g_App.UOFilesPath("texmaps.mul")))
    {
        return false;
    }
    if (!m_TiledataMul.Load(g_App.UOFilesPath("tiledata.mul")))
    {
        return false;
    }

    m_SpeechMul.Load(g_App.UOFilesPath("speech.mul"));
    m_LangcodeIff.Load(g_App.UOFilesPath("Langcode.iff"));
    for (int i = 0; i < countof(m_AnimMul); i++)
    {
        if (i > 1)
        {
            m_AnimIdx[i].Load(g_App.UOFilesPath("anim%i.idx", i));
            MulLoadFile(m_AnimMul[i], g_App.UOFilesPath("anim%i.mul", i));
        }

        string mapName = string("map") + std::to_string(i);
        if (!UopLoadFile(m_MapUOP[i], (mapName + "LegacyMUL.uop").c_str()))
        {
            m_MapMul[i].Load(g_App.UOFilesPath((mapName + ".mul")));
        }

        m_StaticIdx[i].Load(g_App.UOFilesPath("staidx%i.mul", i));
        m_StaticMul[i].Load(g_App.UOFilesPath("statics%i.mul", i));
        m_FacetMul[i].Load(g_App.UOFilesPath("facet0%i.mul", i));

        m_MapDifl[i].Load(g_App.UOFilesPath("mapdifl%i.mul", i));
        m_MapDif[i].Load(g_App.UOFilesPath("mapdif%i.mul", i));

        m_StaDifl[i].Load(g_App.UOFilesPath("stadifl%i.mul", i));
        m_StaDifi[i].Load(g_App.UOFilesPath("stadifi%i.mul", i));
        m_StaDif[i].Load(g_App.UOFilesPath("stadif%i.mul", i));
    }

    for (int i = 0; i < countof(m_UnifontMul); i++)
    {
        auto s = i != 0 ? g_App.UOFilesPath("unifont%i.mul", i) : g_App.UOFilesPath("unifont.mul");
        if (m_UnifontMul[i].Load(s))
        {
            UnicodeFontsCount++;
        }
    }

    if (g_Config.UseVerdata && !m_VerdataMul.Load(g_App.UOFilesPath("verdata.mul")))
    {
        g_Config.UseVerdata = false;
    }

    return true;
}

void CFileManager::Unload()
{
    DEBUG_TRACE_FUNCTION;
    m_ArtIdx.Unload();
    m_GumpIdx.Unload();
    m_SoundIdx.Unload();
    m_ArtMul.Unload();
    m_GumpMul.Unload();
    m_SoundMul.Unload();
    m_ArtLegacyMUL.Unload();
    m_GumpartLegacyMUL.Unload();
    m_SoundLegacyMUL.Unload();
    m_Tileart.Unload();
    m_MultiCollection.Unload();
    m_AnimationSequence.Unload();
    m_MainMisc.Unload();

    m_LightIdx.Unload();
    m_MultiIdx.Unload();
    m_SkillsIdx.Unload();

    m_MultiMap.Unload();
    m_TextureIdx.Unload();
    m_SpeechMul.Unload();
    m_AnimdataMul.Unload();

    m_HuesMul.Unload();

    m_LightMul.Unload();
    m_MultiMul.Unload();
    m_RadarcolMul.Unload();
    m_SkillsMul.Unload();

    m_TextureMul.Unload();
    m_TiledataMul.Unload();

    m_LangcodeIff.Unload();

    for (int i = 0; i < countof(m_AnimMul); i++)
    {
        m_AnimIdx[i].Unload();
        m_AnimMul[i].Unload();
        m_MapUOP[i].Unload();
        m_MapXUOP[i].Unload();
        m_MapMul[i].Unload();
        m_StaticIdx[i].Unload();
        m_StaticMul[i].Unload();
        m_FacetMul[i].Unload();

        m_MapDifl[i].Unload();
        m_MapDif[i].Unload();

        m_StaDifl[i].Unload();
        m_StaDifi[i].Unload();
        m_StaDif[i].Unload();
    }

    for (int i = 0; i < countof(m_UnifontMul); i++)
    {
        m_UnifontMul[i].Unload();
    }

    m_VerdataMul.Unload();
}

// TODO: remove this and instead it we can try share data memory with assistant
// this will be useful for fixing all the issues with assistant too
/*
void CFileManager::SendFilesInfo()
{
    DEBUG_TRACE_FUNCTION;
    if (m_TiledataMul.Start != nullptr)
    {
        CPluginPacketFileInfo(
            OFI_TILEDATA_MUL, (uint64_t)m_TiledataMul.Start, (uint64_t)m_TiledataMul.Size)
            .SendToPlugin();
    }

    if (m_MultiIdx.Start != nullptr)
    {
        CPluginPacketFileInfo(OFI_MULTI_IDX, (uint64_t)m_MultiIdx.Start, (uint64_t)m_MultiIdx.Size)
            .SendToPlugin();
    }

    if (m_MultiMul.Start != nullptr)
    {
        CPluginPacketFileInfo(OFI_MULTI_MUL, (uint64_t)m_MultiMul.Start, (uint64_t)m_MultiMul.Size)
            .SendToPlugin();
    }

    if (m_MultiCollection.Start != nullptr)
    {
        CPluginPacketFileInfo(
            OFI_MULTI_UOP, (uint64_t)m_MultiCollection.Start, (uint64_t)m_MultiCollection.Size)
            .SendToPlugin();
    }

    if (m_HuesMul.Start != nullptr)
    {
        CPluginPacketFileInfo(OFI_HUES_MUL, (uint64_t)m_HuesMul.Start, (uint64_t)m_HuesMul.Size)
            .SendToPlugin();
    }

    for (int i = 0; i < countof(m_MapMul); i++)
    {
        if (m_MapMul[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_MAP_0_MUL + i, (uint64_t)m_MapMul[i].Start, (uint64_t)m_MapMul[i].Size)
                .SendToPlugin();
        }

        if (m_MapUOP[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_MAP_0_UOP + i, (uint64_t)m_MapUOP[i].Start, (uint64_t)m_MapUOP[i].Size)
                .SendToPlugin();
        }

        if (m_MapXUOP[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_MAPX_0_UOP + i, (uint64_t)m_MapXUOP[i].Start, (uint64_t)m_MapXUOP[i].Size)
                .SendToPlugin();
        }

        if (m_StaticIdx[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_STAIDX_0_MUL + i, (uint64_t)m_StaticIdx[i].Start, (uint64_t)m_StaticIdx[i].Size)
                .SendToPlugin();
        }

        if (m_StaticMul[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_STATICS_0_MUL + i,
                (uint64_t)m_StaticMul[i].Start,
                (uint64_t)m_StaticMul[i].Size)
                .SendToPlugin();
        }

        if (m_MapDif[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_MAP_DIF_0_MUL + i, (uint64_t)m_MapDif[i].Start, (uint64_t)m_MapDif[i].Size)
                .SendToPlugin();
        }

        if (m_MapDifl[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_MAP_DIFL_0_MUL + i, (uint64_t)m_MapDifl[i].Start, (uint64_t)m_MapDifl[i].Size)
                .SendToPlugin();
        }

        if (m_StaDif[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_STA_DIF_0_MUL + i, (uint64_t)m_StaDif[i].Start, (uint64_t)m_StaDif[i].Size)
                .SendToPlugin();
        }

        if (m_StaDifi[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_STA_DIFI_0_MUL + i, (uint64_t)m_StaDifi[i].Start, (uint64_t)m_StaDifi[i].Size)
                .SendToPlugin();
        }

        if (m_StaDifl[i].Start != nullptr)
        {
            CPluginPacketFileInfo(
                OFI_STA_DIFL_0_MUL + i, (uint64_t)m_StaDifl[i].Start, (uint64_t)m_StaDifl[i].Size)
                .SendToPlugin();
        }
    }

    if (m_VerdataMul.Start != nullptr)
    {
        CPluginPacketFileInfo(
            OFI_VERDATA_MUL, (uint64_t)m_VerdataMul.Start, (uint64_t)m_VerdataMul.Size)
            .SendToPlugin();
    }

    if (m_RadarcolMul.Start != nullptr)
    {
        CPluginPacketFileInfo(
            OFI_RADARCOL_MUL, (uint64_t)m_RadarcolMul.Start, (uint64_t)m_RadarcolMul.Size)
            .SendToPlugin();
    }

    QFOR(item, g_ClilocManager.m_Items, CCliloc *)
    {
        if (item->Loaded && item->m_File.Start != nullptr)
        {
            CPluginPacketFileInfoLocalized(
                OFI_CLILOC_MUL,
                (uint64_t)item->m_File.Start,
                (uint64_t)item->m_File.Size,
                item->Language)
                .SendToPlugin();
        }
    }
}
*/

void CFileManager::UopReadAnimations()
{
    DEBUG_TRACE_FUNCTION;
    TRACE(Data, "start uop read jobs");
    std::thread readThread(&CFileManager::ReadTask, this);
    readThread.detach();
}

static int UopSetAnimationGroups(int start, int end)
{
    DEBUG_TRACE_FUNCTION;

    const static int count = countof(CFileManager::m_AnimationFrame);
    auto getFileWithAsset = [](uint64_t hash) -> int {
        for (int i = 0; i < count; ++i)
        {
            if (g_FileManager.m_AnimationFrame[i].HasAsset(hash))
                return i;
        }
        return -1;
    };

    TRACE(Data, "running job %d:%d", start, end);
    int lastGroup = 0;
    for (int animId = start; animId < end; ++animId)
    {
        auto &idx = g_AnimationManager.m_DataIndex[animId];
        for (int grpId = 0; grpId < ANIMATION_GROUPS_COUNT; ++grpId)
        {
            auto &group = idx.m_Groups[grpId];
            char hashString[100];
            snprintf(
                hashString,
                sizeof(hashString),
                "build/animationlegacyframe/%06d/%02d.bin",
                animId,
                grpId);
            const auto asset = CreateAssetHash(hashString);

            const auto fileIndex = getFileWithAsset(asset);
            if (fileIndex != -1)
            {
                if (grpId > lastGroup)
                {
                    lastGroup = grpId;
                }
                idx.IsUOP = true;
                //group.m_UOPAnimData = hashes.at(hash);
                for (int dirId = 0; dirId < 5; dirId++)
                {
                    auto &dir = group.m_Direction[dirId];
                    dir.IsUOP = true;
                    dir.BaseAddress = 0;
                    dir.Address = 0;
                    dir.FileIndex = fileIndex;
                }
            }
        }
    }

    return lastGroup;
}

void CFileManager::ReadTask()
{
    DEBUG_TRACE_FUNCTION;

    const static int count = countof(m_AnimationFrame);
    for (int i = 0; i < count; i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "AnimationFrame%d.uop", i + 1);

        auto &file = m_AnimationFrame[i];
        UopLoadFile(file, name);
    }
    UopLoadFile(m_AnimationSequence, "AnimationSequence.uop");

    int range = MAX_ANIMATIONS_DATA_INDEX_COUNT / count;
    int lastGroup[count];
    vector<std::thread> jobs;
    for (int i = 0; i < count; i++)
    {
        int start = range * i;
        int end = range * (i + 1);
        TRACE(Data, "scheduling job for %d with range from %d to %d", i + 1, start, end);
        auto job = [&, start, end]() { lastGroup[i] = UopSetAnimationGroups(start, end); };
        jobs.push_back(std::thread(job));
    }
    for (auto &job : jobs)
    {
        job.join();
    }
    ProcessAnimSequeceData();

    const int maxGroup = *std::max_element(lastGroup, lastGroup + count);
    if (g_AnimationManager.AnimGroupCount < maxGroup)
    {
        g_AnimationManager.AnimGroupCount = maxGroup;
    }
    m_AnimationSequence.Unload();
    m_AutoResetEvent.Set();
}

void CFileManager::ProcessAnimSequeceData()
{
    DEBUG_TRACE_FUNCTION;
    TRACE(Data, "processing AnimationSequence data");
    for (const auto /*&[hash, block]*/ &kvp : m_AnimationSequence.m_Map)
    {
        const auto hash = kvp.first;
        const auto block = kvp.second;
        auto data = m_AnimationSequence.GetData(block);
        SetData(reinterpret_cast<uint8_t *>(&data[0]), data.size());
        const uint32_t animId = ReadInt32LE();
        Move(48); // there's nothing there
        //amount of replaced indices, values seen in files so far: 29, 31, 32, 48, 68
        const uint32_t replaces = ReadInt32LE();
        // human and gargoyle are complicated, skip for now
        if (replaces == 48 || replaces == 68)
        {
            continue;
        }

        auto indexAnim = &g_AnimationManager.m_DataIndex[animId];
        for (int i = 0; i < replaces; ++i)
        {
            const auto oldIdx = ReadInt32LE();
            const auto frameCount = ReadInt32LE();
            auto group = indexAnim->m_Groups[oldIdx];
            if (frameCount == 0)
            {
                auto newIdx = ReadInt32LE();
                if (animId == 432 && oldIdx == 23)
                {
                    //fucking boura
                    newIdx = 29;
                }
                auto newGroup = indexAnim->m_Groups[newIdx];
                indexAnim->m_Groups[oldIdx] = newGroup;
                Move(60);
            }
            else
            {
                Move(64);
                /*
                for( int k = i; k < 5; ++k)
                {
                    group.m_Direction[k].FrameCount = frameCount;
                }
                */
            }
        }
        //There will be a moderate amount of data left at the end of the file
        //Seems like this data is essential to make AnimationSequence work
        // Aimed
    }
    Info(Data, "AnimationSequence processed %d entries", m_AnimationSequence.m_Map.size());
}

static void DateFromTimestamp(const time_t rawtime, char *out, int maxLen)
{
    struct tm *dt = localtime(&rawtime);
    strftime(out, maxLen, "%c", dt);
}

bool CFileManager::UopLoadFile(CUopMappedFile &file, const char *uopFilename)
{
    DEBUG_TRACE_FUNCTION;
    auto path{ g_App.UOFilesPath(uopFilename) };
    if (!fs_path_exists(path))
    {
        return false;
    }

    if (!file.Load(path))
    {
        return false;
    }

    const char *filename = CStringFromPath(path);
    DEBUG(Data, "loading UOP: %s", filename);
    file.Header = (UopHeader *)file.Start;
    if (file.Header->Magic != MYP_MAGIC)
    {
        Error(Data, "%d:unknown file format 0x%08x", filename, file.Header->Magic);
        return false;
    }

    if (file.Header->Version > 5)
    {
        Warning(Data, "%s:unexpected version %d", filename, file.Header->Version);
    }
    TRACE(Data, "%s:signature is 0x%08x", filename, file.Header->Signature);
    TRACE(Data, "%s:max_block_count is %d", filename, file.Header->MaxBlockCount);
    TRACE(Data, "%s:first_section at %d", filename, file.Header->FirstSection);
    TRACE(Data, "%s:file_count is %d", filename, file.Header->FileCount);
    file.ResetPtr();
    uint64_t next = file.Header->FirstSection;
    file.Move(next);
    do
    {
        auto section = (UopBlockSection *)file.Ptr;
        file.Move(sizeof(UopBlockSection));

        for (int i = 0; i < section->FileCount; i++)
        {
            auto item = (UopBlockHeader *)file.Ptr;
            if (item->Offset == 0 || item->DecompressedSize == 0)
            {
                continue;
            }
            //hashes[hash] = item;
            file.Add(item->Hash, item);
            file.Move(sizeof(UopBlockHeader));
        }
        next = section->NextSection;
        file.ResetPtr();
        file.Move(next);
    } while (next != 0);
    file.ResetPtr();

    if (!SDL_strcasecmp(uopFilename, g_dumpUopFile.c_str()))
    {
        char date[128];
        DEBUG(Data, "MypHeader for %s", uopFilename);
        DEBUG(Data, "\tVersion......: %d", file.Header->Version);
        DEBUG(Data, "\tSignature....: %08X", file.Header->Signature);
        DEBUG(Data, "\tFirstSection.: %016x", file.Header->FirstSection);
        DEBUG(Data, "\tMaxBlockCount: %d", file.Header->MaxBlockCount);
        DEBUG(Data, "\tFileCount....: %d", file.Header->FileCount);
        DEBUG(Data, "\tPad1.........: %08x", file.Header->Pad1);
        DEBUG(Data, "\tPad1.........: %08x", file.Header->Pad2);
        DEBUG(Data, "\tPad1.........: %08x", file.Header->Pad3);
        DEBUG(Data, "\tBlocks: ");
        for (const auto /*&[hash, block]*/ &kvp : file.m_Map)
        {
            const auto hash = kvp.first;
            const auto block = kvp.second;
            auto meta = (UopBlockMetadata *)(file.Start + block->Offset);

            DEBUG(Data, "\t\tBlock Header %08X_%016llX:", block->Checksum, block->Hash);
            DEBUG(Data, "\t\t\tOffset..........: %016llx", block->Hash);
            DEBUG(Data, "\t\t\tHeaderSize......: %d", block->HeaderSize);
            DEBUG(Data, "\t\t\tCompressedSize..: %d", block->CompressedSize);
            DEBUG(Data, "\t\t\tDecompressedSize: %d", block->DecompressedSize);
            DEBUG(Data, "\t\t\tHash............: %016llx", block->Hash);
            DEBUG(Data, "\t\t\tChecksum........: %08X", block->Checksum);
            DEBUG(Data, "\t\t\tFlags...........: %d", block->Flags);

            DEBUG(Data, "\t\t\tmetadata:");
            DEBUG(Data, "\t\t\t\tType.....: %d", meta->Type);
            DEBUG(Data, "\t\t\t\tOffset...: %04x", meta->Offset);
            DateFromTimestamp(meta->Timestamp / 100000000, date, sizeof(date));
            DEBUG(Data, "\t\t\t\tTimestamp: %s (%lld)", date, meta->Timestamp);
            /*
            vector<uint8_t> data = file.GetData(block);
            if (data.empty())
            {
                continue;
            }
            DEBUG_DUMP(Data, "CONTENTS:", data.data(), data.size());
            */
        }
        file.ResetPtr();
        exit(1);
    }
    return true;
}

bool CFileManager::MulLoadFile(Wisp::CMappedFile &file, const os_path &fileName)
{
    DEBUG_TRACE_FUNCTION;
    return file.Load(fileName);
}

bool CFileManager::IsMulFileOpen(int idx) const
{
    return idx < countof(m_AnimMul) ? m_AnimMul[idx].Start != 0 : false;
}

uint8_t *CFileManager::MulReadAnimationData(const CTextureAnimationDirection &direction) const
{
    auto &file = m_AnimMul[direction.FileIndex];
    return file.Start + direction.Address;
}

// tiledata.mul
// MulLandTileGroup[512] // land tile groups
// MulStaticTileGroup[...] // static tile groups to the end of the file
void CFileManager::LoadTiledata()
{
    DEBUG_TRACE_FUNCTION;

    auto &file = m_TiledataMul;
    Info(Data, "loading tiledata");
    const int landSize = 512;
    const auto landGroup =
        g_Config.ClientVersion < CV_7090 ? sizeof(MulLandTileGroup1) : sizeof(MulLandTileGroup2);
    const auto staticsGroup = g_Config.ClientVersion < CV_7090 ? sizeof(MulStaticTileGroup1) :
                                                                 sizeof(MulStaticTileGroup2);
    size_t staticsSize = (file.Size - (landSize * landGroup)) / staticsGroup;
    if (staticsSize > 2048)
    {
        staticsSize = 2048;
    }
    Info(Data, "landCount=%d", landSize);
    Info(Data, "staticsCount=%d", staticsSize);

    if (file.Size != 0u)
    {
        const bool isOldVersion = (g_Config.ClientVersion < CV_7090);
        file.ResetPtr();
        g_Data.m_Land.resize(landSize * 32);
        g_Data.m_Static.resize(staticsSize * 32);
        for (int i = 0; i < landSize; i++)
        {
            file.ReadUInt32LE();
            for (int j = 0; j < 32; j++)
            {
                auto &tile = g_Data.m_Land[(i * 32) + j];
                if (isOldVersion)
                {
                    tile.Flags = file.ReadUInt32LE();
                }
                else
                {
                    tile.Flags = file.ReadInt64LE();
                }
                tile.TexID = file.ReadUInt16LE();
                file.ReadBuffer(tile.Name);
            }
        }

        for (int i = 0; i < staticsSize; i++)
        {
            file.ReadUInt32LE();
            for (int j = 0; j < 32; j++)
            {
                auto &tile = g_Data.m_Static[(i * 32) + j];
                if (isOldVersion)
                {
                    tile.Flags = file.ReadUInt32LE();
                }
                else
                {
                    tile.Flags = file.ReadInt64LE();
                }
                tile.Weight = file.ReadInt8();
                tile.Layer = file.ReadInt8();
                tile.Count = file.ReadInt32LE();
                tile.AnimID = file.ReadInt16LE();
                tile.Hue = file.ReadInt16LE();
                tile.LightIndex = file.ReadInt16LE();
                tile.Height = file.ReadInt8();
                file.ReadBuffer(tile.Name);
            }
        }
    }
}

void CFileManager::LoadIndexFiles()
{
    DEBUG_TRACE_FUNCTION;
    Info(Client, "loading indexes");

    auto *LandArtPtr = (ArtIdxBlock *)m_ArtIdx.Start;
    auto *StaticArtPtr =
        (ArtIdxBlock *)((size_t)m_ArtIdx.Start + (g_Data.m_Land.size() * sizeof(ArtIdxBlock)));
    auto *GumpArtPtr = (GumpIdxBlock *)m_GumpIdx.Start;
    auto *TexturePtr = (TexIdxBlock *)m_TextureIdx.Start;
    auto *MultiPtr = (MultiIdxBlock *)m_MultiIdx.Start;
    auto *SoundPtr = (SoundIdxBlock *)m_SoundIdx.Start;
    auto *LightPtr = (LightIdxBlock *)m_LightIdx.Start;

    if (m_MultiCollection.Start != nullptr)
    {
        g_Index.m_MultiIndexCount = MAX_MULTI_DATA_INDEX_COUNT;
    }
    else
    {
        g_Index.m_MultiIndexCount = (int)(m_MultiIdx.Size / sizeof(MultiIdxBlock));

        if (g_Index.m_MultiIndexCount > MAX_MULTI_DATA_INDEX_COUNT)
        {
            g_Index.m_MultiIndexCount = MAX_MULTI_DATA_INDEX_COUNT;
        }
    }

    int maxGumpsCount =
        (int)(m_GumpIdx.Start == nullptr ? MAX_GUMP_DATA_INDEX_COUNT : (m_GumpIdx.End - m_GumpIdx.Start) / sizeof(GumpIdxBlock));
    if (m_ArtMul.Start != nullptr)
    {
        MulReadIndexFile(
            MAX_LAND_DATA_INDEX_COUNT,
            [&](int i) { return &g_Index.m_Land[i]; },
            (size_t)m_ArtMul.Start,
            LandArtPtr,
            [&LandArtPtr]() { return ++LandArtPtr; });
        MulReadIndexFile(
            g_Data.m_Anim.size(),
            [&](int i) { return &g_Index.m_Static[i]; },
            (size_t)m_ArtMul.Start,
            StaticArtPtr,
            [&StaticArtPtr]() { return ++StaticArtPtr; });
    }
    else
    {
        UopReadIndexFile(
            MAX_LAND_DATA_INDEX_COUNT,
            [&](int i) { return &g_Index.m_Land[i]; },
            "artLegacyMUL",
            8,
            ".tga",
            m_ArtLegacyMUL);
        UopReadIndexFile(
            g_Data.m_Anim.size() + MAX_LAND_DATA_INDEX_COUNT,
            [&](int i) { return &g_Index.m_Static[i - MAX_LAND_DATA_INDEX_COUNT]; },
            "artLegacyMUL",
            8,
            ".tga",
            m_ArtLegacyMUL,
            MAX_LAND_DATA_INDEX_COUNT);
    }

    if (m_SoundMul.Start != nullptr)
    {
        MulReadIndexFile(
            MAX_SOUND_DATA_INDEX_COUNT,
            [&](int i) { return &g_Index.m_Sound[i]; },
            (size_t)m_SoundMul.Start,
            SoundPtr,
            [&SoundPtr]() { return ++SoundPtr; });
    }
    else
    {
        UopReadIndexFile(
            MAX_SOUND_DATA_INDEX_COUNT,
            [&](int i) { return &g_Index.m_Sound[i]; },
            "soundLegacyMUL",
            8,
            ".dat",
            m_SoundLegacyMUL);
    }

    if (m_GumpMul.Start != nullptr)
    {
        MulReadIndexFile(
            maxGumpsCount,
            [&](int i) { return &g_Index.m_Gump[i]; },
            (size_t)m_GumpMul.Start,
            GumpArtPtr,
            [&GumpArtPtr]() { return ++GumpArtPtr; });
    }
    else
    {
        UopReadIndexFile(
            maxGumpsCount,
            [&](int i) { return &g_Index.m_Gump[i]; },
            "gumpartLegacyMUL",
            8,
            ".tga",
            m_GumpartLegacyMUL);
    }

    MulReadIndexFile(
        m_TextureIdx.Size / sizeof(TexIdxBlock),
        [&](int i) { return &g_Index.m_Texture[i]; },
        (size_t)m_TextureMul.Start,
        TexturePtr,
        [&TexturePtr]() { return ++TexturePtr; });
    MulReadIndexFile(
        MAX_LIGHTS_DATA_INDEX_COUNT,
        [&](int i) { return &g_Index.m_Light[i]; },
        (size_t)m_LightMul.Start,
        LightPtr,
        [&LightPtr]() { return ++LightPtr; });

    if (m_MultiMul.Start != nullptr)
    {
        MulReadIndexFile(
            g_Index.m_MultiIndexCount,
            [&](int i) { return &g_Index.m_Multi[i]; },
            (size_t)m_MultiMul.Start,
            MultiPtr,
            [&MultiPtr]() { return ++MultiPtr; });
    }
    else
    {
        CUopMappedFile &file = m_MultiCollection;
        for (const auto /*&[hash, block]*/ &kvp : file.m_Map)
        {
            const auto hash = kvp.first;
            const auto block = kvp.second;
            vector<uint8_t> data = file.GetData(block);
            if (data.empty())
            {
                continue;
            }

            Wisp::CDataReader reader(&data[0], data.size());
            uint32_t id = reader.ReadUInt32LE();
            if (id < MAX_MULTI_DATA_INDEX_COUNT)
            {
                CIndexMulti &index = g_Index.m_Multi[id];
                index.Address = size_t(file.Start + block->Offset + block->HeaderSize);
                index.DataSize = block->DecompressedSize;
                index.UopBlock = block;
                //index.ID = -1;
                index.Count = reader.ReadUInt32LE();
            }
        }
        //UopReadIndexFile(g_Index.m_MultiIndexCount, [&](int i){ return &g_Index.m_Multi[i]; }, "MultiCollection", 6, ".bin", m_MultiCollection);
    }
}

void CFileManager::MulReadIndexFile(
    size_t indexMaxCount,
    const std::function<CIndexObject *(int index)> &getIdxObj,
    size_t address,
    IndexBlock *ptr,
    const std::function<IndexBlock *()> &getNewPtrValue)
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < (int)indexMaxCount; i++)
    {
        CIndexObject *obj = getIdxObj((int)i);
        obj->ReadIndexFile(address, ptr);
        ptr = getNewPtrValue();
    }
}

// FIXME: move to FileManager
void CFileManager::UopReadIndexFile(
    size_t indexMaxCount,
    const std::function<CIndexObject *(int)> &getIdxObj,
    const char *uopFileName,
    int padding,
    const char *extesion,
    CUopMappedFile &uopFile,
    int startIndex)
{
    DEBUG_TRACE_FUNCTION;
    string p = uopFileName;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);

    bool isGump = (string("gumpartlegacymul") == p);
    char basePath[200] = { 0 };
    SDL_snprintf(basePath, sizeof(basePath), "build/%s/%%0%ii%s", p.c_str(), padding, extesion);

    for (int i = startIndex; i < (int)indexMaxCount; i++)
    {
        char hashString[200] = { 0 };
        SDL_snprintf(hashString, sizeof(hashString), basePath, (int)i);

        auto block = uopFile.GetBlock(CreateAssetHash(hashString));
        if (block != nullptr)
        {
            CIndexObject *obj = getIdxObj((int)i);
            obj->Address = uintptr_t(uopFile.Start + block->Offset + block->HeaderSize);
            obj->DataSize = block->DecompressedSize;
            obj->UopBlock = block;
            //obj->ID = -1;

            if (isGump)
            {
                obj->Address += 8;
                obj->DataSize -= 8;

                uopFile.ResetPtr();
                uopFile.Move(block->Offset + block->HeaderSize);

                obj->Width = uopFile.ReadUInt32LE();
                obj->Height = uopFile.ReadUInt32LE();
            }
        }
    }
}

void CFileManager::LoadData()
{
    g_Data.m_Anim.resize(m_AnimdataMul.Size);
    memcpy(&g_Data.m_Anim[0], &m_AnimdataMul.Start[0], m_AnimdataMul.Size);
    LoadTiledata();
    LoadIndexFiles();
}
