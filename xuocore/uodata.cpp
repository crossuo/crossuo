// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "uodata.h"
#include "mappedfile.h"
#include "mulstruct.h"
#include "text_parser.h"

#include <stdarg.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <inttypes.h>
#include <unordered_set>

#include <external/zlib_amalg.h>
#include <external/cbase64.h>

#include <common/utils.h>
#include <common/fs.h>
#include <common/checksum.h>

// FIXME: Animation data: group and direction are protected and can be refactored
#define ANIMATION_DEBUG 0
#define ANIMATION_DUMP 0

// clang-format off
#if ANIMATION_DEBUG
#define ANIMATION_ID 10
#define DBG_ANIM(...) if (ANIMATION_ID != 0) DEBUG(Data, __VA_ARGS__);
#define DBG_ANIM_ID(id, ...) if (id == ANIMATION_ID) DEBUG(Data, __VA_ARGS__);
#define DBG_ANIM_DECLARE(x) x;
#define DBG_ANIM_BLOCK(x) if (ANIMATION_ID != 0) { x; }
#define DBG_ANIM_ID_BLOCK(id, x) if (id == ANIMATION_ID) { x; }
#else // #if ANIMATION_DEBUG
#define DBG_ANIM(...)
#define DBG_ANIM_ID(...)
#define DBG_ANIM_DECLARE(...)
#define DBG_ANIM_BLOCK(...)
#define DBG_ANIM_ID_BLOCK(...)
#endif // #else // #if ANIMATION_DEBUG
// clang-format on

#define PALETTE_SIZE (sizeof(uint16_t) * 256)

enum
{
    PatchMap0 = 0x00,
    PatchStaIdx0 = 0x01,
    PatchStatics0 = 0x02,
    PatchArtiIdx = 0x03,
    PatchArt = 0x04,
    PatchAnimIdx = 0x05,
    PatchAnim = 0x06,
    PatchSoundIdx = 0x07,
    PatchSound = 0x08,
    PatchTexIdx = 0x09,
    PatchTexMaps = 0x0A,
    PatchGumpIdx = 0x0B,
    PatchGumpArt = 0x0C,
    PatchMultiIdx = 0x0D,
    PatchMulti = 0x0E,
    PatchSkillsIdx = 0x0F,
    PatchSkills = 0x10,
    PatchTileData = 0x1E,
    PatchAnimData = 0x1F,
    PatchHues = 0x20,
};

astr_t g_dumpUopFile;
UOData g_Data;
Index g_Index;
CFileManager g_FileManager;

MapSize g_MapSize[MAX_MAPS_COUNT] = {
    // Felucca      Trammel         Ilshenar        Malas           Tokuno          TerMur
    { 7168, 4096 }, { 7168, 4096 }, { 2304, 1600 }, { 2560, 2048 }, { 1448, 1448 }, { 1280, 4096 },
};
MapSize g_MapBlockSize[MAX_MAPS_COUNT];

static uint32_t s_ClientVersion = 0;
static bool s_UseVerdata = false;
static char s_UOPath[FS_MAX_PATH];
static fs_path UOFilePath(const char *str, ...)
{
    va_list arg;
    va_start(arg, str);
    char out[FS_MAX_PATH] = { 0 };
    vsnprintf(out, sizeof(out) - 1, str, arg);
    va_end(arg);
    fs_path p = fs_path_join(s_UOPath, out);
    return fs_insensitive(p);
}

void uo_data_init(const char *path, uint32_t client_version, bool use_verdata)
{
    auto len = strlen(path);
    auto max = sizeof(s_UOPath) - 1;
    auto amount = len > max ? max : len;
    strncpy(s_UOPath, path, amount);
    s_UOPath[amount + 1] = 0;
    s_ClientVersion = client_version;
    s_UseVerdata = use_verdata;
}

void uo_animation_dump(const char *file)
{
#if ANIMATION_DUMP
    auto fp = fs_open(fs_path_from(file), FS_WRITE);
    fprintf(
        fp,
        "anim,group,dir,framecount,file,uop,verdata,address,size,patched,base_address,base_size,patched_address,patched_size\n");
    for (int a = 0; a < MAX_ANIMATIONS_DATA_INDEX_COUNT; a++)
    {
        const auto &data = g_Index.m_Anim[a];
        for (int g = 0; g < MAX_ANIMATION_GROUPS_COUNT; g++)
        {
            const auto &group = data.Groups[g];
            for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
            {
                const auto &dir = group.Direction[d];
                const auto patched = dir.Address == dir.PatchedAddress;
                fprintf(
                    fp,
                    "0x%04X,%d,%d,%d,%d,%d,%d,0x%016" PRIx64 ",%u,%d,0x%016" PRIx64
                    ",%u,0x%016" PRIx64 ",%u\n",
                    a,
                    g,
                    d,
                    dir.FrameCount,
                    dir.FileIndex,
                    dir.IsUOP,
                    dir.IsVerdata,
                    (uint64_t)dir.Address,
                    dir.Size,
                    patched,
                    (uint64_t)dir.BaseAddress,
                    dir.BaseSize,
                    (uint64_t)dir.PatchedAddress,
                    dir.PatchedSize);
            }
        }
    }
    fs_close(fp);
#endif // ANIMATION_DUMP
}

void CIndexObject::ReadIndexFile(size_t address, IndexBlock *ptr)
{
    Address = ptr->Position;
    DataSize = ptr->Size;
    if (Address == 0xFFFFFFFF || (DataSize == 0) || DataSize == 0xFFFFFFFF)
    {
        Address = 0;
        DataSize = 0;
    }
    else
    {
        Address = Address + address;
    }
};

void CIndexMulti::ReadIndexFile(size_t address, IndexBlock *block)
{
    CIndexObject::ReadIndexFile(address, block);
    if (s_ClientVersion >= VERSION(7, 0, 9, 0))
    {
        Count = (uint16_t)(DataSize / sizeof(MULTI_BLOCK_NEW));
    }
    else
    {
        Count = (uint16_t)(DataSize / sizeof(MULTI_BLOCK));
    }
};

void CIndexLight::ReadIndexFile(size_t address, IndexBlock *block)
{
    CIndexObject::ReadIndexFile(address, block);
    Width = block->LightData.Width;
    Height = block->LightData.Height;
};

void CIndexGump::ReadIndexFile(size_t address, IndexBlock *block)
{
    CIndexObject::ReadIndexFile(address, block);
    Width = block->LightData.Width;
    Height = block->LightData.Height;
};

void CUopMappedFile::AddAsset(const UopFileEntry *item)
{
    m_NameHashes.push_back(item->Hash);
    m_FileOffsets.push_back(item->Offset);
    m_MapByHash[item->Hash] = item;
    m_MapByOffset[item->Offset] = item;
}

bool CUopMappedFile::HasAsset(uint64_t hash) const
{
    return m_MapByHash.find(hash) != m_MapByHash.end();
}

const UopFileEntry *CUopMappedFile::GetAsset(uint64_t hash) const
{
    auto found = m_MapByHash.find(hash);
    if (found != m_MapByHash.end())
    {
        return found->second;
    }

    return nullptr;
}

const UopFileEntry *CUopMappedFile::GetAsset(const char *filename) const
{
    const uint64_t nameHash = uo_jenkins_hash(filename);
    return GetAsset(nameHash);
}

size_t CUopMappedFile::FileCount() const
{
    return m_MapByHash.size();
}

static bool DecompressBlock(const UopFileEntry *block, uint8_t *dst, uint8_t *src)
{
    assert(block);
    uLongf cLen = block->CompressedSize;
    uLongf dLen = block->DecompressedSize;
    if (cLen == 0 || block->Flags == 0)
    {
        dst = src;
        return true;
    }

    auto p = reinterpret_cast<unsigned char const *>(src);
    int z_err = uncompress(dst, &dLen, p, cLen);
    if (z_err != Z_OK)
    {
        Error(Data, "decompression failed %d", z_err);
        return false;
    }
    return true;
}

static bool UopDecompressBlock(const UopFileEntry *block, uint8_t *dst, int fileId)
{
    assert(block);
    assert(fileId >= 0 && fileId <= countof(g_FileManager.m_AnimationFrame));
    uint8_t *src =
        g_FileManager.m_AnimationFrame[fileId].Start + block->Offset + block->MetadataSize;
    return DecompressBlock(block, dst, src);
}

std::vector<uint8_t> CUopMappedFile::GetMeta(const UopFileEntry *block)
{
    assert(block);
    uint8_t *src = Start + block->Offset;
    std::vector<uint8_t> dst;
    dst.assign(src, src + block->MetadataSize);
    return dst;
}

std::vector<uint8_t> CUopMappedFile::GetRaw(const UopFileEntry *block)
{
    assert(block);
    uint8_t *src = Start + block->Offset + block->MetadataSize;
    std::vector<uint8_t> dst;
    dst.assign(src, src + block->CompressedSize);
    return dst;
}

std::vector<uint8_t> CUopMappedFile::GetData(const UopFileEntry *block)
{
    assert(block);
    uint8_t *src = Start + block->Offset + block->MetadataSize;
    std::vector<uint8_t> dst(block->DecompressedSize, 0);
    if (DecompressBlock(block, dst.data(), src))
    {
        return dst;
    }
    dst.clear();
    return dst;
}

bool CFileManager::Load()
{
    DBG_ANIM("Animation debugging tracing enabled");
    if (s_ClientVersion >= CV_7000)
    {
        g_FileManager.UopReadAnimations();
    }

    g_Index.m_Animations.reserve(2 * 1024);
    bool r = false;
    const bool useUop =
        s_ClientVersion >= VERSION(7, 0, 0, 0) && UopLoadFile(m_MainMisc, "MainMisc.uop");

    if (useUop)
    {
        r = LoadWithUop();
    }
    else
    {
        r = LoadWithMul();
    }

    r &= LoadCommon();

    for (int i = 0; i < countof(m_AnimMul); i++)
    {
        if (i > 1)
        {
            m_AnimIdx[i].Load(UOFilePath("anim%i.idx", i));
            m_AnimMul[i].Load(UOFilePath("anim%i.mul", i));
        }

        if (useUop)
        {
            char uopMapName[64];
            snprintf(uopMapName, sizeof(uopMapName) - 1, "map%dLegacyMUL.uop", i);
            // pattern: "build/map%dlegacymul/%08d.dat"
            if (!UopLoadFile(m_MapUOP[i], uopMapName))
            {
                m_MapMul[i].Load(UOFilePath("map%d.mul", i));
            }
        }
        else
        {
            m_MapMul[i].Load(UOFilePath("map%d.mul", i));
        }

        m_StaticIdx[i].Load(UOFilePath("staidx%i.mul", i));
        m_StaticMul[i].Load(UOFilePath("statics%i.mul", i));
        m_FacetMul[i].Load(UOFilePath("facet0%i.mul", i));

        m_MapDifl[i].Load(UOFilePath("mapdifl%i.mul", i));
        m_MapDif[i].Load(UOFilePath("mapdif%i.mul", i));

        m_StaDifl[i].Load(UOFilePath("stadifl%i.mul", i));
        m_StaDifi[i].Load(UOFilePath("stadifi%i.mul", i));
        m_StaDif[i].Load(UOFilePath("stadif%i.mul", i));
    }

    for (int i = 0; i < countof(m_UnifontMul); i++)
    {
        auto s = i != 0 ? UOFilePath("unifont%i.mul", i) : UOFilePath("unifont.mul");
        if (m_UnifontMul[i].Load(s))
        {
            UnicodeFontsCount++;
        }
    }

    if (s_UseVerdata && !m_VerdataMul.Load(UOFilePath("verdata.mul")))
    {
        s_UseVerdata = false;
    }

    if (!m_AnimdataMul.Size)
    {
        Fatal(
            Data, "Failed to load Ultima Online(tm) data files.", "could not load 'animdata.mul'");
        return false;
    }

    g_Data.m_Anim.resize(m_AnimdataMul.Size);
    memcpy(&g_Data.m_Anim[0], &m_AnimdataMul.Start[0], m_AnimdataMul.Size);
    LoadTiledata();
    LoadIndexFiles();

    Info(Data, "loading skills");
    LoadSkills();
    Info(Data, "loading hues");
    LoadHues();
    Info(Client, "creating map blocksTable");
    CreateBlocksTable();

    Info(Data, "patching files");
    PatchFiles();
    Info(Data, "replacing indexes");
    IndexReplaces();

    LoadAnimations();

    if (s_ClientVersion >= CV_7000)
    {
        g_FileManager.ProcessAnimSequeceData();
    }

    return r;
}

