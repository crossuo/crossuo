// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <vector>
#include <stdint.h>
#include <string.h>
#include <common/fs.h>

struct CDataWriter
{
    bool AutoResize = true;
    uint8_t *Ptr = 0;

    CDataWriter() = default;
    CDataWriter(size_t size, bool autoResize = true);
    ~CDataWriter();

    std::vector<uint8_t> Data() const { return m_Data; }
    uint8_t *DataPtr() { return &m_Data[0]; }
    size_t Size() { return m_Data.size(); }

    void Resize(size_t newSize, bool resetPtr = false);
    void ResetPtr() { Ptr = &m_Data[0]; }
    void Move(const intptr_t &offset);

    void WriteDataBE(const uint8_t *data, size_t size, const intptr_t &offset = 0);
    void WriteDataLE(const uint8_t *data, size_t size, const intptr_t &offset = 0);

    void WriteUInt8(uint8_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint8_t), offset);
    }

    void WriteUInt16BE(uint16_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint16_t), offset);
    }
    void WriteUInt16LE(uint16_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint16_t), offset);
    }

    void WriteUInt32BE(int val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint32_t), offset);
    }
    void WriteUInt32LE(int val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint32_t), offset);
    }

    void WriteUInt64BE(uint64_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(uint64_t), offset);
    }
    void WriteUInt64LE(uint64_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(uint64_t), offset);
    }

    void WriteInt8(int8_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int8_t), offset);
    }

    void WriteInt16BE(int16_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int16_t), offset);
    }
    void WriteInt16LE(int16_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int16_t), offset);
    }

    void WriteInt32BE(int32_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int32_t), offset);
    }
    void WriteInt32LE(int32_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int32_t), offset);
    }

    void WriteInt64BE(int64_t val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(int64_t), offset);
    }
    void WriteInt64LE(int64_t val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(int64_t), offset);
    }

    void WriteFloatBE(float val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(float), offset);
    }
    void WriteFloatLE(float val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(float), offset);
    }

    void WriteDoubleBE(double val, const intptr_t &offset = 0)
    {
        WriteDataBE((uint8_t *)&val, sizeof(double), offset);
    }
    void WriteDoubleLE(double val, const intptr_t &offset = 0)
    {
        WriteDataLE((uint8_t *)&val, sizeof(double), offset);
    }

    void WriteString(
        const astr_t &val,
        size_t length = 0,
        bool nullTerminated = true,
        const intptr_t &offset = 0);
    void WriteWString(
        const wstr_t &val,
        size_t length = 0,
        bool bigEndian = true,
        bool nullTerminated = true,
        const intptr_t &offset = 0);

protected:
    std::vector<uint8_t> m_Data;
};

struct CDataReader
{
    uint8_t *Start = 0;
    size_t Size = 0;
    uint8_t *End = 0;
    uint8_t *Ptr = 0;

#if defined(XUO_EMSC)
    // file-backed mode (web): the source file is never copied into the fixed
    // wasm linear heap (a MEMFS mmap allocates the full file size there);
    // every read resolves through a bounded pread window over the fd.
    // While FileBacked is set, Start/End/Ptr describe the window (never the
    // whole file) and ReadPos is the logical cursor inside [0, Size).
    bool FileBacked = false;
    int FileFd = -1;
    // stream-on-demand source (web): the plan carries the file as streamable
    // but the install holds no bytes yet (the boot skips large loose files
    // instead of leaving hollow zero-filled placeholders). FileFd stays -1
    // until the first read demands the stream lane (bounded range wait) and
    // borrows the install descriptor.
    astr_t StreamName;
    uint64_t ReadPos = 0;   // logical read cursor (relative to FileBase)
    uint64_t FileBase = 0;  // logical 0 lives at this file offset
    uint64_t FileLimit = 0; // exclusive end of the readable span (FileBase + Size)
    uint64_t WinBase = 0;   // file offset of Win[0]
    uint32_t WinLen = 0;    // valid bytes in Win
    std::vector<uint8_t> Win;
    // stream-on-demand registration (web): declares the source lazily served
    // by the stream-file lane under the registry key `name` with `size`
    // planned bytes. The cursor starts at 0; reads acquire the descriptor on
    // demand. Returns false on absurd sizes (a zero-size stream reads cold).
    bool BeginStreamFile(const char *name, uint64_t size);
#endif

    CDataReader() = default;
    CDataReader(uint8_t *start, size_t size);
    ~CDataReader();

