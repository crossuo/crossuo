// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "mappedfile.h"
#include <common/str.h>
#include <common/logging/logging.h>
#include <string.h> // memcpy
#include <set>

#define LOGGER_MODULE Filesystem
#include "vfs.h"
#include "common.h" // LOG_ERROR (the Filesystem module logger)

#if defined(XUO_EMSC)
#include <unistd.h> // pread
#endif

#if 0
#define DATASTREAM_DEBUG DEBUG_TRACE_FUNCTION
#else
#define DATASTREAM_DEBUG
#endif

CDataWriter::CDataWriter(size_t size, bool autoResize)
    : AutoResize(autoResize)
{
    DATASTREAM_DEBUG;
    m_Data.resize(size, 0);
    Ptr = &m_Data[0];
}

CDataWriter::~CDataWriter()
{
    DATASTREAM_DEBUG;
    m_Data.clear();
    Ptr = nullptr;
}

void CDataWriter::Resize(size_t newSize, bool resetPtr)
{
    DATASTREAM_DEBUG;
    m_Data.resize(newSize, 0);

    if (resetPtr)
    {
        Ptr = &m_Data[0];
    }
}

void CDataWriter::Move(const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (AutoResize)
    {
        for (int i = 0; i < (int)offset; i++)
        {
            m_Data.push_back(0);
        }
    }
    else if (Ptr != nullptr)
    {
        Ptr += offset;
    }
}

void CDataWriter::WriteDataBE(const uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (AutoResize)
    {
        for (int i = (int)size - 1; i >= 0; i--)
        {
            m_Data.push_back(data[i]);
        }
    }
    else if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset + size - 1;

        for (int i = (int)size - 1; i >= 0; i--)
        {
            *(ptr - i) = data[i];
        }

        Ptr += size;
    }
}

void CDataWriter::WriteDataLE(const uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (AutoResize)
    {
        for (int i = 0; i < (int)size; i++)
        {
            m_Data.push_back(data[i]);
        }
    }
    else if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset;

        for (int i = 0; i < (int)size; i++)
        {
            ptr[i] = data[i];
        }

        Ptr += size;
    }
}

void CDataWriter::WriteString(
    const astr_t &val, size_t length, bool nullTerminated, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (length == 0u)
    {
        length = val.length();
    }

    if (length <= val.length())
    {
        WriteDataLE((uint8_t *)val.c_str(), length, offset);
    }
    else
    {
        WriteDataLE((uint8_t *)val.c_str(), val.length(), offset);
        Move(length - val.length());
    }

    if (nullTerminated)
    {
        uint8_t nullTerminator = 0;
        WriteDataBE((uint8_t *)&nullTerminator, sizeof(uint8_t), offset);
    }
}

void CDataWriter::WriteWString(
    const wstr_t &val, size_t length, bool bigEndian, bool nullTerminated, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    if (length == 0u)
    {
        length = val.length();
    }

    Ptr += offset;
    size_t size = val.length();

    if (length <= size)
    {
        if (bigEndian)
        {
            for (int i = 0; i < (int)length; i++)
            {
                WriteInt16BE(val[i]);
            }
        }
        else
        {
            for (int i = 0; i < (int)length; i++)
            {
                WriteInt16LE(val[i]);
            }
        }
    }
    else
    {
        if (bigEndian)
        {
            for (int i = 0; i < (int)size; i++)
            {
                WriteInt16BE(val[i]);
            }
        }
        else
        {
            for (int i = 0; i < (int)size; i++)
            {
                WriteInt16LE(val[i]);
            }
        }

        Move((length - size) * 2);
    }

    if (nullTerminated)
    {
        uint16_t nullTerminator = 0;
        WriteDataBE((uint8_t *)&nullTerminator, sizeof(uint16_t), offset);
    }

    Ptr -= offset;
}

CDataReader::CDataReader(uint8_t *start, size_t size)
    : Start(start)
    , Size(size)
    , End(Start + size)
{
    DATASTREAM_DEBUG;
    Ptr = Start;
}