bool CFileManager::LoadCommon()
{
    if (!m_AnimIdx[0].Load(UOFilePath("anim.idx")))
    {
        return false;
    }
    if (!m_LightIdx.Load(UOFilePath("lightidx.mul")))
    {
        return false;
    }
    if (!m_SkillsIdx.Load(UOFilePath("Skills.idx")))
    {
        return false;
    }
    if (!m_MultiMap.Load(UOFilePath("Multimap.rle")))
    {
        if (!m_MultiMap.Load(UOFilePath("multimap.rle")))
        {
            return false;
        }
    }
    if (!m_TextureIdx.Load(UOFilePath("texidx.mul")))
    {
        return false;
    }
    if (!m_AnimMul[0].Load(UOFilePath("anim.mul")))
    {
        return false;
    }
    if (!m_AnimdataMul.Load(UOFilePath("animdata.mul")))
    {
        return false;
    }
    if (!m_HuesMul.Load(UOFilePath("hues.mul")))
    {
        return false;
    }
    if (!m_LightMul.Load(UOFilePath("light.mul")))
    {
        return false;
    }
    if (!m_RadarcolMul.Load(UOFilePath("radarcol.mul")))
    {
        return false;
    }
    if (!m_SkillsMul.Load(UOFilePath("skills.mul")))
    {
        if (!m_SkillsMul.Load(UOFilePath("Skills.mul")))
        {
            return false;
        }
    }
    if (!m_TextureMul.Load(UOFilePath("texmaps.mul")))
    {
        return false;
    }
    if (!m_TiledataMul.Load(UOFilePath("tiledata.mul")))
    {
        return false;
    }

    m_SpeechMul.Load(UOFilePath("speech.mul"));
    m_LangcodeIff.Load(UOFilePath("Langcode.iff"));

    return true;
}

bool CFileManager::LoadWithMul()
{
    if (!m_ArtIdx.Load(UOFilePath("artidx.mul")))
    {
        return false;
    }
    if (!m_ArtMul.Load(UOFilePath("art.mul")))
    {
        return false;
    }
    if (!m_GumpIdx.Load(UOFilePath("gumpidx.mul")))
    {
        return false;
    }
    if (!m_GumpMul.Load(UOFilePath("gumpart.mul")))
    {
        return false;
    }
    if (!m_SoundIdx.Load(UOFilePath("soundidx.mul")))
    {
        return false;
    }
    if (!m_SoundMul.Load(UOFilePath("sound.mul")))
    {
        return false;
    }
    if (!m_MultiIdx.Load(UOFilePath("multi.idx")))
    {
        return false;
    }
    if (!m_MultiMul.Load(UOFilePath("multi.mul")))
    {
        return false;
    }

    return true;
}

bool CFileManager::LoadWithUop()
{
    if (UopLoadFile(m_StringDictionary, "string_dictionary.uop"))
    {
        LoadStringDictionary();
    }

    //Try to use map uop files first, if we can, we will use them.
    // pattern: "build/artlegacymul/%08d.tga"
    if (!UopLoadFile(m_ArtLegacyMUL, "artLegacyMUL.uop"))
    {
        if (!m_ArtIdx.Load(UOFilePath("artidx.mul")))
        {
            return false;
        }
        if (!m_ArtMul.Load(UOFilePath("art.mul")))
        {
            return false;
        }
    }

    // pattern: "build/gumpartlegacymul/%08d.tga"
    if (!UopLoadFile(m_GumpartLegacyMUL, "gumpartLegacyMUL.uop")) // extra width,height
    {
        if (!m_GumpIdx.Load(UOFilePath("gumpidx.mul")))
        {
            return false;
        }
        if (!m_GumpMul.Load(UOFilePath("gumpart.mul")))
        {
            return false;
        }

        UseUOPGumps = false;
    }
    else
    {
        UseUOPGumps = true;
    }

    // pattern: "build/soundlegacymul/%08d.dat"
    if (!UopLoadFile(m_SoundLegacyMUL, "soundLegacyMUL.uop"))
    {
        if (!m_SoundIdx.Load(UOFilePath("soundidx.mul")))
        {
            return false;
        }
        if (!m_SoundMul.Load(UOFilePath("sound.mul")))
        {
            return false;
        }
    }

    // pattern: "build/multicollection/%06d.bin"
    if (!UopLoadFile(m_MultiCollection, "MultiCollection.uop"))
    {
        if (!m_MultiIdx.Load(UOFilePath("multi.idx")))
        {
            return false;
        }
        if (!m_MultiMul.Load(UOFilePath("multi.mul")))
        {
            return false;
        }
    }

    //UopLoadFile(m_AnimationSequence, "AnimationSequence.uop");
    UopLoadFile(m_Tileart, "tileart.uop"); // FIXME: parse tileart definitions

    /* Эти файлы не используются самой последней версией клиента 7.0.52.2
	if (!m_tileart.Load(UOFilePath("tileart.uop")))
	return false;
	if (!m_AnimationSequence.Load(UOFilePath("AnimationSequence.uop")))
	return false;
	*/

    return true;
}

void CFileManager::Unload()
{
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

    m_Skills.clear();
}

static const UopFileEntry *UopGetAnimationAsset(uint64_t hash, int &fileIndex)
{
    const static int count = countof(CFileManager::m_AnimationFrame);
    auto getAssetOwner = [](uint64_t hash) -> int {
        for (int i = 0; i < count; ++i)
        {
            if (g_FileManager.m_AnimationFrame[i].HasAsset(hash))
                return i;
        }
        return -1;
    };

    fileIndex = getAssetOwner(hash);
    if (fileIndex == -1)
        return nullptr;

    return g_FileManager.m_AnimationFrame[fileIndex].GetAsset(hash);
}

void CFileManager::UopReadAnimations()
{
    // pattern: "build/animationsequence/%08d.bin"
    DBG_ANIM("Reading AnimationSequence.uop");
    UopLoadFile(m_AnimationSequence, "AnimationSequence.uop");
    const static int count = countof(m_AnimationFrame);
    for (int i = 0; i < count; i++)
    {
        char name[64];
        // pattern: "build/animationlegacyframe/%06d/%02d.bin"
        snprintf(name, sizeof(name), "AnimationFrame%d.uop", i + 1);
        auto &file = m_AnimationFrame[i];
        DBG_ANIM("Reading %s", name);
        UopLoadFile(file, name);
    }

    char filename[200];
    const char *pattern = "build/animationlegacyframe/%06d/%02d.bin";
    g_Index.m_AnimationGroupFile.reserve(16 * 1024);
    for (int animId = 0; animId < MAX_ANIMATIONS_DATA_INDEX_COUNT; animId++)
    {
        DBG_ANIM_ID(animId, "Loading animation asset:");
        auto &idx = g_Index.m_Anim[animId];
        for (int grpId = 0; grpId < MAX_ANIMATION_GROUPS_COUNT; ++grpId)
        {
            auto &group = idx.Groups[grpId];
            snprintf(filename, sizeof(filename), pattern, animId, grpId);
            const auto asset = uo_jenkins_hash(filename);
            int fileIndex;
            const auto data = UopGetAnimationAsset(asset, fileIndex);
            if (data != nullptr)
            {
                DBG_ANIM_ID(
                    animId,
                    "\tGroup: %d (%d:%s) hash: %016" PRIx64,
                    grpId,
                    fileIndex,
                    filename,
                    asset);
                g_Index.m_AnimationGroupFile.emplace(GroupId(animId, grpId), data);
                idx.IsUOP = true;
                for (int dirId = 0; dirId < 5; dirId++)
                {
                    auto &dir = group.Direction[dirId];
                    dir.IsUOP = true;
                    dir.BaseAddress = 0;
                    dir.Address = 0;
                    dir.FileIndex = fileIndex;
                }
            }
        }
    }
}