    void SetData(uint8_t *start, size_t size, const intptr_t &offset = 0);
#if defined(XUO_EMSC)
    // file-backed mode: reads [windowBase, windowBase + windowSize) of the
    // open descriptor `fd` through a bounded window. The fd is borrowed (the
    // owner keeps closing it); the cursor starts at windowBase. Size becomes
    // the logical span length; Start stays null (nothing is mapped).
    bool BeginFile(int fd, uint64_t windowBase, uint64_t windowSize);
    // leaves file-backed mode (frees the window; does not close the fd)
    void EndFile();
    // borrows the source descriptor for a read: a cold stream-on-demand file
    // first waits (bounded) for the lane front to pass the span (loud once
    // per file on a timeout - hollow zeros are never served silently)
    bool FileAcquire(uint64_t pos, size_t need);
    // makes [pos, pos + need) readable through the window; false when the
    // span leaves the file or `need` exceeds the window capacity (large
    // reads bypass the window with a direct pread instead)
    bool FileEnsure(uint64_t pos, size_t need);
    // direct (window-bypassing) pread; returns the bytes transferred
    size_t FilePread(uint64_t fileOffset, uint8_t *dst, size_t bytes);
#endif
    // window pointer valid for the next `need` bytes at the cursor (null
    // when the span leaves the file); the cursor does not move
    const uint8_t *WindowPtr(size_t need) const
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            return FileEnsure(ReadPos, need) ? Ptr : nullptr;
        }
#endif
        return (Ptr != nullptr && size_t(End - Ptr) >= need) ? Ptr : nullptr;
    }
    // single byte peek at the cursor (0 past the end)
    uint8_t WindowByte() const
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            uint8_t val = 0;
            if (FileEnsure(ReadPos, 1))
            {
                val = *Ptr;
            }
            return val;
        }
#endif
        return (Ptr != nullptr && Ptr < End) ? *Ptr : uint8_t(0);
    }
    // whole readable span into a heap buffer (bounded files only; large
    // reads pread straight into the destination). Platform-neutral: on
    // native it copies out of the mapping.
    std::vector<uint8_t> FileReadWhole();
    bool IsFileBacked() const
    {
#if defined(XUO_EMSC)
        return FileBacked;
#else
        return false;
#endif
    }

    // -----------------------------------------------------------------
    // unified data-source attach: THE platform factory for "parse a slice
    // of a mapped file". Native points the reader into the mapping; web
    // resolves through a bounded file-backed window over the descriptor.
    // `offset` is a file offset in both modes; `len` bounds the readable
    // span. Every per-callsite "EMSC: BeginFile / native: SetData(Start+
    // off)" pair collapses into this one call.
    // -----------------------------------------------------------------
    bool Attach(const struct CMappedFile &file, uint64_t offset, uint64_t len);

    // "the file holds data": mapped (native), or file-backed / lazy stream
    // (web). Replaces the raw Start != nullptr guards at the call sites.
    bool Loaded() const
    {
#if defined(XUO_EMSC)
        return FileBacked || Start != nullptr;
#else
        return Start != nullptr;
#endif
    }
    // logical read cursor (window-local byte offset in file-backed mode)
    size_t Pos() const
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            return size_t(ReadPos);
        }
#endif
        return size_t(Ptr - Start);
    }
    // absolute positioning; false when the position leaves the span
    bool Seek(size_t pos)
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            if (uint64_t(pos) > uint64_t(Size))
            {
                return false;
            }
            ReadPos = uint64_t(pos);
            Ptr = nullptr; // re-ensured lazily by the next read
            return true;
        }
#endif
        if (Start == nullptr || pos > Size)
        {
            return false;
        }
        Ptr = Start + ptrdiff_t(pos);
        return true;
    }

    void ResetPtr()
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            ReadPos = 0;
            Ptr = nullptr;
            return;
        }
#endif
        Ptr = Start;
    }
    bool IsEOF() const
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            return ReadPos >= uint64_t(Size);
        }
#endif
        return Ptr >= End;
    }
    void Move(const intptr_t &offset)
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            ReadPos += uint64_t(offset);
            Ptr = nullptr; // re-ensured lazily by the next read
            return;
        }
