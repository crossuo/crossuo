// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "uodata.h"
#include "mappedfile.h"
#include "mulstruct.h"

#include <stdarg.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <inttypes.h>

#include <external/zlib_amalg.h>
#include <external/cbase64.h>

#include <common/utils.h>
#include <common/fs.h>
#include <common/checksum.h>

#define PALETTE_SIZE (sizeof(uint16_t) * 256)

astr_t g_dumpUopFile;
UOData g_Data;
Index g_Index;
CFileManager g_FileManager;

static bool s_flag = false;
static std::mutex s_protect;
static std::condition_variable s_signal;

static void SetEvent()
{
    std::lock_guard<std::mutex> _(s_protect);
    s_flag = true;
    s_signal.notify_one();
}

static bool WaitEvent()
{
    std::unique_lock<std::mutex> lk(s_protect);
    // prevent spurious wakeups from doing harm
    while (!s_flag)
    {
        s_signal.wait(lk);
    }
    s_flag = false; // waiting resets the flag
    return true;
}

static uint8_t s_AnimGroupCount = PAG_ANIMATION_COUNT;
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

static bool DecompressBlock(const UopFileEntry &block, uint8_t *dst, uint8_t *src)
{
    uLongf cLen = block.CompressedSize;
    uLongf dLen = block.DecompressedSize;
    if (cLen == 0 || block.Flags == 0)
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

static bool UopDecompressBlock(const UopFileEntry &block, uint8_t *dst, int fileId)
{
    assert(fileId >= 0 && fileId <= countof(g_FileManager.m_AnimationFrame));
    uint8_t *src = g_FileManager.m_AnimationFrame[fileId].Start + block.Offset + block.MetadataSize;
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
    if (DecompressBlock(*block, dst.data(), src))
    {
        return dst;
    }
    dst.clear();
    return dst;
}

bool CFileManager::Load()
{
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
            MulLoadFile(m_AnimMul[i], UOFilePath("anim%i.mul", i));
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
        return false;
    }
    if (!m_TextureIdx.Load(UOFilePath("texidx.mul")))
    {
        return false;
    }
    if (!MulLoadFile(m_AnimMul[0], UOFilePath("anim.mul")))
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
        return false;
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
}

void CFileManager::UopReadAnimations()
{
    TRACE(Data, "start uop read jobs");
    // pattern: "build/animationsequence/%08d.bin"
    UopLoadFile(m_AnimationSequence, "AnimationSequence.uop");
    std::thread readThread(&CFileManager::ReadTask, this);
    readThread.detach();
}