// TODO: use map structure instead byte peeking
// pattern: "build/animationsequence/%08d.bin"
void CFileManager::ProcessAnimSequeceData() // "AnimationSequence.uop"
{
    TRACE(Data, "processing AnimationSequence data");
    for (const auto /*&[hash, block]*/ &kvp : m_AnimationSequence.m_MapByHash)
    {
        //const auto hash = kvp.first;
        const auto block = kvp.second;
        auto data = m_AnimationSequence.GetData(block);
        SetData(reinterpret_cast<uint8_t *>(&data[0]), data.size());
        const auto entry = (UopAnimationSequence *)Ptr;
        Move(sizeof(UopAnimationSequence));
        assert(entry->Unk1 == 0 && entry->Unk2 == 0);
        assert(entry->Unk3 == 0 && entry->Unk4 == 0);
        assert(entry->Unk5 == 0 && entry->Unk6 == 0);

        // FIXME
        // amount of replaced indices, values seen in files so far: 29, 31, 32, 48, 68
        // human and gargoyle are complicated, skip for now
        // offset 52
        if (entry->Replaces == 0x30 /*48*/ || entry->Replaces == 0x44 /*68*/)
        {
            // 0x30 0xc070396e5a7ec7f4 0x029A (666) // GENDER_MALE RT_GARGOYLE
            // 0x30 0xc6c811fa536c8b62 0x04E5 (1253) // ?
            // 0x44 0x6c1031f63255845a 0x0190 (400) // GENDER_MALE RT_HUMAN
            continue;
        }
        assert(entry->Graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);
        auto anim = &g_Index.m_Anim[entry->Graphic];
        const bool canPatch = (anim->Flags & AF_USE_UOP_ANIMATION) != 0;
        for (uint32_t replaceIdx = 0; replaceIdx < entry->Replaces; ++replaceIdx)
        {
            const auto seq = (UopAnimationSequenceReplacement *)Ptr;
            assert(seq->Group < MAX_ANIMATION_GROUPS_COUNT);
            if (seq->FrameCount == 0)
            {
                if (canPatch)
                {
                    assert(seq->NewGroup != seq->Group);
                    assert(seq->NewGroup < MAX_ANIMATION_GROUPS_COUNT);
                    anim->Groups[seq->Group] = anim->Groups[seq->NewGroup];
                    // update replaced file blocks with replacement entries
                    auto file = uo_animation_group_get(GroupId(entry->Graphic, seq->NewGroup));
                    g_Index.m_AnimationGroupFile.emplace(GroupId(entry->Graphic, seq->Group), file);
                }
            }
            else
            {
                auto &group = anim->Groups[seq->Group];
                for (int k = replaceIdx; k < 5; ++k)
                {
                    group.Direction[k].FrameCount = seq->FrameCount;
                }
            }
            Move(sizeof(UopAnimationSequenceReplacement));
        }
        switch (entry->Graphic)
        {
            case 0x042D:
            case 0x04E6: // Tiger
            case 0x04E7: // Tiger
            {
                anim->MountedHeightOffset = 18;
            }
            break;
            case 0x01B0: // a boura // && oldIdx == 23 // newIdx = 29
            case 0x0579:
            case 0x05F6:
            case 0x05A0:
            {
                anim->MountedHeightOffset = 9;
            }
            break;
        }
    }
    Info(Data, "AnimationSequence processed %zd entries", m_AnimationSequence.FileCount());
    m_AnimationSequence.Unload();
}

static void DateFromTimestamp(const time_t rawtime, char *out, int maxLen)
{
    struct tm *dt = localtime(&rawtime);
    strftime(out, maxLen, "%c", dt);
}

#define MAX_STRINGS (128 * 1000)
static const char *g_strings[MAX_STRINGS];
static std::vector<uint8_t> g_stringData;
void CFileManager::LoadStringDictionary()
{
    if (m_StringDictionary.Start != nullptr)
    {
        auto block = g_FileManager.m_StringDictionary.GetAsset(Asset::Strings);
        if (block != nullptr)
        {
            g_stringData = g_FileManager.m_StringDictionary.GetData(block);
            CDataReader reader;
            reader.SetData(&g_stringData[0], g_stringData.size());
            auto header = (UopDictionary *)reader.Start;
            Info(
                Data,
                "dictionary: 0x%08x 0x%08x ... 0x%08x - total: %u",
                header->Unk1,
                header->Unk2,
                header->Unk3,
                header->Count);
            reader.Move(sizeof(UopDictionary));

            assert(header->Count - 1 < MAX_STRINGS);
            for (int i = 0; i < int(header->Count - 1) && i < MAX_STRINGS; ++i)
            {
                const auto len = reader.ReadInt16LE();
                *(reader.Ptr - 2) = '\0';
                g_strings[i + 1] = (const char *)reader.Ptr;
                reader.Move(len);
            }
            //for (int i = 1; g_strings[i]; ++i)
            //{
            //    fprintf(stdout, " %d: %s\n", i, g_strings[i]);
            //}
        }
    }
}

bool CFileManager::UopLoadFile(CUopMappedFile &file, const char *uopFilename, bool dumpInfo)
{
    auto path{ UOFilePath(uopFilename) };
    if (!fs_path_exists(path))
    {
        return false;
    }

    if (!file.Load(path))
    {
        return false;
    }

    const char *filename = fs_path_ascii(path);
    DEBUG(Data, "loading UOP: %s", filename);
    file.Header = (UopHeader *)file.Start;
    if (file.Header->Magic != MYP_MAGIC)
    {
        Error(Data, "%s:unknown file format 0x%08x", filename, file.Header->Magic);
        return false;
    }

    if (file.Header->Version > 5)
    {
        Warning(Data, "%s:unexpected version %d", filename, file.Header->Version);
    }
    TRACE(Data, "%s:signature is 0x%08x", filename, file.Header->Signature);
    TRACE(Data, "%s:file_count is %d", filename, file.Header->FileCount);

    dumpInfo = dumpInfo || (g_dumpUopFile.compare(uopFilename) == 0);

    file.ResetPtr();
    uint64_t next = file.Header->SectionOffset;
    file.Move(checked_cast<intptr_t>(next));
    do
    {
        auto section = (UopSection *)file.Ptr;
        file.Move(sizeof(UopSection));

        for (uint32_t i = 0; i < section->FileCount; i++)
        {
            auto entry = (UopFileEntry *)file.Ptr;
            file.Move(sizeof(UopFileEntry));
            if (entry->Offset == 0)
            {
                continue;
            }
            file.AddAsset(entry);
        }
        next = section->NextSection;
        file.ResetPtr();
        file.Move(checked_cast<intptr_t>(next));
    } while (next != 0);
    file.ResetPtr();

    if (dumpInfo)
    {
        std::sort(file.m_NameHashes.begin(), file.m_NameHashes.end());
        std::sort(file.m_FileOffsets.begin(), file.m_FileOffsets.end());

        char outname[FS_MAX_PATH];
        snprintf(outname, sizeof(outname), "%s.log", uopFilename);
        auto fp = fs_open(fs_path_from(outname), FS_WRITE);

        fprintf(fp, "MypHeader for %s\n", uopFilename);
        fprintf(fp, "\tVersion......: %d\n", file.Header->Version);
        fprintf(fp, "\tSignature....: %08X\n", file.Header->Signature);
        fprintf(fp, "\tSectionOffset: %016" PRIx64 "\n", file.Header->SectionOffset);
        fprintf(fp, "\tFileCapacity.: %d\n", file.Header->FileCapacity);
        fprintf(fp, "\tFileCount....: %d (Real: %zu)\n", file.Header->FileCount, file.FileCount());
        fprintf(fp, "\tSectionCount.: %08x\n", file.Header->SectionCount);
        fprintf(fp, "\tUnk1.........: %08x\n", file.Header->Unk1);
        fprintf(fp, "\tUnk2.........: %08x\n", file.Header->Unk2);
        fprintf(fp, "\tBlocks:\n");
        //for (const auto /*&[hash, block]*/ &kvp : file.m_Map)
        for (const auto it : file.m_NameHashes)
        //for (const auto it : file.m_FileOffsets)
        {
            //const auto hash = kvp.first;
            //const auto block = kvp.second;
            //const auto block = file.m_Map[it];
            const auto block = file.m_MapByHash[it];

            auto meta = (UopFileMetadata *)(file.Start + block->Offset);
            fprintf(fp, "\t\tBlock Header %08X_%016" PRIX64 ":\n", block->Checksum, block->Hash);
            fprintf(fp, "\t\t\tOffset..........: %016" PRIx64 "\n", block->Offset);
            fprintf(fp, "\t\t\tMetadataSize....: %d\n", block->MetadataSize);
            fprintf(fp, "\t\t\tCompressedSize..: %d\n", block->CompressedSize);
            fprintf(fp, "\t\t\tDecompressedSize: %d\n", block->DecompressedSize);
            fprintf(
                fp,
                "\t\t\tHash............: %016" PRIx64 " (%08x%08x)\n",
                block->Hash,
                UOP_HASH_SH(block->Hash),
                UOP_HASH_PH(block->Hash));
            fprintf(fp, "\t\t\tChecksum........: %08X\n", block->Checksum);
            fprintf(fp, "\t\t\tFlags...........: %d\n", block->Flags);

            fprintf(fp, "\t\t\tMetadata........:\n");
            fprintf(fp, "\t\t\t\tType....: %d\n", meta->Type);
            fprintf(fp, "\t\t\t\tSize....: %d\n", meta->Size);
            switch (meta->Type)
            {
                case 3:
                {
                    auto meta3 = (UopFileMetadata3 *)(meta);
                    assert(sizeof(UopFileMetadata3) == block->MetadataSize);
                    char date[128];
                    DateFromTimestamp(meta3->Timestamp / 100000000, date, sizeof(date));
                    fprintf(fp, "\t\t\t\tTimestamp: %s (%" PRIu64 ")\n", date, meta3->Timestamp);
                }
                break;

                case 4:
                {
                    assert(sizeof(UopFileMetadata) + meta->Size == block->MetadataSize);
                    auto data = (uint8_t *)(file.Start + block->Offset + sizeof(UopFileMetadata));
                    cbase64_encodestate state;
                    cbase64_init_encodestate(&state);
                    char buf[2048] = {};
                    cbase64_encode_block(data, meta->Size, buf, &state);
                    fprintf(fp, "\t\t\t\tData....: %s\n", buf);
                }
                break;

                case 5:
                {
                }
                break;

                default:
                {
                    fprintf(fp, "Unknown Meta Type: %d\n", meta->Type);
                    assert(false && "unknown metadata type");
                }
                break;
            };
        }
        fs_close(fp);
        file.ResetPtr();
    }
    return true;
}

bool CFileManager::IsMulFileOpen(int idx) const
{
    return idx < countof(m_AnimMul) ? m_AnimMul[idx].Start != 0 : false;
}

uint8_t *CFileManager::MulReadAnimationData(const AnimationDirection &direction) const
{
    auto &file = m_AnimMul[direction.FileIndex];
    return file.Start + direction.Address;
}