CDataReader::~CDataReader()
{
    DATASTREAM_DEBUG;
#if defined(XUO_EMSC)
    EndFile();
#endif
    Start = nullptr;
    Size = 0;
    End = nullptr;
    Ptr = nullptr;
}

void CDataReader::SetData(uint8_t *start, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
#if defined(XUO_EMSC)
    // adopting a memory buffer leaves file-backed mode (the window and the
    // borrowed descriptor no longer describe the source)
    EndFile();
#endif
    Start = start;
    Size = size;
    End = Start + size;
    Ptr = Start + offset;
}

#if defined(XUO_EMSC)
// web file-backed reads: the window capacity bounds both the read-ahead
// buffer and the largest in-window read (larger reads pread straight into
// the caller's destination). 64KB keeps ~120 open files at ~8MB of windows
// while a single fill still amortizes hot sequential decode loops.
static constexpr uint64_t FILE_WINDOW_SIZE = 64 * 1024;

bool CDataReader::BeginFile(int fd, uint64_t windowBase, uint64_t windowSize)
{
    DATASTREAM_DEBUG;
    EndFile();
    if (fd < 0)
    {
        return false;
    }
    FileFd = fd;
    FileBase = windowBase;
    FileLimit = windowBase + windowSize;
    Size = size_t(windowSize);
    Start = nullptr;
    End = nullptr;
    Ptr = nullptr;
    ReadPos = 0;
    WinBase = windowBase;
    WinLen = 0;
    FileBacked = true;
    if (Win.empty())
    {
        Win.resize(size_t(FILE_WINDOW_SIZE));
    }
    return true;
}

bool CDataReader::BeginStreamFile(const char *name, uint64_t size)
{
    DATASTREAM_DEBUG;
    EndFile();
    if (name == nullptr || size == 0)
    {
        return false;
    }
    StreamName = name;
    FileFd = -1; // acquired by the first read (stream lane range wait)
    FileBase = 0;
    FileLimit = size;
    Size = size_t(size);
    Start = nullptr;
    End = nullptr;
    Ptr = nullptr;
    ReadPos = 0;
    WinBase = 0;
    WinLen = 0;
    FileBacked = true;
    if (Win.empty())
    {
        Win.resize(size_t(FILE_WINDOW_SIZE));
    }
    return true;
}

void CDataReader::EndFile()
{
    DATASTREAM_DEBUG;
    FileBacked = false;
    FileFd = -1;
    StreamName.clear();
    ReadPos = 0;
    FileBase = 0;
    FileLimit = 0;
    WinBase = 0;
    WinLen = 0;
    if (!Win.empty())
    {
        Win.clear();
        Win.shrink_to_fit();
    }
}

size_t CDataReader::FilePread(uint64_t fileOffset, uint8_t *dst, size_t bytes)
{
    size_t done = 0;
    while (done < bytes)
    {
        const auto n = pread(
            FileFd, dst + done, bytes - done, off_t(fileOffset + uint64_t(done)));
        if (n <= 0)
        {
            break; // MEMFS reads are whole; a short read means end of file
        }
        done += size_t(n);
    }
    return done;
}

// loud once-per-file failure (shared by the windowed read path and Attach):
// a read that cannot be served must never degrade to silent zeros (the
// hollow-placeholder failure mode this lane replaces)
static void stream_lane_complain(const astr_t &name, uint64_t span, size_t need)
{
    static std::set<astr_t> complained;
    if (complained.insert(name).second)
    {
        LOG_ERROR(
            "file-backed read of %s could not wait out the stream lane "
            "(span %llu+%zu stays hollow; reads fail loudly until the lane delivers)",
            name.c_str(),
            (unsigned long long)span,
            need);
    }
}

