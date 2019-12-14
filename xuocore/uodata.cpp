// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "uolib.h"
#include "uodata.h"
#include "mappedfile.h"

#include <stdarg.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <inttypes.h>

#include <common/utils.h>

#define FS_LOG_DEBUG(...) DEBUG(Data, __VA_ARGS__)
#define FS_LOG_ERROR(...) Error(Data, __VA_ARGS__)
#define FS_IMPLEMENTATION
#include <common/fs.h>

#define CHECKSUM_IMPLEMENTATION
#include <common/checksum.h>

#define MINIZ_IMPLEMENTATION
#include <external/miniz.h>

#define PALETTE_SIZE (sizeof(uint16_t) * 256)

std::string g_dumpUopFile;
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
    m_MapByName[item->Hash] = item;
    m_MapByOffset[item->Offset] = item;
}

bool CUopMappedFile::HasAsset(uint64_t hash) const
{
    return m_MapByName.find(hash) != m_MapByName.end();
}

const UopFileEntry *CUopMappedFile::GetAsset(uint64_t hash) const
{
    auto found = m_MapByName.find(hash);
    if (found != m_MapByName.end())
    {
        return found->second;
    }

    return nullptr;
}

const UopFileEntry *CUopMappedFile::GetAsset(const char *filename) const
{
    uint64_t nameHash = uo_jenkins_hash(filename);
    m_FileName[nameHash] = filename;
    return GetAsset(nameHash);
}