// tiledata.mul
// MulLandTileGroup[512] // land tile groups
// MulStaticTileGroup[...] // static tile groups to the end of the file
void CFileManager::LoadTiledata()
{
    auto &file = m_TiledataMul;
    Info(Data, "loading tiledata");
    const bool isOldVersion = (s_ClientVersion < VERSION(7, 0, 9, 0));
    const int landSize = 512;
    const auto landGroup = isOldVersion ? sizeof(MulLandTileGroup1) : sizeof(MulLandTileGroup2);
    const auto staticsGroup =
        isOldVersion ? sizeof(MulStaticTileGroup1) : sizeof(MulStaticTileGroup2);
    size_t staticsSize = (file.Size - (landSize * landGroup)) / staticsGroup;
    if (staticsSize > 2048)
    {
        staticsSize = 2048;
    }
    Info(Data, "landCount=%d", landSize);
    Info(Data, "staticsCount=%zu", staticsSize);

    if (file.Size != 0u)
    {
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

        for (size_t i = 0; i < staticsSize; i++)
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
    Info(Data, "loading indexes");

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
            g_Data.m_Static.size(),
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
            g_Data.m_Static.size() + MAX_LAND_DATA_INDEX_COUNT,
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
        for (const auto /*&[hash, block]*/ &kvp : file.m_MapByHash)
        {
            //const auto hash = kvp.first;
            const auto block = kvp.second;
            std::vector<uint8_t> data = file.GetData(block);
            if (data.empty())
            {
                continue;
            }

            CDataReader reader(&data[0], data.size());
            uint32_t id = reader.ReadUInt32LE();
            if (id < MAX_MULTI_DATA_INDEX_COUNT)
            {
                CIndexMulti &index = g_Index.m_Multi[id];
                index.Address = size_t(file.Start + block->Offset + block->MetadataSize);
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
    for (int i = 0; i < (int)indexMaxCount; i++)
    {
        CIndexObject *obj = getIdxObj((int)i);
        obj->ReadIndexFile(address, ptr);
        ptr = getNewPtrValue();
    }
}

void CFileManager::UopReadIndexFile(
    size_t indexMaxCount,
    const std::function<CIndexObject *(int)> &getIdxObj,
    const char *uopFileName,
    int padding,
    const char *extesion,
    CUopMappedFile &uopFile,
    int startIndex)
{
    astr_t p = uopFileName;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);

    bool isGump = (astr_t("gumpartlegacymul") == p);
    char basePath[100];
    snprintf(basePath, sizeof(basePath), "build/%s/%%0%ii%s", p.c_str(), padding, extesion);

    for (int i = startIndex; i < (int)indexMaxCount; i++)
    {
        char filename[200];
        snprintf(filename, sizeof(filename), basePath, (int)i);

        auto block = uopFile.GetAsset(filename);
        if (block != nullptr)
        {
            CIndexObject *obj = getIdxObj((int)i);
            obj->Address = uintptr_t(uopFile.Start + block->Offset + block->MetadataSize);
            obj->DataSize = block->DecompressedSize;
            obj->UopBlock = block;
            //obj->ID = -1;

            if (isGump)
            {
                obj->Address += 8;
                obj->DataSize -= 8;

                uopFile.ResetPtr();
                uopFile.Move(checked_cast<intptr_t>(block->Offset + block->MetadataSize));

                obj->Width = uopFile.ReadUInt32LE();
                obj->Height = uopFile.ReadUInt32LE();
            }
        }
    }
}

UopAnimationHeader CFileManager::UopReadAnimationHeader()
{
    UopAnimationHeader hdr;
    hdr.Format = ReadUInt32LE();
    hdr.Version = ReadUInt32LE();
    hdr.DecompressedSize = ReadUInt32LE();
    hdr.AnimationId = ReadUInt32LE();
    hdr.Unk1 = ReadUInt32LE();
    hdr.Unk2 = ReadUInt32LE();
    hdr.Unk3 = ReadInt16LE();
    hdr.Unk4 = ReadInt16LE();
    hdr.HeaderSize = ReadUInt32LE();
    hdr.FrameCount = ReadUInt32LE();
    hdr.Offset = ReadUInt32LE();
    Ptr = Start + hdr.Offset;
    return hdr;
}

UopAnimationFrame CFileManager::UopReadAnimationFrame()
{
    UopAnimationFrame frame;
    frame.DataStart = Ptr;
    frame.GroupId = ReadInt16LE();
    frame.FrameId = ReadInt16LE();
    frame.Unk1 = ReadUInt32LE();
    frame.Unk2 = ReadUInt32LE();
    frame.PixelDataOffset = ReadUInt32LE();
    return frame;
}

std::vector<UopAnimationFrame> CFileManager::UopReadAnimationFramesData()
{
    auto header = UopReadAnimationHeader();
    std::vector<UopAnimationFrame> data;
    data.reserve(header.FrameCount);
    for (uint32_t i = 0; i < header.FrameCount; i++)
    {
        data.emplace_back(UopReadAnimationFrame());
    }
    return data;
}

void CFileManager::UopReadAnimationFrameInfo(
    AnimationFrameInfo &result,
    AnimationDirection &direction,
    const UopFileEntry *block,
    bool isCorpse)
{
    if (block == nullptr || block->Hash == 0)
    {
        return;
    }

    std::vector<uint8_t> scratchBuffer;
    scratchBuffer.reserve(block->DecompressedSize);
    if (!UopDecompressBlock(block, scratchBuffer.data(), direction.FileIndex))
    {
        return;
    }
    // FIXME: CFileManager SHOULD not be a DataReader! In fact, no class need to be. Clean up!
    SetData(scratchBuffer.data(), block->DecompressedSize);
    const auto _header = UopReadAnimationHeader();
    (void)_header;
    auto frame = UopReadAnimationFrame(); // read only first frame to get image dimensions
    Ptr = frame.DataStart + frame.PixelDataOffset + PALETTE_SIZE;
    auto frameInfo = (AnimationFrameInfo *)Ptr;
    result = *frameInfo;
    Move(sizeof(AnimationFrameInfo));
}

inline bool
CFileManager::UopReadAnimationFrames(const AnimationState &anim, LoadPixelData16Cb pLoadFunc)
{
    const auto block = uo_animation_group_get(anim);
    if (block == nullptr || block->Hash == 0)
    {
        return false;
    }

    std::vector<uint8_t> scratchBuffer;
    assert(anim.Group < MAX_ANIMATION_GROUPS_COUNT);
    auto &direction = g_Index.m_Anim[anim.Graphic].Groups[anim.Group].Direction[anim.Direction];
    scratchBuffer.reserve(block->DecompressedSize);
    if (!UopDecompressBlock(block, scratchBuffer.data(), direction.FileIndex))
    {
        return false;
    }
    SetData(scratchBuffer.data(), block->DecompressedSize);

    const auto framesData = UopReadAnimationFramesData();
    const int frameCount = checked_cast<uint8_t>(framesData.size() / 5);
    direction.FrameCount = frameCount;
    int dirFrameStartIdx = frameCount * anim.Direction;

    if (uo_animation_get(anim) != nullptr)
    {
        return true;
    }
    auto animation = uo_animation_create(anim);

    for (int i = 0; i < frameCount; i++)
    {
        UopAnimationFrame frameData = framesData[i + dirFrameStartIdx];
        if (frameData.DataStart == nullptr)
        {
            continue;
        }

        Ptr = frameData.DataStart + frameData.PixelDataOffset;
        auto palette = (uint16_t *)Ptr;
        Move(PALETTE_SIZE);

        auto &frame = animation->Frames[i];
        LoadAnimationFrame(frame, palette, pLoadFunc);
    }
    return true;
}

void CFileManager::MulReadAnimationFrameInfo(
    AnimationFrameInfo &result, AnimationDirection &direction, uint8_t frameIndex, bool isCorpse)
{
    auto ptr = (uint8_t *)direction.Address;
    if (!direction.IsVerdata)
    {
        ptr = MulReadAnimationData(direction);
    }
    SetData(ptr, direction.Size);

    Move(PALETTE_SIZE);
    uint8_t *dataStart = Ptr;
    int frameCount = ReadUInt32LE();
    if (frameCount > 0 && frameIndex >= frameCount)
    {
        frameIndex = 0;
    }

    if (frameIndex < frameCount)
    {
        uint32_t *frameOffset = (uint32_t *)Ptr;
        Ptr = dataStart + frameOffset[frameIndex];
        auto frameInfo = (AnimationFrameInfo *)Ptr;
        result = *frameInfo;
    }
}

inline bool
CFileManager::MulReadAnimationFrames(const AnimationState &anim, LoadPixelData16Cb pLoadFunc)
{
    assert(anim.Group < MAX_ANIMATION_GROUPS_COUNT);
    auto &direction = g_Index.m_Anim[anim.Graphic].Groups[anim.Group].Direction[anim.Direction];
    auto ptr = (uint8_t *)direction.Address;
    if (!direction.IsVerdata)
    {
        ptr = MulReadAnimationData(direction);
    }
    SetData(ptr, direction.Size);

    uint16_t *palette = (uint16_t *)Start;
    Move(PALETTE_SIZE);

    uint8_t *dataStart = Ptr;
    const uint32_t frameCount = ReadUInt32LE();
    uint32_t *frameOffset = (uint32_t *)Ptr;
    //uint16_t color = g_Index.m_Anim[graphic].Color;

    direction.FrameCount = frameCount;
    if (uo_animation_get(anim) != nullptr)
    {
        return true;
    }
    auto animation = uo_animation_create(anim);

    for (uint32_t i = 0; i < frameCount; i++)
    {
        auto &frame = animation->Frames[i];
        if (frame.UserData != nullptr)
        {
            continue;
        }
        Ptr = dataStart + frameOffset[i];
        LoadAnimationFrame(frame, palette, pLoadFunc);
    }

    return true;
}

void CFileManager::LoadAnimationFrame(
    AnimationFrame &frame, uint16_t *palette, LoadPixelData16Cb pLoadFunc)
{
    auto frameInfo = (AnimationFrameInfo *)Ptr;
    Move(sizeof(AnimationFrameInfo));

    frame.CenterX = frameInfo->CenterX;
    frame.CenterY = frameInfo->CenterY;
    if (frameInfo->Width == 0 || frameInfo->Height == 0)
    {
        Error(Data, "image data invalid (%d,%d)", frameInfo->Width, frameInfo->Height);
        return;
    }

    const auto imageWidth = frameInfo->Width;
    const auto imageHeight = frameInfo->Height;
    const auto imageCenterX = frameInfo->CenterX;
    const auto imageCenterY = frameInfo->CenterY;
    const int textureSize = imageWidth * imageHeight;
    std::vector<uint16_t> pixels(textureSize, 0);
    if (pixels.size() != textureSize)
    {
        Error(Data, "couldn't allocate pixel data (%d bytes)", textureSize);
        return;
    }

    uint32_t header = ReadUInt32LE();
    while (header != 0x7FFF7FFF && !IsEOF())
    {
        const uint16_t runLength = (header & 0x0FFF);
        int x = (header >> 22) & 0x03FF;
        if ((x & 0x0200) != 0)
        {
            x |= 0xFFFFFE00;
        }

        int y = (header >> 12) & 0x03FF;
        if ((y & 0x0200) != 0)
        {
            y |= 0xFFFFFE00;
        }

        x += imageCenterX;
        y += imageCenterY + imageHeight;
        int block = (y * imageWidth) + x;
        for (int k = 0; k < runLength; k++)
        {
            const uint8_t paletteIndex = ReadUInt8();
            uint16_t val = palette[paletteIndex];
            if (val != 0)
            {
                val |= 0x8000;
            }
            pixels[block++] = val;
        }
        header = ReadUInt32LE();
    }

    assert(pLoadFunc);
    assert(frame.UserData == nullptr);
    frame.UserData = pLoadFunc(imageWidth, imageHeight, pixels.data());
}

void CFileManager::LoadAnimationFrameInfo(
    AnimationFrameInfo &result, const AnimationState &anim, uint8_t frameIndex, bool isCorpse)
{
    assert(anim.Group < MAX_ANIMATION_GROUPS_COUNT);
    auto &direction = g_Index.m_Anim[anim.Graphic].Groups[anim.Group].Direction[0];
    if (direction.Size != 0)
    {
        MulReadAnimationFrameInfo(result, direction, frameIndex, isCorpse);
    }
    else
    {
        auto file = uo_animation_group_get(anim);
        UopReadAnimationFrameInfo(result, direction, file, isCorpse);
    }
}

bool CFileManager::LoadAnimation(const AnimationState &anim, LoadPixelData16Cb pLoadFunc)
{
    assert(anim.Group < MAX_ANIMATION_GROUPS_COUNT);
    auto &direction = g_Index.m_Anim[anim.Graphic].Groups[anim.Group].Direction[anim.Direction];
    bool res = false;
    if (direction.Size != 0)
    {
        res = MulReadAnimationFrames(anim, pLoadFunc);
    }
    else
    {
        res = UopReadAnimationFrames(anim, pLoadFunc);
    }
    return res;
}

ANIMATION_GROUPS_TYPE uo_type_by_graphic(uint16_t graphic)
{
    return graphic < 200 ? AGT_MONSTER : (graphic < 400 ? AGT_ANIMAL : AGT_HUMAN);
}

uint32_t uo_get_group_offset(ANIMATION_GROUPS group, uint16_t graphic)
{
    switch (group)
    {
        case AG_LOW:
        {
            return ((graphic - 200) * 65 + 22000) * sizeof(AnimIdxBlock);
        }
        break;

        case AG_HIGH:
        {
            return graphic * 110 * sizeof(AnimIdxBlock);
        }
        break;

        case AG_PEOPLE:
        {
            return ((graphic - 400) * 175 + 35000) * sizeof(AnimIdxBlock);
        }
        break;

        case AG_NONE:
        case AG_INVALID:
        default:
        {
            assert(false && "Invalid group parameter");
        }
    }
    return -1;
}

static bool uo_anim_group_is_die(ANIMATION_GROUPS_TYPE type, uint8_t group)
{
    switch (type)
    {
        case AGT_MONSTER:
        case AGT_SEA_MONSTER:
        {
            return (group == HAG_DIE_1 || group == HAG_DIE_2);
        }
        case AGT_HUMAN:
        case AGT_EQUIPMENT:
        {
            return (group == PAG_DIE_1 || group == PAG_DIE_2);
        }
        case AGT_ANIMAL:
        {
            return (group == LAG_DIE_1 || group == LAG_DIE_2);
        }
        default:
            break;
    }
    return false;
}

/* AnimationLoader */

static uint32_t uo_group_count_by_type[] = {
    HAG_ANIMATION_COUNT, // AGT_MONSTER
    HAG_ANIMATION_COUNT, // AGT_SEA_MONSTER
    LAG_ANIMATION_COUNT, // AGT_ANIMAL
    PAG_ANIMATION_COUNT, // AGT_HUMAN
    PAG_ANIMATION_COUNT, // AGT_EQUIPMENT
};
static_assert(countof(uo_group_count_by_type) == AGT_COUNT, "missing count for group");

/*
static uint32_t uo_group_count_by_group[] = {
    0, // AG_NONE
    LAG_ANIMATION_COUNT, // AG_LOW
    HAG_ANIMATION_COUNT, // AG_HIGH
    PAG_ANIMATION_COUNT, // AG_PEOPLE
};
*/

uint64_t
uo_get_anim_offset(uint16_t graphic, uint32_t flags, ANIMATION_GROUPS_TYPE type, int &groupCount)
{
    uint64_t result = 0;
    groupCount = 0;
    ANIMATION_GROUPS group = AG_NONE;
    switch (type)
    {
        case AGT_MONSTER:
        {
            if (flags & AF_CALCULATE_OFFSET_BY_PEOPLE_GROUP)
            {
                group = AG_PEOPLE;
            }
            else if (flags & AF_CALCULATE_OFFSET_BY_LOW_GROUP)
            {
                group = AG_LOW;
            }
            else
            {
                group = AG_HIGH;
            }
        }
        break;

        case AGT_SEA_MONSTER:
        {
            result = uo_get_group_offset(AG_HIGH, graphic);
            groupCount = LAG_ANIMATION_COUNT;
        }
        break;

        case AGT_ANIMAL:
        {
            if (flags & AF_CALCULATE_OFFSET_LOW_GROUP_EXTENDED)
            {
                if (flags & AF_CALCULATE_OFFSET_BY_PEOPLE_GROUP)
                {
                    group = AG_PEOPLE;
                }
                else if (flags & AF_CALCULATE_OFFSET_BY_LOW_GROUP)
                {
                    group = AG_LOW;
                }
                else
                {
                    group = AG_HIGH;
                }
            }
            else
            {
                group = AG_LOW;
            }
        }
        break;

        default:
        {
            group = AG_PEOPLE;
        }
        break;
    }

    switch (group)
    {
        case AG_LOW:
        {
            groupCount = LAG_ANIMATION_COUNT;
            result = uo_get_group_offset(group, graphic);
        }
        break;

        case AG_HIGH:
        {
            groupCount = HAG_ANIMATION_COUNT;
            result = uo_get_group_offset(group, graphic);
        }
        break;

        case AG_PEOPLE:
        {
            groupCount = PAG_ANIMATION_COUNT;
            result = uo_get_group_offset(group, graphic);
        }
        break;

        case AG_NONE:
        case AG_INVALID:
        default:
            break;
    }

    return result;
}

static void load_mobtype()
{
    static const astr_t typeNames[] = { "monster", "sea_monster", "animal", "human", "equipment" };
    static_assert(countof(typeNames) == AGT_UNKNOWN, "update group types");
    DBG_ANIM_DECLARE(bool dbgEntryFound = false);
    DBG_ANIM("mobtype.txt");

    TextFileParser mobtypesParser(UOFilePath("mobtypes.txt"), " \t", "#;//", "");
    while (!mobtypesParser.IsEOF())
    {
        const auto strings = mobtypesParser.ReadTokens();
        if (strings.size() >= 3)
        {
            const uint16_t index = str_to_int(strings[0]);
            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                DBG_ANIM("\tdetected invalid entry in mobtypes.txt: %d", index);
                continue;
            }

            const auto entryType = str_lower(strings[1]);
            char *endP = nullptr;
            const auto number = strtoul(("0x" + strings[2]).c_str(), &endP, 16);
            DBG_ANIM_ID(
                index, "\tentry type: %s with: 0x%04x | AF_FOUND", entryType.c_str(), (int)number);
            for (int i = 0; i < countof(typeNames); i++)
            {
                if (entryType == typeNames[i])
                {
                    DBG_ANIM_BLOCK(dbgEntryFound = true);
                    g_Index.m_Anim[index].Type = ANIMATION_GROUPS_TYPE(i);
                    g_Index.m_Anim[index].Flags = ANIMATION_FLAGS(AF_FOUND | number);
                    break;
                }
            }
        }
    }
    DBG_ANIM("\twas anim entry found in mobtypes.txt? %s", dbgEntryFound ? "yes" : "no");
}

static void load_animations(CFileManager *mgr)
{
    DBG_ANIM("load_animations");
    size_t maxAddress = mgr->m_AddressIdx[0] + mgr->m_SizeIdx[0];
    for (int i = 0; i < MAX_ANIMATIONS_DATA_INDEX_COUNT; i++)
    {
        IndexAnimation &index = g_Index.m_Anim[i];
        if (index.Type == AGT_UNKNOWN)
        {
            index.Type = uo_type_by_graphic(i);
        }
        index.Graphic = (int)i;
        index.CorpseGraphic = i;
        int count = 0;
        const auto dataOffset = uo_get_anim_offset(i, index.Flags, index.Type, count);
        DBG_ANIM_ID(
            i,
            "\tload mul animation %d type: %d, group count: %d",
            i,
            index.Type,
            uo_group_count_by_type[index.Type]);
        if (dataOffset >= mgr->m_SizeIdx[0])
            continue;

        DBG_ANIM_ID(i, "\t\toffset: %" PRId64, dataOffset);
        bool isValid = false;
        size_t address = mgr->m_AddressIdx[0] + dataOffset;
        DBG_ANIM_ID(i, "\t\taddress: %" PRId64, address);
        int block = 0;
        for (int j = 0; j < count; j++)
        {
            assert(j < MAX_ANIMATION_GROUPS_COUNT);
            auto &group = index.Groups[j];
            DBG_ANIM_ID(i, "\t\tgroup: %d", j);
            for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
            {
                auto &direction = group.Direction[d];
                const auto *file = (AnimIdxBlock *)(address + block * sizeof(AnimIdxBlock));
                block++;
                if ((size_t)file >= maxAddress)
                    break;
                if (file->Size > 0 && file->Position != -1 && file->Size != -1)
                {
                    direction.BaseAddress = file->Position;
                    direction.BaseSize = file->Size;
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                    isValid = true;
                    DBG_ANIM_ID(
                        i, "\t\t\tdir: %d address: %d size: %d", d, file->Position, file->Size);
                }
            }
        }
        index.IsValidMUL = isValid;
    }
}

static void load_verdata(CFileManager *mgr)
{
    DBG_ANIM("load_verdata");
    auto verdata = (uint32_t *)g_FileManager.m_VerdataMul.Start;
    if (verdata == nullptr)
        return;

    int dataCount = *verdata;
    for (int j = 0; j < dataCount; j++)
    {
        const auto *vh = (VERDATA_HEADER *)((size_t)verdata + 4 + (j * sizeof(VERDATA_HEADER)));
        if (vh->FileID == PatchAnim)
        {
            ANIMATION_GROUPS_TYPE groupType = AGT_HUMAN;
            uint32_t graphic = vh->BlockID;
            uint16_t id = 0xFFFF;
            uint32_t group = 0;
            uint32_t dir = 0;
            uint32_t offset = 0;
            int count = 0;

            TRACE(Data, "vh->ID = 0x%02X vh->BlockID = 0x%08X", vh->FileID, graphic);
            DBG_ANIM_ID(graphic, "\tpatching anim using verdata");

            // FIXME: uo_get_group_offset?
            if (graphic < 35000)
            {
                if (graphic < 22000) //monsters
                {
                    count = HAG_ANIMATION_COUNT;
                    groupType = AGT_MONSTER;
                    id = graphic / 110;
                    offset = graphic - (id * 110);
                }
                else //animals
                {
                    count = LAG_ANIMATION_COUNT;
                    groupType = AGT_ANIMAL;
                    id = (graphic - 22000) / 65;
                    offset = graphic - ((id * 65) + 22000);
                    id += 200;
                }
            }
            else //humans
            {
                groupType = AGT_HUMAN;
                count = PAG_ANIMATION_COUNT;
                id = (graphic - 35000) / 175;
                offset = graphic - ((id * 175) + 35000);
                id += 400;
            }

            group = offset / MAX_MOBILE_DIRECTIONS;
            dir = offset % MAX_MOBILE_DIRECTIONS;
            if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                Warning(Data, "Invalid animation patch 0x%04X (0x%08X)", id, graphic);
                continue;
            }
            if (group >= (uint32_t)count)
            {
                Warning(
                    Data,
                    "Invalid group index: %i in animation patch 0x%04X (0x%08X)",
                    group,
                    id,
                    graphic);
                continue;
            }

            DBG_ANIM_ID(
                id,
                "\tverdata animation patch: 0x%04X (0x%08X) group: %d dir: %d",
                id,
                graphic,
                group,
                dir);
            auto &index = g_Index.m_Anim[id];
            assert(group < MAX_ANIMATION_GROUPS_COUNT);
            auto &direction = index.Groups[group].Direction[dir];
            direction.IsVerdata = true;
            direction.BaseAddress = (size_t)g_FileManager.m_VerdataMul.Start + vh->Position;
            direction.BaseSize = vh->Size;
            direction.Address = direction.BaseAddress;
            direction.Size = direction.BaseSize;
            DBG_ANIM_ID(id, "\t\taddress: %" PRId64, direction.Address);
            DBG_ANIM_ID(id, "\t\tsize: %d", direction.Size);
            index.Graphic = id;
            index.Type = groupType;
            index.IsValidMUL = true;
        }
    }
}