bool CDataReader::FileAcquire(uint64_t pos, size_t need)
{
    if (!FileBacked)
    {
        return false;
    }
    if (FileFd >= 0)
    {
        return true;
    }
    if (StreamName.empty())
    {
        return false; // no descriptor and no lane: plain cold file-backed read
    }
    // stream-on-demand: wait (bounded) for the paced lane front to pass the
    // span, then borrow the install descriptor. The lane keeps its pacing;
    // reads ahead of the front park here briefly (UO loaders read
    // sequentially-ish, so the front converges under them).
    FileFd = vfs_stream_file_ensure_fd(StreamName.c_str(), FileBase + pos, uint64_t(need));
    if (FileFd < 0)
    {
        stream_lane_complain(StreamName, FileBase + pos, need);
        return false;
    }
    return true;
}

bool CDataReader::FileEnsure(uint64_t pos, size_t need)
{
    if (!FileBacked || !FileAcquire(pos, need))
    {
        return false;
    }
    if (need == 0)
    {
        return pos <= uint64_t(Size);
    }
    if (pos > uint64_t(Size) || uint64_t(need) > uint64_t(Size) - pos)
    {
        return false; // the read span leaves the logical window
    }
    if (uint64_t(need) > FILE_WINDOW_SIZE)
    {
        return false; // large reads bypass the window (direct pread)
    }
    const uint64_t fileOffset = FileBase + pos;
    if (fileOffset >= WinBase && fileOffset + uint64_t(need) <= WinBase + uint64_t(WinLen))
    {
        Ptr = Win.data() + size_t(fileOffset - WinBase); // already resident
        return true;
    }
    // refill: one pread per window fill (a JS-side MEMFS op)
    const uint64_t span = FileLimit - fileOffset;
    const size_t len = span < FILE_WINDOW_SIZE ? size_t(span) : size_t(FILE_WINDOW_SIZE);
    WinLen = uint32_t(FilePread(fileOffset, Win.data(), len));
    WinBase = fileOffset;
    Start = Win.data();
    End = Start + WinLen;
    Ptr = Start;
    return WinLen == len; // a short window means the read span is not there
}

#endif

// whole readable span into a heap buffer: web resolves through the source
// descriptor (acquiring it first - a lazy stream file waits for its lane);
// native copies out of the mapping
std::vector<uint8_t> CDataReader::FileReadWhole()
{
    std::vector<uint8_t> out;
#if defined(XUO_EMSC)
    if (FileBacked)
    {
        if (Size != 0 && FileAcquire(0, size_t(Size)))
        {
            out.resize(Size);
            if (FilePread(FileBase, out.data(), Size) != Size)
            {
                out.clear();
            }
        }
        return out;
    }
#endif
    if (Start != nullptr && Size != 0)
    {
        out.assign(Start, Start + Size);
    }
    return out;
}

// THE platform factory (see the header comment): native points into the
// mapping, web opens a bounded file-backed window at the file offset
bool CDataReader::Attach(const CMappedFile &file, uint64_t offset, uint64_t len)
{
#if defined(XUO_EMSC)
    if (file.FileBacked)
    {
        // a stream-on-demand source owns no descriptor until its first read
        // (Fd() == -1): acquire it first - the same bounded lane wait and
        // once-per-file failure the windowed read path uses - or BeginFile
        // would reject every read of a lazily streamed file
        int fd = file.Fd();
        if (fd < 0)
        {
            if (file.StreamName.empty())
            {
                return false; // no descriptor and no lane to demand one from
            }
            fd = vfs_stream_file_ensure_fd(file.StreamName.c_str(), offset, len);
            if (fd < 0)
            {
                stream_lane_complain(file.StreamName, offset, size_t(len));
                return false;
            }
        }
        return BeginFile(fd, offset, len);
    }
#endif
    if (file.Start == nullptr || offset > uint64_t(file.Size) ||
        len > uint64_t(file.Size) - offset)
    {
        return false;
    }
    SetData(file.Start + ptrdiff_t(offset), size_t(len));
    return true;
}