size_t CUopMappedFile::FileCount() const
{
    return m_MapByName.size();
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
    int z_err = mz_uncompress(dst, &dLen, p, cLen);
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
        Error(Data, "Could not load UO data");
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
    //Try to use map uop files first, if we can, we will use them.
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

    if (!UopLoadFile(m_GumpartLegacyMUL, "gumpartLegacyMUL.uop"))
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
    UopLoadFile(m_Tileart, "tileart.uop");

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
    int lastGroup = 0;
    for (int animId = start; animId < end; ++animId)
    {
        auto &idx = g_Index.m_Anim[animId];
        for (int grpId = 0; grpId < MAX_ANIMATION_GROUPS_COUNT; ++grpId)
        {
            auto &group = idx.m_Groups[grpId];
            char filename[200];
            snprintf(
                filename,
                sizeof(filename),
                "build/animationlegacyframe/%06d/%02d.bin",
                animId,
                grpId);
            const auto asset = uo_jenkins_hash(filename);
            const auto fileIndex = getAssetOwner(asset);
            if (fileIndex != -1)
            {
                if (grpId > lastGroup)
                {
                    lastGroup = grpId;
                }
                idx.IsUOP = true;
                group.m_UOPAnimData = g_FileManager.m_AnimationFrame[fileIndex].GetAsset(filename);
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
        snprintf(name, sizeof(name), "AnimationFrame%d.uop", i + 1);

        auto &file = m_AnimationFrame[i];
        UopLoadFile(file, name);
    }
    UopLoadFile(m_AnimationSequence, "AnimationSequence.uop");

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

void CFileManager::ProcessAnimSequeceData()
{
    TRACE(Data, "processing AnimationSequence data");
    for (const auto /*&[hash, block]*/ &kvp : m_AnimationSequence.m_MapByName)
    {
        //const auto hash = kvp.first;
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

        auto indexAnim = &g_Index.m_Anim[animId];
        for (uint32_t i = 0; i < replaces; ++i)
        {
            const auto oldIdx = ReadInt32LE();
            const auto frameCount = ReadInt32LE();
            //auto group = indexAnim->m_Groups[oldIdx];
            if (frameCount == 0)
            {
                auto newIdx = ReadInt32LE();
                if (animId == 432 && oldIdx == 23)
                {
                    // a boura
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
    Info(Data, "AnimationSequence processed %zd entries", m_AnimationSequence.FileCount());
}
/*
static void DateFromTimestamp(const time_t rawtime, char *out, int maxLen)
{
    struct tm *dt = localtime(&rawtime);
    strftime(out, maxLen, "%c", dt);
}
*/
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
    file.Move(next);
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
        file.Move(next);
    } while (next != 0);
    file.ResetPtr();

    if (dumpInfo)
    {
        std::sort(file.m_NameHashes.begin(), file.m_NameHashes.end());
        std::sort(file.m_FileOffsets.begin(), file.m_FileOffsets.end());

        DEBUG(Data, "MypHeader for %s", uopFilename);
        DEBUG(Data, "\tVersion......: %d", file.Header->Version);
        DEBUG(Data, "\tSignature....: %08X", file.Header->Signature);
        DEBUG(Data, "\tSectionOffset: %016lx", file.Header->SectionOffset);
        DEBUG(Data, "\tFileCapacity.: %d", file.Header->FileCapacity);
        DEBUG(Data, "\tFileCount....: %d (Real: %d)", file.Header->FileCount, file.FileCount());
        DEBUG(Data, "\tSectionCount.: %08x", file.Header->SectionCount);
        DEBUG(Data, "\tUnk1.........: %08x", file.Header->Unk1);
        DEBUG(Data, "\tUnk2.........: %08x", file.Header->Unk2);
        DEBUG(Data, "\tBlocks: ");
        //for (const auto /*&[hash, block]*/ &kvp : file.m_Map)
        for (const auto it : file.m_NameHashes)
        //for (const auto it : file.m_FileOffsets)
        {
            //const auto hash = kvp.first;
            //const auto block = kvp.second;
            //const auto block = file.m_Map[it];
            const auto block = file.m_MapByName[it];

            auto meta = (UopFileMetadata *)(file.Start + block->Offset);
            DEBUG(Data, "\t\tBlock Header %08X_%016lX:", block->Checksum, block->Hash);
            DEBUG(Data, "\t\t\tOffset..........: %016lx", block->Offset);
            DEBUG(Data, "\t\t\tMetadataSize....: %d", block->MetadataSize);
            DEBUG(Data, "\t\t\tCompressedSize..: %d", block->CompressedSize);
            DEBUG(Data, "\t\t\tDecompressedSize: %d", block->DecompressedSize);
            DEBUG(Data, "\t\t\tHash............: %016lx", block->Hash);
            DEBUG(Data, "\t\t\tChecksum........: %08X", block->Checksum);
            DEBUG(Data, "\t\t\tFlags...........: %d", block->Flags);

            //char date[128];
            //DateFromTimestamp(meta->Unk / 100000000, date, sizeof(date));
            DEBUG(Data, "\t\t\tMetadata........: ");
            DEBUG(Data, "\t\t\t\tType....: %d", meta->Type);
            DEBUG(Data, "\t\t\t\tSize....: %d", meta->Size);
            switch (meta->Type)
            {
                case 3:
                {
                    auto meta3 = (UopFileMetadata3 *)(file.Start + block->Offset + meta->Size);
                    assert(sizeof(UopFileMetadata3) + meta->Size == block->MetadataSize);
                    DEBUG(Data, "\t\t\t\tUnk: %d (0x%08x)", meta3->Unk, meta3->Unk);
                }
                break;

                case 4:
                {
                    auto meta4 = (UopFileMetadata4 *)(file.Start + block->Offset + meta->Size);
                    assert(sizeof(UopFileMetadata4) + meta->Size == block->MetadataSize);
                    DEBUG(Data, "\t\t\t\tSigType.: %d (0x%04x)", meta4->SigType, meta4->SigType);
                    DEBUG(Data, "\t\t\t\tSigSize.: %d (0x%04x)", meta4->SigSize, meta4->SigSize);
                    //auto data = (uint8_t *)(file.Start + block->Offset + sizeof(UopFileMetadata));
                    //DEBUG_DUMP("\t\t\t\tData....: ", data, block->MetadataSize);
                }
                break;

                case 5:
                {
                }
                break;

                default:
                {
                    DEBUG(Data, "Unknown Meta Type: %d", meta->Type);
                    assert(false && "unknown metadata type");
                }
                break;
            };
        }
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
        for (const auto /*&[hash, block]*/ &kvp : file.m_MapByName)
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
    std::string p = uopFileName;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);

    bool isGump = (std::string("gumpartlegacymul") == p);
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
                uopFile.Move(block->Offset + block->MetadataSize);

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
    auto &block = *g_Index.m_Anim[anim.Graphic].m_Groups[anim.Group].m_UOPAnimData;
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

    auto framesData = UopReadAnimationFramesData();

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
        if (isCorpse)
        {
            frameIndex = frameCount - 1;
        }
        else
        {
            frameIndex = 0;
        }
    }

    if (frameIndex < frameCount)
    {
        uint32_t *frameOffset = (uint32_t *)Ptr;
        //Move(frameOffset[frameIndex]);
        Ptr = dataStart + frameOffset[frameIndex];
        result.CenterX = ReadInt16LE();
        result.CenterY = ReadInt16LE();
        result.Width = ReadInt16LE();
        result.Height = ReadInt16LE();
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
            if (val != 0u)
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
        UopReadAnimationFrameInfo(result, direction, *group.m_UOPAnimData);
    }
}

bool CFileManager::LoadAnimation(const AnimationSelector &anim, LoadPixelData16Cb pLoadFunc)
{
    CTextureAnimationGroup &group = g_Index.m_Anim[anim.Graphic].m_Groups[anim.Group];
    CTextureAnimationDirection &direction = group.m_Direction[anim.Direction];
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