static void load_animdef(CFileManager *mgr)
{
    TextFileParser animParser[] = {
        TextFileParser(UOFilePath("Anim1.def"), " \t", "#;//", "{}"), // AG_LOW
        TextFileParser(UOFilePath("Anim2.def"), " \t", "#;//", "{}"), // AG_PEOPLE
    };
    for (int i = 0; i < countof(animParser); i++)
    {
        while (!animParser[i].IsEOF())
        {
            const auto strings = animParser[i].ReadTokens();
            if (strings.size() < 2)
            {
                continue;
            }
            const auto group = uint16_t(str_to_int(strings[0]));
            if (group == 0xffff)
            {
                continue;
            }
            const auto replaceGroup = uint8_t(str_to_int(strings[1]));
            mgr->m_GroupReplaces[i].push_back({ group, replaceGroup });
        }
    }
}

static EQUIP_CONV_BODY_MAP s_EquipConv;

const EQUIP_CONV_BODY_MAP::iterator uo_get_equip_body_conv(uint16_t bodyGraphic)
{
    return s_EquipConv.find(bodyGraphic);
}

const CEquipConvData *
uo_get_equipconv(const EQUIP_CONV_BODY_MAP::iterator bodyMapIter, uint16_t graphic)
{
    if (bodyMapIter != s_EquipConv.end())
    {
        auto dataIter = bodyMapIter->second.find(graphic);
        if (dataIter != bodyMapIter->second.end())
        {
            return &dataIter->second;
        }
    }
    return nullptr;
}