void CDataReader::ReadDataBE(uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
#if defined(XUO_EMSC)
    if (FileBacked)
    {
        const uint64_t pos = ReadPos + uint64_t(offset);
        if (pos > uint64_t(Size) || uint64_t(size) > uint64_t(Size) - pos)
        {
            return;
        }
        if (uint64_t(size) > FILE_WINDOW_SIZE)
        {
            // read larger than the window capacity: direct pread into the
            // caller's destination (no window copy)
            std::vector<uint8_t> scratch(size);
            if (!FileAcquire(pos, size) ||
                FilePread(FileBase + pos, scratch.data(), size) != size)
            {
                return;
            }
            for (size_t i = 0; i < size; i++)
            {
                data[i] = scratch[size - 1 - i];
            }
        }
        else if (FileEnsure(pos, size))
        {
            const uint8_t *src = Ptr + ptrdiff_t(size - 1);
            for (size_t i = 0; i < size; i++)
            {
                data[i] = src[-ptrdiff_t(i)];
            }
        }
        else
        {
            return;
        }
        ReadPos = pos + uint64_t(size);
        return;
    }
#endif
    if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset + size - 1;
        if (ptr >= Start && ptr <= End)
        {
            for (int i = 0; i < (int)size; i++)
            {
                data[i] = *(ptr - i);
            }

            Ptr += size;
        }
    }
}

void CDataReader::ReadDataLE(uint8_t *data, size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
#if defined(XUO_EMSC)
    if (FileBacked)
    {
        if (size == 0u)
        {
            return; // nothing to move: skip the window dance and the memcpy
        }
        const uint64_t pos = ReadPos + uint64_t(offset);
        if (pos > uint64_t(Size) || uint64_t(size) > uint64_t(Size) - pos)
        {
            return;
        }
        if (uint64_t(size) > FILE_WINDOW_SIZE)
        {
            // read larger than the window capacity: direct pread into the
            // caller's destination (no window copy)
            if (!FileAcquire(pos, size) || FilePread(FileBase + pos, data, size) != size)
            {
                return;
            }
        }
        else if (FileEnsure(pos, size))
        {
            memcpy(data, Ptr, size);
        }
        else
        {
            return;
        }
        ReadPos = pos + uint64_t(size);
        return;
    }
#endif
    if (Ptr != nullptr)
    {
        uint8_t *ptr = Ptr + offset;
        if (ptr >= Start && ptr + size <= End)
        {
            memcpy(data, ptr, size);
            Ptr += size;
        }
    }
}

astr_t CDataReader::ReadString(size_t size, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    uint8_t *ptr = Ptr + offset;
#if defined(XUO_EMSC)
    if (FileBacked && size == 0u)
    {
        // NUL-terminated scan bounded by the readable span: walk the window
        // with refills instead of dereferencing a (nonexistent) mapping
        const uint64_t start = ReadPos + uint64_t(offset);
        uint64_t scan = start;
        bool found = false;
        while (!found && scan < uint64_t(Size))
        {
            const uint64_t left = uint64_t(Size) - scan;
            const size_t chunk = left < FILE_WINDOW_SIZE ? size_t(left) : size_t(FILE_WINDOW_SIZE);
            if (!FileEnsure(scan, chunk))
            {
                break;
            }
            const uint8_t *base = Win.data() + size_t(FileBase + scan - WinBase);
            const void *nul = memchr(base, 0, chunk);
            if (nul != nullptr)
            {
                scan += uint64_t((const uint8_t *)nul - base) + 1;
                found = true;
            }
            else
            {
                scan += uint64_t(chunk);
            }
        }
        const auto len = size_t(scan - start);
        auto *buf = (char *)alloca(len + 1);
        ReadDataLE((uint8_t *)buf, len, offset);
        buf[len] = '\0';
        return { buf };
    }
#endif
    if (size == 0u)
    {
        if (ptr >= Start && ptr <= End)
        {
            uint8_t *buf = ptr;
            while (buf <= End && (*buf != 0u))
            {
                buf++;
            }
            size = (buf - ptr) + 1;
        }
    }

    auto *buf = (char *)alloca(size + 1);
    ReadDataLE((uint8_t *)buf, size, offset);
    buf[size] = '\0';
    return { buf };
}