#endif
        Ptr += offset;
    }

    void ReadDataBE(uint8_t *data, size_t size, const intptr_t &offset = 0);
    void ReadDataLE(uint8_t *data, size_t size, const intptr_t &offset = 0);

    uint8_t ReadUInt8(const intptr_t &offset = 0)
    {
        uint8_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint8_t), offset);
        return val;
    }

    uint16_t ReadUInt16BE(const intptr_t &offset = 0)
    {
        uint16_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint16_t), offset);
        return val;
    }
    uint16_t ReadUInt16LE(const intptr_t &offset = 0)
    {
        uint16_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint16_t), offset);
        return val;
    }

    uint32_t ReadUInt32BE(const intptr_t &offset = 0)
    {
        uint32_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint32_t), offset);
        return val;
    }
    uint32_t ReadUInt32LE(const intptr_t &offset = 0)
    {
        uint32_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint32_t), offset);
        return val;
    }

    int8_t ReadInt8(const intptr_t &offset = 0)
    {
        char val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int8_t), offset);
        return val;
    }

    int16_t ReadInt16BE(const intptr_t &offset = 0)
    {
        int16_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int16_t), offset);
        return val;
    }
    int16_t ReadInt16LE(const intptr_t &offset = 0)
    {
        int16_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int16_t), offset);
        return val;
    }

    int32_t ReadInt32BE(const intptr_t &offset = 0)
    {
        int32_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int32_t), offset);
        return val;
    }
    int32_t ReadInt32LE(const intptr_t &offset = 0)
    {
        int32_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int32_t), offset);
        return val;
    }

    int64_t ReadInt64BE(const intptr_t &offset = 0)
    {
        int64_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(int64_t), offset);
        return val;
    }
    int64_t ReadInt64LE(const intptr_t &offset = 0)
    {
        int64_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(int64_t), offset);
        return val;
    }

    uint64_t ReadUInt64BE(const intptr_t &offset = 0)
    {
        uint64_t val = 0;
        ReadDataBE((uint8_t *)&val, sizeof(uint64_t), offset);
        return val;
    }
    uint64_t ReadUInt64LE(const intptr_t &offset = 0)
    {
        uint64_t val = 0;
        ReadDataLE((uint8_t *)&val, sizeof(uint64_t), offset);
        return val;
    }

    float ReadFloatBE(const intptr_t &offset = 0)
    {
        float val = 0.0f;
        ReadDataBE((uint8_t *)&val, sizeof(float), offset);
        return val;
    }
    float ReadFloatLE(const intptr_t &offset = 0)
    {
        float val = 0.0f;
        ReadDataLE((uint8_t *)&val, sizeof(float), offset);
        return val;
    }

    double ReadDoubleBE(const intptr_t &offset = 0)
    {
        double val = 0.0;
        ReadDataBE((uint8_t *)&val, sizeof(double), offset);
        return val;
    }
    double ReadDoubleLE(const intptr_t &offset = 0)
    {
        double val = 0.0;
        ReadDataLE((uint8_t *)&val, sizeof(double), offset);
        return val;
    }

    template <typename T, size_t SIZE>
    void ReadBuffer(T (&buffer)[SIZE])
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            // the windowed read carries the bounds check the raw memcpy
            // below cannot do without a mapped image
            ReadDataLE((uint8_t *)buffer, SIZE * sizeof(T));
            return;
        }
#endif
        memcpy(buffer, Ptr, SIZE * sizeof(T));
        // advance by the same byte count the copy moved (both branches share
        // the SIZE * sizeof(T) semantic; every call site is a char array)
        Move(intptr_t(SIZE * sizeof(T)));
    }
    astr_t ReadString(size_t size = 0, const intptr_t &offset = 0);
    wstr_t ReadWStringBE(size_t size = 0, const intptr_t &offset = 0);
    wstr_t ReadWStringLE(size_t size = 0, const intptr_t &offset = 0);
    wstr_t ReadWString(size_t size = 0, bool bigEndian = true, const intptr_t &offset = 0);
};

struct CMappedFile : public CDataReader
{
    CMappedFile() = default;
    ~CMappedFile() { Unload(); }

    bool Load(const fs_path &path);
    void Unload();

#if defined(XUO_EMSC)
    // web: mapped files are file-backed (the bytes stay in the store file,
    // outside the fixed wasm heap); Start stays null for them
    FILE *m_Fp = nullptr; // owner of the descriptor (fs_close on Unload)
    int m_Fd = -1;
    fs_path m_Path; // resolved source path (managed installs resolve case)
    int Fd() const { return FileFd; }
    const fs_path &Path() const { return m_Path; }
#endif

    // replaces the mapping with heap memory owned by this file (Unload-safe);
    // used when a mapped payload has to be decoded before it can be parsed
    void Attach(std::vector<uint8_t> &&data)
    {
        Unload();
        m_Memory = std::move(data);
        SetData(m_Memory.data(), m_Memory.size());
    }

    // adopt the whole source as owned heap memory (web file-backed only;
    // no-op when mapped on native - the mapping already backs the pointers)
    void Materialize()
    {
#if defined(XUO_EMSC)
        if (FileBacked)
        {
            Attach(FileReadWhole());
        }
#endif
    }

private:
    std::vector<uint8_t> m_Memory;
};