const CEquipConvData *uo_get_equipconv(uint16_t bodyGraphic, uint16_t graphic)
{
    const auto bodyMapIter = uo_get_equip_body_conv(bodyGraphic);
    return uo_get_equipconv(bodyMapIter, graphic);
}

static void load_equipconv(CFileManager *mgr)
{
    TextFileParser equipConvParser(UOFilePath("Equipconv.def"), " \t", "#;//", "");
    while (!equipConvParser.IsEOF())
    {
        const auto strings = equipConvParser.ReadTokens();
        if (strings.size() >= 5)
        {
            const auto body = (uint16_t)str_to_int(strings[0]);
            if (body >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            const auto graphic = (uint16_t)str_to_int(strings[1]);
            if (graphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            auto newGraphic = (uint16_t)str_to_int(strings[2]);
            if (newGraphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            const auto gumpId = (uint32_t)str_to_int(strings[3]);
            if (gumpId >= MAX_GUMP_DATA_INDEX_COUNT)
            {
                continue;
            }

            auto gump = checked_cast<uint16_t>(gumpId);
            if (gump == 0)
            {
                gump = graphic; // +50000;
            }
            else if (gump == 0xFFFF)
            {
                gump = newGraphic; // +50000;
            }

            auto color = (uint16_t)str_to_int(strings[4]);
            auto bodyMapIter = s_EquipConv.find(body);
            if (bodyMapIter == s_EquipConv.end())
            {
                s_EquipConv.insert({ body, {} });
                bodyMapIter = s_EquipConv.find(body);
                assert(bodyMapIter != s_EquipConv.end());
            }
            bodyMapIter->second.insert({ graphic, { newGraphic, gump, color } });
        }
    }
}

static void load_bodyconv(CFileManager *mgr)
{
    DBG_ANIM("load_bodyconv");
    TextFileParser bodyconvParser(UOFilePath("Bodyconv.def"), " \t", "#;//", "");
    while (!bodyconvParser.IsEOF())
    {
        const auto strings = bodyconvParser.ReadTokens();
        if (strings.size() >= 2)
        {
            const uint16_t index = str_to_int(strings[0]);
            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            int anim[4] = { str_to_int(strings[1]), -1, -1, -1 };
            if (strings.size() >= 3)
            {
                anim[1] = str_to_int(strings[2]);
                if (strings.size() >= 4)
                {
                    anim[2] = str_to_int(strings[3]);
                    if (strings.size() >= 5)
                    {
                        anim[3] = str_to_int(strings[4]);
                    }
                }
            }

            DBG_ANIM_ID(index, "\tbodyconv {%d, %d, %d, %d}", anim[0], anim[1], anim[2], anim[3]);
            int animFile = 1;
            uint16_t realAnimId = -1;
            char mountedHeightOffset = 0;
            if (anim[0] != -1 && mgr->m_AddressIdx[2] != 0 && g_FileManager.IsMulFileOpen(2))
            {
                animFile = 2;
                realAnimId = anim[0];
                if (index == 192 || index == 793) // 0x00C0 || 0x0319
                {
                    mountedHeightOffset = -9;
                }
            }
            else if (anim[1] != -1 && mgr->m_AddressIdx[3] != 0 && g_FileManager.IsMulFileOpen(3))
            {
                animFile = 3;
                realAnimId = anim[1];
                if (index == 1401) // 0x0579
                {
                    mountedHeightOffset = 9;
                }
            }
            else if (anim[2] != -1 && mgr->m_AddressIdx[4] != 0 && g_FileManager.IsMulFileOpen(4))
            {
                animFile = 4;
                realAnimId = anim[2];
            }
            else if (anim[3] != -1 && mgr->m_AddressIdx[5] != 0 && g_FileManager.IsMulFileOpen(5))
            {
                animFile = 5;
                realAnimId = anim[3];
                switch (index)
                {
                    case 192: // 0x00C0
                    case 277: // 0x0115
                        mountedHeightOffset = 0;
                        break;
                    case 1069: // 0x042D
                        mountedHeightOffset = 3;
                        break;
                    default:
                        mountedHeightOffset = -9;
                        break;
                }
            }
            DBG_ANIM_ID(index, "\treal anim is %d is in animFile %d", realAnimId, animFile);
            if (animFile > 1 && realAnimId != 0xFFFF)
            {
                auto &dataIndex = g_Index.m_Anim[index];
                const auto realType =
                    (s_ClientVersion < CV_500A) ? uo_type_by_graphic(realAnimId) : dataIndex.Type;
                int count = 0;
                auto dataOffset = uo_get_anim_offset(realAnimId, dataIndex.Flags, realType, count);
                if (dataOffset < mgr->m_SizeIdx[animFile])
                {
                    dataIndex.Type = realType;
                    assert(realAnimId < APF_GROUP);
                    dataIndex.GraphicConversion = realAnimId | APF_BODYCONV;
                    dataIndex.FileIndex = animFile;
                    DBG_ANIM_ID(index, "\t\toffset: %" PRId64, dataOffset);
                    DBG_ANIM_ID(index, "\t\ttype: %d", dataIndex.Type);
                    DBG_ANIM_ID(
                        index, "\t\tgraphic conversion: 0x%04X", dataIndex.GraphicConversion);
                    if (dataIndex.MountedHeightOffset == 0)
                    {
                        dataIndex.MountedHeightOffset = mountedHeightOffset;
                    }

                    dataOffset += mgr->m_AddressIdx[animFile];
                    const auto maxAddress = mgr->m_AddressIdx[animFile] + mgr->m_SizeIdx[animFile];
                    int block = 0;
                    for (int j = 0; j < count; j++)
                    {
                        assert(j < MAX_ANIMATION_GROUPS_COUNT);
                        auto &group = dataIndex.Groups[j];
                        for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
                        {
                            const auto *aidx =
                                (AnimIdxBlock *)(dataOffset + block * sizeof(AnimIdxBlock));
                            block++;
                            if ((size_t)aidx >= maxAddress)
                            {
                                continue;
                            }
                            DBG_ANIM_ID(
                                index,
                                "\t\tpatch group: %d dir: %d offset: %d size: %d",
                                j,
                                d,
                                aidx->Position,
                                aidx->Size);
                            if (aidx->Size != 0 && aidx->Position != -1 && aidx->Size != -1)
                            {
                                dataIndex.GraphicConversion |= APF_GROUP;
                                auto &direction = group.Direction[d];
                                direction.PatchedAddress = aidx->Position;
                                direction.PatchedSize = aidx->Size;
                                direction.FileIndex = animFile;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void load_body_corpse_def(fs_path file, bool alive)
{
    TextFileParser newParser({}, " \t,{}", "#;//", "");
    TextFileParser parser(file, " \t", "#;//", "{}");
    std::unordered_set<uint16_t> used = {};
    while (!parser.IsEOF())
    {
        const auto strings = parser.ReadTokens();
        if (strings.size() >= 3)
        {
            const auto graphic = checked_cast<uint16_t>(str_to_int(strings[0]));
            assert(graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);

            const auto replaces = newParser.GetTokens(strings[1]);
            if (replaces.empty())
            {
                continue;
            }

            if (used.find(graphic) != used.end())
            {
                continue;
            }

            // FIXME: there is something missing here: { anim1, anim2, anim3, anim4 }
            auto newGraphic = checked_cast<uint16_t>(str_to_int(replaces[0]));
            if (replaces.size() >= 3)
            {
                newGraphic = checked_cast<uint16_t>(str_to_int(replaces[2]));
            }
            assert(newGraphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);

            DBG_ANIM_ID(graphic, "\tnew graphic: 0x%04X", newGraphic);
            auto &dataIndex = g_Index.m_Anim[graphic];
            const auto type = dataIndex.Type;
            auto &newDataIndex = g_Index.m_Anim[newGraphic];
            const auto count = (int)uo_group_count_by_type[type];
            for (int grpId = 0; grpId < count; grpId++)
            {
                const bool die = uo_anim_group_is_die(type, grpId);
                const bool skip = !(alive ^ die);
                if (skip)
                    continue;

                DBG_ANIM_ID(graphic, "\tgroup: %d", grpId);
                assert(grpId < MAX_ANIMATION_GROUPS_COUNT);
                auto &group = dataIndex.Groups[grpId];
                const auto &newGroup = newDataIndex.Groups[grpId];
                for (int dirId = 0; dirId < MAX_MOBILE_DIRECTIONS; dirId++)
                {
                    auto &direction = group.Direction[dirId];
                    const auto &newDirection = newGroup.Direction[dirId];
                    direction.BaseAddress = newDirection.BaseAddress;
                    direction.BaseSize = newDirection.BaseSize;
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                    if (direction.PatchedAddress == 0)
                    {
                        direction.PatchedAddress = newDirection.PatchedAddress;
                        direction.PatchedSize = newDirection.PatchedSize;
                        direction.FileIndex = newDirection.FileIndex;
                        DBG_ANIM_ID(
                            graphic,
                            "\t\tpatch address: %" PRId64 ", size: %d file: %d",
                            direction.PatchedAddress,
                            direction.PatchedSize,
                            direction.FileIndex);
                    }
                    if (direction.BaseAddress == 0)
                    {
                        direction.BaseAddress = direction.PatchedAddress;
                        direction.BaseSize = direction.PatchedSize;
                        direction.Address = direction.BaseAddress;
                        direction.Size = direction.BaseSize;
                    }
                    DBG_ANIM_ID(
                        graphic,
                        "\t\tdir: %d, address: %" PRId64 ", size: %d file: %d",
                        dirId,
                        direction.Address,
                        direction.Size,
                        direction.FileIndex);
                }
            }
            // do not overwrite type or flags, this is just graphics replacement
            //dataIndex.Type = newDataIndex.Type;
            //dataIndex.Flags = newDataIndex.Flags;
            dataIndex.Graphic = newGraphic;
            dataIndex.Color = checked_cast<uint16_t>(str_to_int(strings[2]));
            dataIndex.IsValidMUL = true;
            DBG_ANIM_ID(
                graphic,
                "\tmul anim type: %d, flags: 0x%08x, color: 0x%04x",
                dataIndex.Type,
                dataIndex.Flags,
                dataIndex.Color);
            used.insert(graphic);
        }
    }
}

static void load_bodydef()
{
    const bool alive = true;
    DBG_ANIM("load_bodydef");
    load_body_corpse_def(UOFilePath("Body.def"), alive);
}

static void load_corpsedef()
{
    const bool alive = false;
    DBG_ANIM("load_corpsedef");
    load_body_corpse_def(UOFilePath("Corpse.def"), alive);
}

void static load_data_patches(CFileManager *mgr)
{
    if (s_ClientVersion >= CV_500A)
    {
        load_mobtype();
    }
    load_animations(mgr);
    load_verdata(mgr); // <-- TODO: REFACTOR
    load_animdef(mgr);
    if (s_ClientVersion < CV_305D)
    {
        return;
    }
    load_equipconv(mgr);
    load_bodyconv(mgr);
    load_bodydef();
    load_corpsedef();
}

void CFileManager::LoadAnimations()
{
    memset(m_AddressIdx, 0, sizeof(m_AddressIdx));
    memset(m_SizeIdx, 0, sizeof(m_SizeIdx));
    for (int i = 0; i < countof(m_AnimIdx); i++)
    {
        m_AddressIdx[i] = (size_t)m_AnimIdx[i].Start;
        m_SizeIdx[i] = (size_t)m_AnimIdx[i].Size;
    }
    load_data_patches(this);
    uo_animation_dump("anim-pre.csv");
}

void CFileManager::LoadSkills()
{
    if (m_SkillsIdx.Size == 0 || m_SkillsMul.Size == 0)
        return;

    m_Skills.clear();
    auto &idx = m_SkillsIdx;
    auto &mul = m_SkillsMul;
    while (!idx.IsEOF())
    {
        const auto idxBlock = (SkillIdxBlock *)idx.Ptr;
        idx.Move(sizeof(SkillIdxBlock));
        if (idxBlock->Size != 0 && idxBlock->Position != ~0 && idxBlock->Size != ~0)
        {
            mul.Ptr = mul.Start + idxBlock->Position;
            const bool haveButton = (mul.ReadUInt8() != 0);
            SkillData data = { mul.ReadString(idxBlock->Size - 1), haveButton };
            m_Skills.emplace_back(data);
        }
    }
}

void CFileManager::LoadHues()
{
    const auto addr = (intptr_t)m_HuesMul.Start;
    const auto size = m_HuesMul.Size;
    if (addr == 0 || size == 0)
        return;

    const size_t count = size / sizeof(HUES_GROUP);
    m_Hues.resize(count * 8);
    memcpy(&m_Hues[0], (void *)addr, count * sizeof(HUES_GROUP));
}

void CFileManager::CreateBlocksTable()
{
    for (int i = 0; i < MAX_MAPS_COUNT; i++)
    {
        g_MapBlockSize[i].Width = g_MapSize[i].Width / 8;
        g_MapBlockSize[i].Height = g_MapSize[i].Height / 8;
    }

    for (int map = 0; map < MAX_MAPS_COUNT; map++)
    {
        const auto &size = g_MapBlockSize[map];
        CreateBlockTable((int)map, size.Width, size.Height);
    }
}

void CFileManager::CreateBlockTable(int map, int width, int height)
{
    MAP_INDEX_LIST &list = m_BlockData[map];
    const int maxBlockCount = width * height;
    if (maxBlockCount < 1)
    {
        return;
    }

    list.resize(maxBlockCount);

    size_t mapAddress = (size_t)g_FileManager.m_MapMul[map].Start;
    size_t endMapAddress = mapAddress + g_FileManager.m_MapMul[map].Size;

    CUopMappedFile &uopFile = g_FileManager.m_MapUOP[map];
    const bool isUop = (uopFile.Start != nullptr);
    if (isUop)
    {
        mapAddress = (size_t)uopFile.Start;
        endMapAddress = mapAddress + uopFile.Size;
    }

    size_t staticIdxAddress = (size_t)g_FileManager.m_StaticIdx[map].Start;
    size_t endStaticIdxAddress = staticIdxAddress + g_FileManager.m_StaticIdx[map].Size;
    size_t staticAddress = (size_t)g_FileManager.m_StaticMul[map].Start;
    size_t endStaticAddress = staticAddress + g_FileManager.m_StaticMul[map].Size;
    if ((mapAddress == 0u) || (staticIdxAddress == 0u) || (staticAddress == 0u))
    {
        return;
    }

    int fileNumber = -1;
    size_t uopOffset = 0;
    for (int blockIdx = 0; blockIdx < maxBlockCount; blockIdx++)
    {
        CIndexMap &index = list[blockIdx];
        size_t realMapAddress = 0;
        size_t realStaticAddress = 0;
        int realStaticCount = 0;
        int blockNumber = (int)blockIdx;
        if (isUop)
        {
            blockNumber &= 4095;
            int shifted = (int)blockIdx >> 12;
            if (fileNumber != shifted)
            {
                fileNumber = shifted;
                char mapFilePath[200];
                snprintf(
                    mapFilePath,
                    sizeof(mapFilePath),
                    "build/map%dlegacymul/%08d.dat",
                    map,
                    shifted);
                auto file = uopFile.GetAsset(mapFilePath);
                if (file != nullptr)
                {
                    uopOffset = size_t(file->Offset + file->MetadataSize);
                }
                else
                {
                    Warning(Data, "couldn't find asset %s", mapFilePath);
                }
            }
        }

        const size_t address = mapAddress + uopOffset + (blockNumber * sizeof(MAP_BLOCK));
        if (address < endMapAddress)
        {
            realMapAddress = address;
        }

        const auto *sidx = (StaIdxBlock *)(staticIdxAddress + blockIdx * sizeof(StaIdxBlock));
        if ((size_t)sidx < endStaticIdxAddress && sidx->Size > 0 && sidx->Position != 0xFFFFFFFF)
        {
            const size_t address2 = staticAddress + sidx->Position;
            if (address2 < endStaticAddress)
            {
                realStaticAddress = address2;
                realStaticCount = sidx->Size / sizeof(STATICS_BLOCK);
                if (realStaticCount > 1024)
                {
                    realStaticCount = 1024;
                }
            }
        }
        index.OriginalMapAddress = realMapAddress;
        index.OriginalStaticAddress = realStaticAddress;
        index.OriginalStaticCount = realStaticCount;
        index.MapAddress = realMapAddress;
        index.StaticAddress = realStaticAddress;
        index.StaticCount = realStaticCount;
    }
}

void CFileManager::PatchFiles()
{
    auto &file = g_FileManager.m_VerdataMul;
    if (!s_UseVerdata || file.Size == 0)
    {
        return;
    }

    const auto dataCount = *(int32_t *)file.Start;
    const auto vAddr = (size_t)file.Start;
    for (int i = 0; i < dataCount; i++)
    {
        VERDATA_HEADER *vh = (VERDATA_HEADER *)(vAddr + 4 + (i * sizeof(VERDATA_HEADER)));
        if (vh->FileID == PatchMap0)
        {
            const auto &size = g_MapBlockSize[0];
            const int maxBlockCount = size.Width * size.Height;
            if (maxBlockCount < 1)
                continue;

            const auto block = vh->BlockID;
            const auto address = vAddr + vh->Position;
            auto &data = m_BlockData[0];
            data[block].OriginalMapAddress = address;
            data[block].MapAddress = address;
        }
        else if (vh->FileID == PatchArt)
        {
            if (vh->BlockID >= MAX_LAND_DATA_INDEX_COUNT) //Run
            {
                uint16_t ID = (uint16_t)vh->BlockID - MAX_LAND_DATA_INDEX_COUNT;
                g_Index.m_Static[ID].Address = vAddr + vh->Position;
                g_Index.m_Static[ID].DataSize = vh->Size;
            }
            else //Raw
            {
                g_Index.m_Land[vh->BlockID].Address = vAddr + vh->Position;
                g_Index.m_Land[vh->BlockID].DataSize = vh->Size;
            }
        }
        else if (vh->FileID == PatchGumpArt)
        {
            g_Index.m_Gump[vh->BlockID].Address = vAddr + vh->Position;
            g_Index.m_Gump[vh->BlockID].DataSize = vh->Size;
            g_Index.m_Gump[vh->BlockID].Width = vh->GumpData >> 16;
            g_Index.m_Gump[vh->BlockID].Height = vh->GumpData & 0xFFFF;
        }
        else if (vh->FileID == PatchMulti && (int)vh->BlockID < g_Index.m_MultiIndexCount)
        {
            g_Index.m_Multi[vh->BlockID].Address = vAddr + vh->Position;
            g_Index.m_Multi[vh->BlockID].DataSize = vh->Size;
            g_Index.m_Multi[vh->BlockID].Count = uint16_t(vh->Size / sizeof(MultiIdxBlock));
        }
        else if (vh->FileID == PatchSkills && vh->BlockID < m_Skills.size())
        {
            const auto skillIdx = vh->BlockID;
            auto &skill = m_Skills[skillIdx];
            CDataReader reader((uint8_t *)vAddr + vh->Position, vh->Size);
            skill.Iteractive = reader.ReadUInt8() != 0;
            skill.Name = reader.ReadString(vh->Size - 1);
        }
        else if (vh->FileID == PatchTileData)
        {
            file.ResetPtr();
            file.Move(vh->Position);
            if (vh->Size == 836)
            {
                const int offset = vh->BlockID * 32;
                if (offset + 32 > (int)g_Data.m_Land.size())
                {
                    continue;
                }
                file.ReadUInt32LE();
                for (int j = 0; j < 32; j++)
                {
                    MulLandTile2 &tile = g_Data.m_Land[offset + j];
                    if (s_ClientVersion < CV_7090)
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
            else if (vh->Size == 1188)
            {
                int offset = (vh->BlockID - 0x0200) * 32;
                if (offset + 32 > (int)g_Data.m_Static.size())
                {
                    continue;
                }
                file.ReadUInt32LE();
                for (int j = 0; j < 32; j++)
                {
                    MulStaticTile2 &tile = g_Data.m_Static[offset + j];
                    if (s_ClientVersion < CV_7090)
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
        else if (vh->FileID == PatchHues)
        {
            const auto hueIdx = vh->BlockID;
            if (hueIdx < m_Hues.size())
            {
                const auto group = (VERDATA_HUES_GROUP *)(vAddr + vh->Position);
                m_Hues[hueIdx].Header = group->Header;
                for (int entry = 0; entry < 8; entry++)
                {
                    memcpy(
                        &m_Hues[hueIdx].Entries[entry].ColorTable[0],
                        &group->Entries[entry].ColorTable[0],
                        sizeof(HUES_BLOCK::ColorTable));
                }
            }
        }
        else if (vh->FileID != PatchAnimIdx && vh->FileID != PatchAnim)
        {
            Warning(
                Client, "Unused verdata block (fileID) = %i (BlockID+ %i", vh->FileID, vh->BlockID);
        }
    }
}

// FIXME: there is some issue here - looks like this does not work correctly
// need further investigation
void CFileManager::IndexReplaces()
{
    if (s_ClientVersion < CV_305D)
    { //CV_204C
        return;
    }

    TextFileParser newDataParser({}, " \t,{}", "#;//", "");
    TextFileParser artParser(UOFilePath("art.def"), " \t", "#;//", "{}");
    TextFileParser textureParser(UOFilePath("TexTerr.def"), " \t", "#;//", "{}");
    TextFileParser gumpParser(UOFilePath("gump.def"), " \t", "#;//", "{}");
    TextFileParser multiParser(UOFilePath("Multi.def"), " \t", "#;//", "{}");
    TextFileParser soundParser(UOFilePath("Sound.def"), " \t", "#;//", "{}");
    TextFileParser mp3Parser(UOFilePath("Music/Digital/Config.txt"), " ,", "#;", "");

    Info(Data, "replacing arts");
    while (!artParser.IsEOF())
    {
        auto strings = artParser.ReadTokens();
        if (strings.size() >= 3)
        {
            int index = str_to_int(strings[0]);
            if (index < 0 || index >= MAX_LAND_DATA_INDEX_COUNT + (int)g_Data.m_Static.size())
            {
                continue;
            }

            auto newArt = newDataParser.GetTokens(strings[1]);
            int size = (int)newArt.size();
            for (int i = 0; i < size; i++)
            {
                int checkIndex = str_to_int(newArt[i]);
                if (checkIndex < 0 ||
                    checkIndex >= MAX_LAND_DATA_INDEX_COUNT + (int)g_Data.m_Static.size())
                {
                    continue;
                }

                if (index < MAX_LAND_DATA_INDEX_COUNT && checkIndex < MAX_LAND_DATA_INDEX_COUNT &&
                    g_Index.m_Land[checkIndex].Address != 0 && g_Index.m_Land[index].Address == 0)
                {
                    g_Index.m_Land[index] = g_Index.m_Land[checkIndex];
                    assert(g_Index.m_Land[index].UserData == nullptr);
                    g_Index.m_Land[index].UserData = nullptr;
                    g_Index.m_Land[index].Color = str_to_int(strings[2]);
                    break;
                }
                if (index >= MAX_LAND_DATA_INDEX_COUNT && checkIndex >= MAX_LAND_DATA_INDEX_COUNT)
                {
                    checkIndex -= MAX_LAND_DATA_INDEX_COUNT;
                    checkIndex &= 0x3FFF;
                    index -= MAX_LAND_DATA_INDEX_COUNT;
                    if (g_Index.m_Static[index].Address == 0 &&
                        g_Index.m_Static[checkIndex].Address != 0)
                    {
                        g_Index.m_Static[index] = g_Index.m_Static[checkIndex];
                        assert(g_Index.m_Static[index].UserData == nullptr);
                        g_Index.m_Static[index].UserData = nullptr;
                        g_Index.m_Static[index].Color = str_to_int(strings[2]);
                        break;
                    }
                }
            }
        }
    }

    Info(Data, "replacing textures");
    while (!textureParser.IsEOF())
    {
        auto strings = textureParser.ReadTokens();
        if (strings.size() >= 3)
        {
            int index = str_to_int(strings[0]);
            if (index < 0 || index >= MAX_LAND_TEXTURES_DATA_INDEX_COUNT ||
                g_Index.m_Texture[index].Address != 0)
            {
                continue;
            }

            auto newTexture = newDataParser.GetTokens(strings[1]);
            const int size = (int)newTexture.size();
            for (int i = 0; i < size; i++)
            {
                int checkIndex = str_to_int(newTexture[i]);
                if (checkIndex < 0)
                {
                    continue;
                }

                if (index < TexturesDataCount && checkIndex < TexturesDataCount &&
                    g_Index.m_Texture[checkIndex].Address != 0)
                {
                    g_Index.m_Texture[index] = g_Index.m_Texture[checkIndex];
                    assert(g_Index.m_Texture[index].UserData == nullptr);
                    g_Index.m_Texture[index].UserData = nullptr;
                    g_Index.m_Texture[index].Color = str_to_int(strings[2]);
                    break;
                }
            }
        }
    }

    Info(Data, "replacing gumps");
    while (!gumpParser.IsEOF())
    {
        auto strings = gumpParser.ReadTokens();
        if (strings.size() >= 3)
        {
            int index = str_to_int(strings[0]);
            if (index < 0 || index >= MAX_GUMP_DATA_INDEX_COUNT ||
                g_Index.m_Gump[index].Address != 0)
            {
                continue;
            }

            auto newGump = newDataParser.GetTokens(strings[1]);
            const int size = (int)newGump.size();
            for (int i = 0; i < size; i++)
            {
                const int checkIndex = str_to_int(newGump[i]);
                if (checkIndex < 0 || checkIndex >= MAX_GUMP_DATA_INDEX_COUNT ||
                    g_Index.m_Gump[checkIndex].Address == 0)
                {
                    continue;
                }
                g_Index.m_Gump[index] = g_Index.m_Gump[checkIndex];
                assert(g_Index.m_Gump[index].UserData == nullptr);
                g_Index.m_Gump[index].UserData = nullptr;
                g_Index.m_Gump[index].Color = str_to_int(strings[2]);
                break;
            }
        }
    }

    Info(Data, "replacing multi");
    while (!multiParser.IsEOF())
    {
        auto strings = multiParser.ReadTokens();
        if (strings.size() >= 3)
        {
            int index = str_to_int(strings[0]);
            if (index < 0 || index >= g_Index.m_MultiIndexCount ||
                g_Index.m_Multi[index].Address != 0)
            {
                continue;
            }

            auto newMulti = newDataParser.GetTokens(strings[1]);
            const int size = (int)newMulti.size();
            for (int i = 0; i < size; i++)
            {
                const int checkIndex = str_to_int(newMulti[i]);
                if (checkIndex < 0 || checkIndex >= g_Index.m_MultiIndexCount ||
                    g_Index.m_Multi[checkIndex].Address == 0)
                {
                    continue;
                }
                g_Index.m_Multi[index] = g_Index.m_Multi[checkIndex];
                break;
            }
        }
    }

    Info(Data, "replacing sounds");
    while (!soundParser.IsEOF())
    {
        auto strings = soundParser.ReadTokens();
        if (strings.size() >= 2)
        {
            int index = str_to_int(strings[0]);
            if (index < 0 || index >= MAX_SOUND_DATA_INDEX_COUNT ||
                g_Index.m_Sound[index].Address != 0)
            {
                continue;
            }

            auto newSound = newDataParser.GetTokens(strings[1]);
            const int size = (int)newSound.size();
            for (int i = 0; i < size; i++)
            {
                const int checkIndex = str_to_int(newSound[i]);
                if (checkIndex < -1 || checkIndex >= MAX_SOUND_DATA_INDEX_COUNT)
                {
                    continue;
                }

                CIndexSound &in = g_Index.m_Sound[index];
                if (checkIndex == -1)
                {
                    in.Address = 0;
                    in.DataSize = 0;
                    in.Delay = 0;
                    in.LastAccessTime = 0;
                }
                else
                {
                    CIndexSound &out = g_Index.m_Sound[checkIndex];
                    if (out.Address == 0)
                    {
                        continue;
                    }
                    in.Address = out.Address;
                    in.DataSize = out.DataSize;
                    in.Delay = out.Delay;
                    in.LastAccessTime = out.LastAccessTime;
                }

                free(in.m_WaveFile); // FIXME!!!
                in.m_WaveFile = nullptr;
                in.UserData = nullptr;
                break;
            }
        }
    }

    Info(Data, "loading music config");
    while (!mp3Parser.IsEOF())
    {
        auto strings = mp3Parser.ReadTokens();
        const size_t size = strings.size();
        if (size > 0)
        {
            const uint32_t index = str_to_int(strings[0]);
            CIndexMusic &mp3 = g_Index.m_MP3[index];
            astr_t name = "music/digital/" + strings[1];
            astr_t extension = ".mp3";
            if (name.find(extension) == astr_t::npos)
            {
                name += extension;
            }
            if (size > 1)
            {
                mp3.FilePath = fs_path_str(UOFilePath(name.c_str()));
            }
            if (size > 2)
            {
                mp3.Loop = true;
            }
        }
    }
}

bool uo_animation_exists(uint16_t graphic, uint8_t group)
{
    assert(graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT && group < MAX_ANIMATION_GROUPS_COUNT);
    const auto dir = g_Index.m_Anim[graphic].Groups[group].Direction[0];
    return dir.Size != 0 || dir.IsUOP;
}

void uo_update_animation_tables(uint32_t lockedFlags)
{
    for (int i = 0; i < MAX_ANIMATIONS_DATA_INDEX_COUNT; i++)
    {
        auto &data = g_Index.m_Anim[i];
        for (int g = 0; g < MAX_ANIMATION_GROUPS_COUNT; g++)
        {
            auto &group = data.Groups[g];
            for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
            {
                auto &direction = group.Direction[d];
                bool replace = (direction.FileIndex >= 4);
                if (direction.FileIndex == 2)
                {
                    replace = lockedFlags & LFF_LBR;
                }
                else if (direction.FileIndex == 3)
                {
                    replace = lockedFlags & LFF_AOS;
                }

                if (replace)
                {
                    direction.Address = direction.PatchedAddress;
                    direction.Size = direction.PatchedSize;
                }
                else
                {
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                }

                if (replace && !uo_has_body_conversion(data))
                {
                    data.GraphicConversion &= ~APF_BODYCONV;
                }
            }
        }
    }
    uo_animation_dump("anim-post.csv");
}