wstr_t CDataReader::ReadWString(size_t size, bool bigEndian, const intptr_t &offset)
{
    DATASTREAM_DEBUG;
    uint8_t *ptr = Ptr + offset;
#if defined(XUO_EMSC)
    if (FileBacked && size == 0u)
    {
        // NUL-terminated (uint16) scan bounded by the readable span, mirroring
        // the native walk below on windowed storage
        const uint64_t start = ReadPos + uint64_t(offset);
        uint64_t scan = start;
        while (scan + 2 <= uint64_t(Size))
        {
            if (!FileEnsure(scan, 2))
            {
                break;
            }
            const uint8_t *base = Win.data() + size_t(FileBase + scan - WinBase);
            const uint16_t val =
                bigEndian ? uint16_t((base[0] << 8) | base[1]) : *(const uint16_t *)base;
            scan += 2;
            if (val == 0u)
            {
                break;
            }
        }
        size = size_t((scan - start) / 2);
        // the scalar reads below resolve through the window; the native
        // pointer guard must not run
        wstr_t result;
        if (start + uint64_t(size) * 2 <= uint64_t(Size))
        {
            result.reserve(size);
            for (int i = 0; i < (int)size; i++)
            {
                wchar_t c = (bigEndian ? ReadInt16BE(offset) : ReadInt16LE(offset)) & 0xffff;
                result.push_back(c);
            }
        }
        return result;
    }
#endif
    if (size == 0u)
    {
        if (ptr >= Start && ptr <= End)
        {
            uint8_t *buf = ptr;
            while (buf <= End)
            {
                uint16_t val = (bigEndian ? ((buf[0] << 8) | buf[1]) : *(uint16_t *)buf);
                buf += 2;
                if (val == 0u)
                {
                    break;
                }
            }

            size = ((buf - ptr) / 2);
        }
    }

    wstr_t result;
#if defined(XUO_EMSC)
    if (FileBacked)
    {
        // explicit-size read (or an out-of-span one): the scalar reads
        // resolve through the window, the native pointer guard does not apply
        if (ReadPos + uint64_t(offset) + uint64_t(size) * 2 <= uint64_t(Size))
        {
            result.reserve(size);
            for (int i = 0; i < (int)size; i++)
            {
                wchar_t c = (bigEndian ? ReadInt16BE(offset) : ReadInt16LE(offset)) & 0xffff;
                result.push_back(c);
            }
        }
        return result;
    }
#endif
    if (ptr >= Start && ptr + size <= End)
    {
        result.reserve(size);
        if (bigEndian)
        {
            for (int i = 0; i < (int)size; i++)
            {
                wchar_t c = ReadInt16BE(offset) & 0xffff;
                result.push_back(c);
            }
        }
        else
        {
            for (int i = 0; i < (int)size; i++)
            {
                wchar_t c = ReadInt16LE(offset) & 0xffff;
                result.push_back(c);
            }
        }
    }
    return result;
}

wstr_t CDataReader::ReadWStringLE(size_t size, const intptr_t &offset)
{
    return ReadWString(size, false, offset);
}

wstr_t CDataReader::ReadWStringBE(size_t size, const intptr_t &offset)
{
    return ReadWString(size, true, offset);
}