static int UopSetAnimationGroups(int start, int end)
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

    TRACE(Data, "running job %d:%d", start, end);
    char filename[200];
    const char *pattern = "build/animationlegacyframe/%06d/%02d.bin";
    int lastGroup = 0;
    for (int animId = start; animId < end; ++animId)
    {
        auto &idx = g_Index.m_Anim[animId];
        for (int grpId = 0; grpId < MAX_ANIMATION_GROUPS_COUNT; ++grpId)
        {
            auto &group = idx.Groups[grpId];
            snprintf(filename, sizeof(filename), pattern, animId, grpId);
            const auto asset = uo_jenkins_hash(filename);
            const auto fileIndex = getAssetOwner(asset);
            if (fileIndex != -1)
            {
                if (grpId > lastGroup)
                {
                    lastGroup = grpId;
                }
                idx.IsUOP = true;
                group.AnimData = g_FileManager.m_AnimationFrame[fileIndex].GetAsset(asset);
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

    return lastGroup;
}

void CFileManager::WaitTasks() const
{
    WaitEvent();
}

void CFileManager::ReadTask()
{
    const static int count = countof(m_AnimationFrame);
    for (int i = 0; i < count; i++)
    {
        char name[64];
        // pattern: "build/animationlegacyframe/%06d/%02d.bin"
        snprintf(name, sizeof(name), "AnimationFrame%d.uop", i + 1);

        auto &file = m_AnimationFrame[i];
        UopLoadFile(file, name);
    }

    int range = MAX_ANIMATIONS_DATA_INDEX_COUNT / count;
    static int lastGroup[count];
    std::vector<std::thread> jobs;
    for (int i = 0; i < count; i++)
    {
        int start = range * i;
        int end = range * (i + 1);
        TRACE(Data, "scheduling job for %d with range from %d to %d", i + 1, start, end);
        auto job = [i, start, end]() { lastGroup[i] = UopSetAnimationGroups(start, end); };
        jobs.push_back(std::thread(job));
    }
    for (auto &job : jobs)
    {
        job.join();
    }
    ProcessAnimSequeceData();

    const int maxGroup = *std::max_element(lastGroup, lastGroup + count);
    if (s_AnimGroupCount < maxGroup)
    {
        s_AnimGroupCount = maxGroup;
    }
    m_AnimationSequence.Unload();
    SetEvent();
}

/*
static void uo_replace_group(uint16_t graphic, uint8_t old, uint8_t newG) // ReplaceUopGroup
{
    assert(graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT && old < MAX_ANIMATION_GROUPS_COUNT);
    assert(newG < MAX_ANIMATION_GROUPS_COUNT);
    g_Index.m_Anim[graphic].ReplaceGroupIndex[old] = newG + 1; // 0 == not replaced
}
*/

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
        const uint32_t animId = ReadInt32LE(); // FIXME: uint16_t
        Move(48);                              // there's nothing there

        // FIXME
        // amount of replaced indices, values seen in files so far: 29, 31, 32, 48, 68
        // human and gargoyle are complicated, skip for now
        // offset 52
        const uint32_t replaces = ReadInt32LE();
        if (replaces == 48 || replaces == 68)
        {
            // 0x30 0xc070396e5a7ec7f4 0x029A (666) // GENDER_MALE RT_GARGOYLE
            // 0x30 0xc6c811fa536c8b62 0x04E5 (1253) // ?
            // 0x44 0x6c1031f63255845a 0x0190 (400) // GENDER_MALE RT_HUMAN
            continue;
        }
        assert(animId < MAX_ANIMATIONS_DATA_INDEX_COUNT);
        auto anim = &g_Index.m_Anim[animId];
        for (uint32_t i = 0; i < replaces; ++i)
        {
            const auto oldGroupIdx = ReadInt32LE();
            const auto frameCount = ReadInt32LE();
            const auto newGroupIdx = ReadInt32LE();
            //auto data = indexAnim->Groups[oldGroupIdx];
            if (frameCount == 0)
            {
                assert(newGroupIdx != oldGroupIdx);
                assert(oldGroupIdx < MAX_ANIMATION_GROUPS_COUNT);
                assert(newGroupIdx < MAX_ANIMATION_GROUPS_COUNT);
                anim->Groups[oldGroupIdx] = anim->Groups[newGroupIdx];
                //uo_replace_group(animId, oldGroupIdx, newGroupIdx);
            }
            /*else
            {
                for( int k = i; k < 5; ++k)
                {
                    //group.Direction[k].FrameCount = frameCount;
                }
            }*/
            Move(60);
        }
        switch (animId)
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
            for (int i = 0; i < header->Count - 1 && i < MAX_STRINGS; ++i)
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
        fprintf(fp, "\tSectionOffset: %016lx\n", file.Header->SectionOffset);
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
            fprintf(fp, "\t\tBlock Header %08X_%016lX:\n", block->Checksum, block->Hash);
            fprintf(fp, "\t\t\tOffset..........: %016lx\n", block->Offset);
            fprintf(fp, "\t\t\tMetadataSize....: %d\n", block->MetadataSize);
            fprintf(fp, "\t\t\tCompressedSize..: %d\n", block->CompressedSize);
            fprintf(fp, "\t\t\tDecompressedSize: %d\n", block->DecompressedSize);
            fprintf(
                fp,
                "\t\t\tHash............: %016lx (%08x%08x)\n",
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

bool CFileManager::MulLoadFile(CMappedFile &file, const fs_path &fileName)
{
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
    data.resize(header.FrameCount);
    for (uint32_t i = 0; i < header.FrameCount; i++)
    {
        data.emplace(data.begin() + i, UopReadAnimationFrame());
    }
    return data;
}

void CFileManager::UopReadAnimationFrameInfo(
    AnimationFrameInfo &result, CTextureAnimationDirection &direction, const UopFileEntry &block)
{
    if (block.Hash == 0)
    {
        return;
    }

    std::vector<uint8_t> scratchBuffer;
    scratchBuffer.reserve(block.DecompressedSize);
    if (!UopDecompressBlock(block, scratchBuffer.data(), direction.FileIndex))
    {
        return;
    }
    // FIXME: CFileManager SHOULD not be a DataReader! In fact, no class need to be. Clean up!
    SetData(scratchBuffer.data(), block.DecompressedSize);
    const auto _header = UopReadAnimationHeader();
    (void)_header;
    auto frame = UopReadAnimationFrame(); // read only first frame to get image dimensions
    Ptr = frame.DataStart + frame.PixelDataOffset + PALETTE_SIZE;
    auto frameInfo = (AnimationFrameInfo *)Ptr;
    result = *frameInfo;
    Move(sizeof(AnimationFrameInfo));
}

bool CFileManager::UopReadAnimationFrames(
    CTextureAnimationDirection &direction,
    const AnimationSelector &anim,
    LoadPixelData16Cb pLoadFunc)
{
    auto &block = *g_Index.m_Anim[anim.Graphic].Groups[anim.Group].AnimData;
    std::vector<uint8_t> scratchBuffer;
    if (block.Hash == 0)
    {
        return false;
    }

    scratchBuffer.reserve(block.DecompressedSize);
    if (!UopDecompressBlock(block, scratchBuffer.data(), direction.FileIndex))
    {
        return false;
    }
    SetData(scratchBuffer.data(), block.DecompressedSize);

    const auto framesData = UopReadAnimationFramesData();
    direction.FrameCount = checked_cast<uint8_t>(framesData.size() / 5);
    int dirFrameStartIdx = direction.FrameCount * anim.Direction;
    if (direction.m_Frames == nullptr)
    {
        direction.m_Frames = new CTextureAnimationFrame[direction.FrameCount];
    }

    const int frameCount = direction.FrameCount;
    for (int i = 0; i < frameCount; i++)
    {
        CTextureAnimationFrame &frame = direction.m_Frames[i];
        if (frame.UserData != nullptr)
        {
            continue;
        }

        UopAnimationFrame frameData = framesData[i + dirFrameStartIdx];
        if (frameData.DataStart == nullptr)
        {
            continue;
        }

        Ptr = frameData.DataStart + frameData.PixelDataOffset;
        auto palette = (uint16_t *)Ptr;
        Move(PALETTE_SIZE);
        LoadAnimationFrame(frame, palette, pLoadFunc);
    }
    return true;
}

void CFileManager::MulReadAnimationFrameInfo(
    AnimationFrameInfo &result,
    CTextureAnimationDirection &direction,
    uint8_t frameIndex,
    bool isCorpse)
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

bool CFileManager::MulReadAnimationFrames(
    CTextureAnimationDirection &direction, LoadPixelData16Cb pLoadFunc)
{
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
    if (direction.m_Frames == nullptr)
    {
        direction.m_Frames = new CTextureAnimationFrame[frameCount];
    }

    for (uint32_t i = 0; i < frameCount; i++)
    {
        CTextureAnimationFrame &frame = direction.m_Frames[i];
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
    CTextureAnimationFrame &frame, uint16_t *palette, LoadPixelData16Cb pLoadFunc)
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
    AnimationFrameInfo &result,
    CTextureAnimationDirection &direction,
    CTextureAnimationGroup &group,
    uint8_t frameIndex,
    bool isCorpse)
{
    if (direction.Address != 0)
    {
        assert(direction.Size != 0 && "please report this back");
        MulReadAnimationFrameInfo(result, direction, frameIndex, isCorpse);
    }
    else if (direction.IsUOP)
    {
        UopReadAnimationFrameInfo(result, direction, *group.AnimData);
    }
}

bool CFileManager::LoadAnimation(const AnimationSelector &anim, LoadPixelData16Cb pLoadFunc)
{
    CTextureAnimationGroup &group = g_Index.m_Anim[anim.Graphic].Groups[anim.Group];
    CTextureAnimationDirection &direction = group.Direction[anim.Direction];
    if (direction.Address != 0)
    {
        assert(direction.Size != 0 && "please report this back");
        return MulReadAnimationFrames(direction, pLoadFunc);
    }
    else if (direction.IsUOP)
    {
        return UopReadAnimationFrames(direction, anim, pLoadFunc);
    }
    return false;
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