bool CMappedFile::Load(const fs_path &path)
{
#if defined(XUO_EMSC)
    Info(Filesystem, "opening %s (file-backed)", fs_path_ascii(path));
#else
    Info(Filesystem, "mmaping %s", fs_path_ascii(path));
#endif
    bool result = false;

    // managed artifacts (streaming install) map with hole pages through the
    // trap - the managed lookup is case-normalized, so a lowercase install
    // answers the game's mixed-case requests even when the literal path is
    // absent; everything else - and everything, when streaming is disabled -
    // maps exactly as before
    if (fs_path_exists(path) || vfs_enabled())
    {
        Unload();
#if defined(XUO_EMSC)
        // web: an mmap of a MEMFS file allocates the full file size inside
        // the fixed wasm linear heap (no true paging), which OOMs products
        // shipping more than the heap size. Mapped files stay file-backed:
        // reads resolve through a bounded pread window over the store file
        // (its bytes live in the JS heap, outside the wasm cap).
        m_Path = path;
        // managed install: resolve the case-normalized name BEFORE the raw
        // attempt - the game asks stock mixed-case names while the
        // materialized install is lowercase, so the raw fopen below would
        // only produce a per-file error log on every boot (the managed
        // registry carries the case-normalized path)
        fs_path managed;
        if (m_Fp == nullptr && vfs_enabled() && vfs_managed_file_path(path, &managed))
        {
            m_Fp = fs_open(managed, FS_READ);
            if (m_Fp != nullptr)
            {
                m_Path = managed;
            }
        }
        if (m_Fp == nullptr)
        {
            m_Fp = fs_open(path, FS_READ);
        }
        if (m_Fp != nullptr)
        {
            m_Fd = fileno(m_Fp);
            if (m_Fd >= 0 && fseek(m_Fp, 0, SEEK_END) == 0)
            {
                const long size = ftell(m_Fp);
                rewind(m_Fp);
                if (size > 0 && BeginFile(m_Fd, 0, uint64_t(size)))
                {
                    const auto ascii_path = fs_path_ascii(path);
                    const auto len = strlen(ascii_path);
                    if (len > 4 && strcasecmp(ascii_path + len - 4, ".uop") == 0)
                    {
                        uint8_t head[8] = {};
                        pread(m_Fd, head, sizeof(head), 0);
                        Info(
                            Filesystem,
                            "opened %s (%ld bytes, head %02x %02x %02x %02x)",
                            ascii_path,
                            size,
                            head[0], head[1], head[2], head[3]);
                    }
                    result = true;
                }
            }
        }
        if (!result)
        {
            // stream-on-demand: the plan keeps this file as streamable
            // metadata (the web boot skips large loose files instead of
            // leaving hollow zero-filled placeholders). Report the file
            // present at its planned size; reads demand the lane front for
            // their span (bounded wait) before touching any byte.
            const char *streamKey = nullptr;
            uint64_t streamSize = 0;
            if (vfs_stream_file_plan(m_Path, &streamKey, &streamSize) &&
                BeginStreamFile(streamKey, streamSize))
            {
                Info(
                    Filesystem,
                    "opening %s (stream-on-demand, %.1f MB via the stream lane)",
                    fs_path_ascii(path),
                    double(streamSize) / 1048576.0);
                result = true;
            }
        }
        if (!result)
        {
            m_Path = fs_path();
            if (m_Fp != nullptr)
            {
                fs_close(m_Fp);
                m_Fp = nullptr;
            }
            m_Fd = -1;
        }
#else
        result = vfs_map_file(path, Start, Size);
        SetData(Start, Size);
#endif
    }
    else
    {
        Warning(Filesystem, "file not found %s", fs_path_ascii(path));
    }

    return result;
}

void CMappedFile::Unload()
{
#if defined(XUO_EMSC)
    // file-backed sources close here (the window dies with the reader state)
    if (m_Fp != nullptr)
    {
        fs_close(m_Fp);
        m_Fp = nullptr;
    }
    m_Fd = -1;
    m_Path = fs_path();
    EndFile();
#endif
    if (Start != nullptr && !m_Memory.empty())
    {
        Start = nullptr; // heap memory adopted via Attach: nothing to unmap
    }
    if (Start != nullptr)
    {
        vfs_unmap_file(Start, Size);
    }
    m_Memory.clear();
    SetData(nullptr, 0);
}
