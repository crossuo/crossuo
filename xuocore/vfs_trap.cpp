// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_trap implementation. See vfs_trap.h for the design contract.
//
// Threading and lock order: trap_lock (s_mutex) ->
// store_lock (the boot state mutex in vfs.cpp) -> fetch_lock (vfs_fetch).
// Resolution runs in three phases so the faulting thread never holds
// trap_lock across the backend call:
//   1. lookup/claim under trap_lock (mark covering blocks in-flight),
//   2. fetch + materialize with no trap lock held (backend),
//   3. commit + page flip under trap_lock, then wake dedup waiters.
// The backend (vfs.cpp) takes store_lock for the store write path and only
// touches fetch_lock inside the fetch, so a T1 thread holding store_lock can
// never form a cycle with a faulting thread (T1 never dereferences managed
// pages: it writes through FILE* handles only).
//
// Known async-signal-safety corners, deliberate trade-offs (a "pragmatic
// design": the handler is only async-signal-safe-ish by intent): the POSIX SIGSEGV handler takes s_mutex (a normal mutex, not a
// spinlock), waits on s_cv for in-flight dedup, and performs a blocking
// network fetch (curl) inside the handler. Everything else in the hot path
// is syscall or lock-free: mmap/mprotect/pwrite plus preallocated atomic
// counters; no logging, no allocation (fixed-size claim batches).
// Handlers are installed once; faults outside managed ranges chain to the
// previously installed action (SIG_IGN honored, default action restored and
// re-raised) - unrelated segfaults are never swallowed.

#define LOGGER_MODULE Vfs

#include "vfs_trap.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "uop.h"

#include "common.h"
#include "vfs_stub.h"
#include <common/logging/logging.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>
#endif

// resolution never spins past this waiting on other threads' in-flight work
#define VFS_TRAP_WAIT_DEADLINE_MS (120 * 1000)
// shutdown waits this long for in-flight resolutions before zombifying
#define VFS_TRAP_SHUTDOWN_WAIT_MS (5 * 1000)
// grace an unmap grants the last resolver after its pin drops: the faulting
// thread must return from the handler and re-execute its instruction against
// the flipped page before the pages may disappear
#define VFS_TRAP_UNMAP_SETTLE_MS 50
// transient mprotect failures get this many attempts before a page is
// declared stuck (no refault loop, no kill on transient ENOMEM)
#define VFS_TRAP_ZERO_ATTEMPTS 3
#define VFS_TRAP_ZERO_RETRY_MS 10
// fixed claim batch: keeps the fault path allocation-free; a page covered by
// more blocks than this resolves in several passes
#define VFS_TRAP_BATCH 64

namespace
{

// ---------------------------------------------------------------------------
// page state
// ---------------------------------------------------------------------------

enum vfs_trap_page_state : uint8_t
{
    trap_page_none = 0,     // protected (PROT_NONE / PAGE_NOACCESS)
    trap_page_readable = 1, // real bytes, readable
    trap_page_zeroed = 2,   // degraded: readable zeros (no stub content)
    trap_page_stubbed = 3,  // degraded: readable placeholder payload
};

enum vfs_trap_asset_state : uint8_t
{
    trap_asset_absent = 0,
    trap_asset_present = 1,
    trap_asset_inflight = 2,
};

struct vfs_trap_asset
{
    uint64_t start = 0;    // first byte of the block in the file
    uint64_t end = 0;      // one past the last byte
    uint64_t expected = 0; // exact on-wire blob size (0: unknown)
    uint32_t plan_index = 0; // asset index in the plan file (table position)
    uint8_t state = trap_asset_absent;
    // resolve-call epoch of the last failed attempt: bounds the retry loop
    // (a failed block is never re-claimed within the same resolution)
    uint32_t attempt = 0;
    // served from a stub payload; `reported` hands the transition to the
    // backend exactly once (state DB mark + demand prefetch)
    uint8_t stubbed = 0;
    uint8_t reported = 0;
};

struct vfs_trap_mapping
{
    uint8_t *base = nullptr;
    size_t size = 0;      // mapped (file) size; pages beyond it are holes
    size_t table_end = 0; // header + section + entries (always present)
    fs_path path;
    const VfsPlan *plan = nullptr; // borrowed: the plan outlives its mappings
    size_t file_index = 0;
    bool is_pack = false;
    vfs_stub_kind stub_kind = vfs_stub_misc; // stub payload kind of the whole file
    bool sealed = false;
    // zombie: unmap/shutdown gave up waiting for a resolution and left the
    // mapping (object + pages) alive for the process lifetime; never unmapped
    // or erased again, so no in-flight resolver can touch freed state
    bool zombie = false;
    uint32_t resolve_epoch = 0;
    uint32_t resolving = 0; // resolutions in flight (pins against unmap)
    std::vector<vfs_trap_asset> assets; // pack: non-hole blocks, plan order
    std::vector<uint8_t> page_state;
    std::vector<uint32_t> page_pending; // absent blocks overlapping the page
    std::vector<uint8_t> page_stuck;    // pages whose zero-fill never landed
};

size_t os_page_size()
{
#if defined(_MSC_VER)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return size_t(info.dwPageSize);
#else
    const long size = sysconf(_SC_PAGESIZE);
    return size_t(size > 0 ? size : 4096);
#endif
}

const size_t s_page_size = os_page_size();

size_t trap_page_count(size_t size)
{
    return (size + s_page_size - 1) / s_page_size;
}

size_t trap_page_of(uint64_t offset)
{
    return size_t(offset / s_page_size);
}

// bytes of `m` the page holds (the last page is only partially backed)
uint64_t trap_page_span(size_t page, const vfs_trap_mapping &m)
{
    const uint64_t start = uint64_t(page) * s_page_size;
    const uint64_t end = start + s_page_size;
    return (end > m.size ? m.size : end) - start;
}

// ---------------------------------------------------------------------------
// os primitives: RW-backed shared mapping + per-page protection
// ---------------------------------------------------------------------------

#if defined(_MSC_VER)

uint8_t *os_map_rw(const fs_path &path, size_t *out_size)
{
    *out_size = 0;
    const auto &wide = fs_path_wstr(path);
    HANDLE file = CreateFileW(
        wide.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return nullptr;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0)
    {
        CloseHandle(file);
        return nullptr;
    }
    HANDLE map = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    CloseHandle(file);
    if (!map)
        return nullptr;
    // the view is write-backed (FILE_MAP_ALL_ACCESS): store writes through a
    // separate handle stay coherent with the mapping - the standard sparse
    // materialization technique on Windows
    void *view = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CloseHandle(map);
    if (!view)
        return nullptr;
    *out_size = size_t(size.QuadPart);
    return static_cast<uint8_t *>(view);
}

void os_unmap(uint8_t *base, size_t size)
{
    (void)size;
    UnmapViewOfFile(base);
}

bool os_protect(uint8_t *base, size_t size, DWORD protection)
{
    DWORD old = 0;
    return VirtualProtect(base, size, protection, &old) != 0;
}

bool os_protect_none(uint8_t *base, size_t size)
{
    return os_protect(base, size, PAGE_NOACCESS);
}

bool os_protect_read(uint8_t *base, size_t size)
{
    return os_protect(base, size, PAGE_READONLY);
}

bool os_protect_read_write(uint8_t *base, size_t size)
{
    return os_protect(base, size, PAGE_READWRITE);
}

#else // POSIX

uint8_t *os_map_rw(const fs_path &path, size_t *out_size)
{
    *out_size = 0;
    // O_RDWR (not O_RDONLY): the mapping must observe store writes made
    // through a separate fd (coherent via the page cache), and must stay
    // MAP_SHARED - never COW
    const int fd = open(fs_path_ascii(path), O_RDWR);
    if (fd < 0)
        return nullptr;
    struct stat st;
    memset(&st, 0, sizeof(st));
    size_t size = 0;
    uint8_t *base = nullptr;
    if (fstat(fd, &st) == 0 && st.st_size > 0)
    {
        size = size_t(st.st_size);
        void *mapped = mmap(
            nullptr,
            trap_page_count(size) * s_page_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0);
        if (mapped != MAP_FAILED)
            base = static_cast<uint8_t *>(mapped);
    }
    close(fd);
    if (!base)
        return nullptr;
    *out_size = size;
    return base;
}

void os_unmap(uint8_t *base, size_t size)
{
    munmap(base, trap_page_count(size) * s_page_size);
}

bool os_protect(uint8_t *base, size_t size, int protection)
{
    return mprotect(base, size, protection) == 0;
}

bool os_protect_none(uint8_t *base, size_t size)
{
    return os_protect(base, size, PROT_NONE);
}

bool os_protect_read(uint8_t *base, size_t size)
{
    return os_protect(base, size, PROT_READ);
}

bool os_protect_read_write(uint8_t *base, size_t size)
{
    return os_protect(base, size, PROT_READ | PROT_WRITE);
}

#endif // defined(_MSC_VER)

// ---------------------------------------------------------------------------
// module state
// ---------------------------------------------------------------------------

enum vfs_trap_counter
{
    trap_faults = 0,
    trap_resolved,
    trap_zeroed,
    trap_stubbed,
    trap_stub_served,
    trap_dedup_waits,
    trap_chained,
    trap_flips,
    trap_counter_count
};

std::atomic<uint64_t> s_counters[trap_counter_count] = {};
std::mutex s_mutex; // trap_lock (lock order: see file head)
std::condition_variable s_cv; // in-flight commit notifications
std::vector<std::unique_ptr<vfs_trap_mapping>> s_mappings;
uint32_t s_resolving = 0; // resolutions in flight across all mappings
vfs_trap_backend s_backend;
bool s_handler_installed = false;
bool s_report_counters = false;
bool s_zerolat = false; // zero-latency: faults serve stubs and enqueue demand fetches
// escape hatch (--streaming-trap=off): new mappings never protect pages
bool s_traps = true;

void trap_counter_inc(vfs_trap_counter which, uint64_t delta = 1)
{
    s_counters[which].fetch_add(delta, std::memory_order_relaxed);
}

uint64_t trap_counter_get(vfs_trap_counter which)
{
    return s_counters[which].load(std::memory_order_relaxed);
}

uint64_t trap_now_ms()
{
    return uint64_t(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// ---------------------------------------------------------------------------
// registry
// ---------------------------------------------------------------------------

// the registry is small (one entry per managed file), so faults pay a linear
// scan under trap_lock; containment is [base, base + size)
vfs_trap_mapping *trap_find(const uint8_t *addr)
{
    for (auto &mapping : s_mappings)
    {
        if (addr >= mapping->base && addr < mapping->base + mapping->size)
            return mapping.get();
    }
    return nullptr;
}

vfs_trap_mapping *trap_find_base(const uint8_t *base)
{
    for (auto &mapping : s_mappings)
    {
        if (mapping->base == base)
            return mapping.get();
    }
    return nullptr;
}

// pins a mapping against unmap for the duration of a resolution; call and
// release under trap_lock (unmap waits for the pin to drop - or zombifies -
// before munmap, so a resolver can never run against a freed mapping)
void trap_pin(vfs_trap_mapping *m)
{
    m->resolving++;
    s_resolving++;
}

void trap_unpin(vfs_trap_mapping *m)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (m->resolving == 0)
        return;
    m->resolving--;
    s_resolving--;
    if (m->resolving == 0)
        s_cv.notify_all();
}

// ---------------------------------------------------------------------------
// coverage updates (callers hold trap_lock)
// ---------------------------------------------------------------------------

// flips every fully-covered page overlapping [start, end) to readable; a
// page degraded to zeros is promoted to real when its last absent block
// lands - the MAP_SHARED mapping already shows the bytes the store writer put
// down (callers hold trap_lock)
void trap_flip_pages(vfs_trap_mapping &m, uint64_t start, uint64_t end)
{
    size_t page = trap_page_of(start);
    const size_t last = trap_page_of(end > 0 ? end - 1 : 0);
    for (; page <= last && page < m.page_state.size(); ++page)
    {
        if (m.page_pending[page] != 0 || m.page_state[page] == trap_page_readable)
            continue;
        const size_t len = size_t(trap_page_span(page, m));
        if (!os_protect_read(m.base + page * s_page_size, len))
            continue; // stays protected: the next fault re-runs resolution
        if (m.page_state[page] == trap_page_none)
            trap_counter_inc(trap_flips);
        m.page_state[page] = trap_page_readable;
        m.page_stuck[page] = 0;
    }
}

void trap_mark_present(vfs_trap_mapping &m, size_t asset_index)
{
    auto &asset = m.assets[asset_index];
    asset.state = trap_asset_present;
    const size_t first = trap_page_of(asset.start);
    const size_t last = trap_page_of(asset.end > 0 ? asset.end - 1 : 0);
    for (size_t page = first; page <= last && page < m.page_pending.size(); ++page)
    {
        if (m.page_pending[page] > 0)
            m.page_pending[page]--;
    }
    trap_flip_pages(m, asset.start, asset.end);
}

// true when every byte of [start, end) sits on a readable page
bool trap_range_covered(const vfs_trap_mapping &m, uint64_t start, uint64_t end)
{
    if (m.sealed)
        return true;
    size_t page = trap_page_of(start);
    const size_t last = trap_page_of(end > 0 ? end - 1 : 0);
    for (; page <= last && page < m.page_state.size(); ++page)
    {
        if (m.page_state[page] == trap_page_none)
            return false;
    }
    return true;
}

// first asset whose range may cover `offset` (assets are ascending and
// non-overlapping in the planned layout)
size_t trap_first_candidate(const vfs_trap_mapping &m, uint64_t offset)
{
    size_t low = 0, high = m.assets.size();
    while (low < high)
    {
        const size_t mid = low + (high - low) / 2;
        if (m.assets[mid].end <= offset)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

// rewrites the pack's on-disk entry as a stored (Flags=0) block of the stub
// bytes actually placed, so the asset becomes visible to UopLoadFile on the
// next boot (mid-session tables are already parsed and need no patch). The
// entry keeps the plan's DecompressedSize/Hash/Checksum and advertises only
// the stored span, so the presence check still reports the asset absent
// and a real blob heals it (the state DB stays the source of truth).
void trap_stub_patch_entry(vfs_trap_mapping &m, size_t index, uint32_t stored_len, size_t fill_page)
{
    if (!m.is_pack)
        return;
    const auto &trap_asset = m.assets[index];
    const auto &asset = m.plan->files[m.file_index].assets[trap_asset.plan_index];
    UopFileEntry entry;
    entry.Offset = asset.file_offset;
    entry.MetadataSize = asset.ml;
    entry.CompressedSize = stored_len;
    entry.DecompressedSize = uint32_t(asset.ul);
    entry.Hash = asset.uop_hash;
    entry.Checksum = asset.mc;
    entry.Flags = 0;
    // the record may straddle a page boundary: flip [pos, pos+size) writable
    const uint64_t pos = sizeof(UopHeader) + sizeof(UopSection) +
                         sizeof(UopFileEntry) * trap_asset.plan_index;
    if (pos + sizeof(entry) > m.table_end)
        return;
    const size_t first = trap_page_of(pos);
    const size_t last = trap_page_of(pos + sizeof(entry) - 1);
    for (size_t page = first; page <= last && page < m.page_state.size(); ++page)
    {
        const size_t len = size_t(trap_page_span(page, m));
        if (!os_protect_read_write(m.base + page * s_page_size, len))
            return; // leave the entry untouched: the asset stays invisible
    }
    memcpy(m.base + pos, &entry, sizeof(entry));
    for (size_t page = first; page <= last && page < m.page_state.size(); ++page)
    {
        const size_t len = size_t(trap_page_span(page, m));
        // a table page can also carry an absent block's payload: restore its
        // guard, or the patch would leave those bytes readable and the block
        // would silently read sparse zeros instead of healing on demand
        // (the page being filled is handled by the caller's own flip)
        if (page != fill_page && m.page_state[page] == trap_page_none)
            os_protect_none(m.base + page * s_page_size, len);
        else
            os_protect_read(m.base + page * s_page_size, len);
    }
}

// serves the still-absent sub-ranges of every protected page overlapping
// [start, end) with the file kind's stub payload (plain zeros when the
// kind has no payload content or the payload cannot fit), preserving
// entry-table and present-block bytes. Stubs only ever occupy the block's
// planned stored span ([start + ml, start + ml + min(ul, stored)): the
// decompressed readers take the memcpy path of DecompressBlock, which pads
// the tail with zeros, and raw-pointer readers never walk past what the
// header short-circuits. Deterministic generators write straight into the
// mapping: no allocation, and the cost is bounded by the page - the same
// guarantees the plain zero-fill path gave. Per-asset stub transitions are flagged
// (`stubbed`) and reported by trap_report_stubs once, with no trap lock
// held. Entry-table patches are deferred until the page is readable again:
// patching mid-fill would restore protection under the writer's feet.
// Transient mprotect failures (memory-pressure ENOMEM) are retried; a
// persistent failure leaves the page protected but marks it zeroed and
// stuck, so the next fault chains instead of refault-looping (no
// infinite loop, no process kill on a transient failure).
void trap_stub_fill(vfs_trap_mapping &m, uint64_t start, uint64_t end)
{
    size_t page = trap_page_of(start);
    const size_t last = trap_page_of(end > 0 ? end - 1 : 0);
    for (; page <= last && page < m.page_state.size(); ++page)
    {
        if (m.page_state[page] != trap_page_none)
            continue;
        const size_t page_offset = page * s_page_size;
        const size_t len = size_t(trap_page_span(page, m));
        uint8_t *const at = m.base + page_offset;
        struct vfs_trap_patch
        {
            size_t index;
            uint32_t stored_len;
        };
        vfs_trap_patch patches[VFS_TRAP_BATCH];
        size_t patch_count = 0;
        bool flipped = false;
        size_t page_nonzero = 0;
        for (int attempt = 0; attempt < VFS_TRAP_ZERO_ATTEMPTS && !flipped; ++attempt)
        {
            if (attempt > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(VFS_TRAP_ZERO_RETRY_MS));
            if (!os_protect_read_write(at, len))
                continue;
            patch_count = 0;
            page_nonzero = 0;
            for (size_t i = trap_first_candidate(m, page_offset); i < m.assets.size(); ++i)
            {
                auto &trap_asset = m.assets[i];
                if (trap_asset.start >= page_offset + len)
                    break;
                if (trap_asset.state == trap_asset_present)
                    continue;
                if (trap_asset.stubbed == 0)
                {
                    // the payload lives behind the block's metadata and can
                    // never exceed the block's planned stored span
                    const auto &asset =
                        m.plan->files[m.file_index].assets[trap_asset.plan_index];
                    const uint64_t payload = trap_asset.start + asset.ml;
                    const uint64_t stored =
                        trap_asset.end > payload ? trap_asset.end - payload : 0;
                    const uint64_t capacity =
                        stored < uint64_t(asset.ul) ? stored : uint64_t(asset.ul);
                    if (capacity >= vfs_stub_min_payload(m.stub_kind))
                    {
                        const uint64_t from =
                            payload > page_offset ? payload : page_offset;
                        const uint64_t to = payload + capacity < page_offset + len
                                                ? payload + capacity
                                                : page_offset + len;
                        if (to > from)
                        {
                            page_nonzero += vfs_stub_fill_slice(
                                m.stub_kind,
                                asset.ul,
                                m.is_pack,
                                from - payload,
                                at + size_t(from - page_offset),
                                size_t(to - from));
                        }
                        if (patch_count < VFS_TRAP_BATCH)
                        {
                            patches[patch_count].index = i;
                            patches[patch_count].stored_len = uint32_t(capacity);
                            ++patch_count;
                        }
                        // more than VFS_TRAP_BATCH blocks sharing one page is
                        // a degenerate layout: the surplus keeps a zeroed
                        // entry (the asset simply stays invisible)
                    }
                    trap_asset.stubbed = 1;
                }
            }
            flipped = os_protect_read(at, len);
        }
        if (!flipped)
        {
            // pragmatic corner: this logs from the fault path, but only
            // after retries against a persistent os failure
            LOG_ERROR("vfs_trap: stub fill stuck (mprotect failed) on %s", fs_path_ascii(m.path));
            m.page_state[page] = trap_page_zeroed;
            m.page_stuck[page] = 1;
            continue;
        }
        // the page is readable again: entry patches may flip table pages now
        for (size_t p = 0; p < patch_count; ++p)
            trap_stub_patch_entry(m, patches[p].index, patches[p].stored_len, page);
        m.page_state[page] = page_nonzero != 0 ? trap_page_stubbed : trap_page_zeroed;
        trap_counter_inc(trap_flips);
    }
}

// one covering block handed to the backend for resolution (definition below;
// the fill helper above only flags transitions)
void trap_block_of(const vfs_trap_mapping &m, size_t asset_index, vfs_trap_block *out);

// hands every not-yet-reported stub transition overlapping [start, end) to
// the backend - `stubbed` (state DB mark, counters) always and `prefetch`
// after zero-latency serves - with no trap lock held
void trap_report_stubs(vfs_trap_mapping &m, uint64_t start, uint64_t end, bool prefetch)
{
    if (s_backend.stubbed == nullptr && (!prefetch || s_backend.prefetch == nullptr))
        return;
    for (;;)
    {
        vfs_trap_block blocks[VFS_TRAP_BATCH];
        size_t count = 0;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            for (size_t i = trap_first_candidate(m, start); i < m.assets.size(); ++i)
            {
                auto &trap_asset = m.assets[i];
                if (trap_asset.start >= end)
                    break;
                if (trap_asset.stubbed == 0 || trap_asset.reported != 0)
                    continue;
                trap_asset.reported = 1;
                trap_block_of(m, i, &blocks[count]);
                if (++count == VFS_TRAP_BATCH)
                    break;
            }
        }
        for (size_t i = 0; i < count; ++i)
        {
            if (s_backend.stubbed != nullptr)
                s_backend.stubbed(s_backend.context, &blocks[i]);
            if (prefetch && s_backend.prefetch != nullptr)
                s_backend.prefetch(s_backend.context, &blocks[i]);
            trap_counter_inc(trap_stubbed);
        }
        if (count < VFS_TRAP_BATCH)
            break;
    }
}

// ---------------------------------------------------------------------------
// resolution (three phases, see file head)
// ---------------------------------------------------------------------------

void trap_block_of(const vfs_trap_mapping &m, size_t asset_index, vfs_trap_block *out)
{
    const auto &asset = m.plan->files[m.file_index].assets[m.assets[asset_index].plan_index];
    out->file_index = uint32_t(m.file_index);
    out->asset_index = m.assets[asset_index].plan_index;
    out->tail = asset.blob.c_str();
    out->expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
    out->is_loose = !m.is_pack;
}

// claims up to `cap` absent covering blocks (marks them in-flight); returns
// the claimed count. `waited` reports whether some covering block is
// in-flight in another thread (dedup wait) even when nothing was claimed.
size_t trap_claim(
    vfs_trap_mapping &m,
    uint64_t start,
    uint64_t end,
    uint32_t epoch,
    vfs_trap_block *blocks,
    uint32_t *claimed,
    size_t cap,
    bool *waited)
{
    size_t count = 0;
    for (size_t i = trap_first_candidate(m, start); i < m.assets.size(); ++i)
    {
        auto &asset = m.assets[i];
        if (asset.start >= end)
            break;
        if (asset.state == trap_asset_present)
            continue;
        if (asset.state == trap_asset_inflight ||
            (asset.state == trap_asset_absent && asset.attempt == epoch))
        {
            if (asset.state == trap_asset_inflight)
                *waited = true;
            continue;
        }
        asset.state = trap_asset_inflight;
        trap_block_of(m, i, &blocks[count]);
        claimed[count] = uint32_t(i);
        if (++count == cap)
            break;
    }
    return count;
}

// one synchronous resolution of [start, end); returns true when every
// covering byte is readable afterwards (present, or degraded to zeros)
bool trap_resolve_range(vfs_trap_mapping &m, uint64_t start, uint64_t end, bool allow_zero)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        m.resolve_epoch++;
    }
    const uint32_t epoch = m.resolve_epoch;
    const uint64_t deadline = trap_now_ms() + VFS_TRAP_WAIT_DEADLINE_MS;
    bool any_failed = false;
    for (;;)
    {
        vfs_trap_block blocks[VFS_TRAP_BATCH];
        uint32_t claimed[VFS_TRAP_BATCH];
        bool waited = false;
        size_t count = 0;
        {
            // phase 1: claim work under trap_lock
            std::lock_guard<std::mutex> lock(s_mutex);
            count = trap_claim(m, start, end, epoch, blocks, claimed, VFS_TRAP_BATCH, &waited);
        }
        if (count == 0)
        {
            bool covered = false;
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                covered = trap_range_covered(m, start, end);
            }
            if (covered || !waited || trap_now_ms() > deadline)
                break;
            // in-flight elsewhere: wait for the commit, then re-check (dedup)
            trap_counter_inc(trap_dedup_waits);
            std::unique_lock<std::mutex> lock(s_mutex);
            s_cv.wait_for(lock, std::chrono::milliseconds(VFS_TRAP_WAIT_DEADLINE_MS));
            continue;
        }
        // phase 2: backend call with no trap lock held (fetch + store writer)
        for (size_t i = 0; i < count; ++i)
        {
            const bool done =
                s_backend.ensure ? s_backend.ensure(s_backend.context, &blocks[i]) : false;
            // phase 3: commit + flip under trap_lock, wake dedup waiters
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                auto &asset = m.assets[claimed[i]];
                if (done)
                {
                    trap_mark_present(m, claimed[i]);
                }
                else
                {
                    asset.state = trap_asset_absent;
                    asset.attempt = epoch; // do not re-claim within this call
                }
            }
            s_cv.notify_all();
            any_failed |= !done;
        }
    }
    if (any_failed)
    {
        if (!allow_zero)
            return false;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            trap_stub_fill(m, start, end);
        }
        trap_report_stubs(m, start, end, false); // no demand fetch: default mode
        trap_counter_inc(trap_zeroed);
    }
    return true;
}

// initial presence: the entry table is the on-disk truth for packs (the
// store writer patches the block's Offset in place when the block lands); for
// loose files
// the state DB byte is the only truth (the sparse skeleton file matches the
// planned size but holds no content). Runs while every page is still
// readable, so the table reads cannot fault.
bool trap_asset_present_on_disk(
    const vfs_trap_mapping &m,
    size_t asset_index,
    const uint8_t *file_states)
{
    const auto &trap_asset_range = m.assets[asset_index];
    const auto &asset = m.plan->files[m.file_index].assets[trap_asset_range.plan_index];
    if (m.is_pack)
        return vfs_store_pack_asset_present(
            m.base, uint64_t(m.size), trap_asset_range.plan_index, asset);
    return file_states != nullptr && file_states[trap_asset_range.plan_index] == vfs_fetched;
}

// ---------------------------------------------------------------------------
// fault handling core
// ---------------------------------------------------------------------------

// a page whose zero-fill never landed (persistent mprotect failure): reads
// cannot be served, the caller must chain instead of refault-looping
bool trap_page_stuck_at(const vfs_trap_mapping &m, uint64_t offset)
{
    if (m.sealed)
        return false;
    const size_t page = trap_page_of(offset);
    return page < m.page_stuck.size() && m.page_stuck[page] != 0;
}

// address -> mapping -> covering page resolution; false when the address is
// not ours (caller chains to the previous handler)
bool trap_handle_fault(uint8_t *addr)
{
    vfs_trap_mapping *m = nullptr;
    uint64_t offset = 0;
    bool sealed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        m = trap_find(addr);
        if (!m)
            return false;
        offset = uint64_t(addr - m->base);
        if (offset >= m->size)
            return false; // exact end-of-file boundary: not ours
        trap_counter_inc(trap_faults);
        if (m->sealed)
        {
            sealed = true;
        }
        else if (trap_page_stuck_at(*m, offset))
        {
            return false; // degraded page: chain (anti-loop)
        }
        else
        {
            trap_pin(m); // alive until the resolution completes
        }
    }
    if (sealed)
    {
        // cannot happen (sealed files map plain readable); make it so anyway
        std::lock_guard<std::mutex> lock(s_mutex);
        if (os_protect_read(m->base, trap_page_count(m->size) * s_page_size))
        {
            memset(m->page_state.data(), trap_page_readable, m->page_state.size());
            trap_counter_inc(trap_flips, m->page_state.size());
        }
        return true;
    }
    // resolve the whole faulting page: page granularity is the flip unit, so
    // every block touching it must land before the instruction can proceed
    const size_t page = trap_page_of(offset);
    const uint64_t start = uint64_t(page) * s_page_size;
    const uint64_t len = trap_page_span(page, *m);
    if (s_zerolat)
    {
        // zero-latency rule: never wait on the network in the fault path -
        // serve the stub
        // payload right away, flip the page and enqueue the real fetch; the
        // heal arrives through the demand tier and the refresh hooks.
        // Counted as a placeholder serve, not a resolution: the bytes behind
        // the fault are synthetic (the per-asset stubbed counter above stays
        // the once-per-asset report)
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            m->resolve_epoch++;
            trap_stub_fill(*m, start, start + len);
        }
        trap_report_stubs(*m, start, start + len, true);
        trap_unpin(m);
        trap_counter_inc(trap_stub_served);
        return true;
    }
    const bool ok = trap_resolve_range(*m, start, start + len, true);
    trap_unpin(m);
    if (!ok)
        return false;
    trap_counter_inc(trap_resolved);
    return true;
}

// ---------------------------------------------------------------------------
// fault handlers
// ---------------------------------------------------------------------------

#if defined(_MSC_VER)

long NTAPI trap_veh(PEXCEPTION_POINTERS info)
{
    if (!info || !info->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    // ExceptionInformation[1] is the inaccessible address for both reads
    // ([0] == 0) and writes ([0] == 1); a write can never be satisfied by
    // resolving bytes - it would refault forever (livelock), so chain
    if (vfs_trap_fault_is_write(info->ExceptionRecord))
    {
        trap_counter_inc(trap_chained);
        return EXCEPTION_CONTINUE_SEARCH; // game bug: the default path crashes
    }
    auto *addr = reinterpret_cast<uint8_t *>(info->ExceptionRecord->ExceptionInformation[1]);
    if (trap_handle_fault(addr))
        return EXCEPTION_CONTINUE_EXECUTION; // the faulting access re-runs
    trap_counter_inc(trap_chained);
    return EXCEPTION_CONTINUE_SEARCH; // real bug: the default path crashes
}

#else // POSIX

struct sigaction s_previous_action;
bool s_previous_valid = false;

// chains to whatever was installed before us; the default action is restored
// and re-raised (never returns) so a foreign segfault still crashes
void trap_chain(int sig, siginfo_t *info, void *context)
{
    if (s_previous_valid && (s_previous_action.sa_flags & SA_SIGINFO))
    {
        s_previous_action.sa_sigaction(sig, info, context);
        return;
    }
    if (s_previous_valid && s_previous_action.sa_handler != SIG_DFL)
    {
        if (s_previous_action.sa_handler != SIG_IGN)
            s_previous_action.sa_handler(sig);
        return;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void trap_sigaction(int sig, siginfo_t *info, void *context)
{
    // protection faults (holes) arrive as SEGV_ACCERR; SEGV_MAPERR is for
    // addresses outside any mapping, which the range check rejects below
    if (info->si_code != SEGV_ACCERR && info->si_code != SEGV_MAPERR)
    {
        trap_counter_inc(trap_chained);
        trap_chain(sig, info, context);
        return;
    }
    // a write to a flipped (PROT_READ) page of our mapping is a game bug: no
    // resolution can satisfy it and resolving would livelock - restore the
    // pre-trap crash behavior
    if (vfs_trap_fault_is_write(context))
    {
        trap_counter_inc(trap_chained);
        trap_chain(sig, info, context);
        return;
    }
    if (trap_handle_fault(reinterpret_cast<uint8_t *>(info->si_addr)))
    {
        // the faulting instruction re-executes against the flipped page
        return;
    }
    trap_counter_inc(trap_chained);
    trap_chain(sig, info, context);
}

#endif // defined(_MSC_VER)

} // namespace

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

// raw fault-context discriminator: true when the context describes a
// WRITE access - POSIX: the ucontext_t* a SA_SIGINFO handler received (the
// REG_ERR write bit, x86-64); Windows: the EXCEPTION_RECORD* of an access
// violation (ExceptionInformation[0] == 1). Unknown/null contexts report a
// read: writes must be chained, never resolved (resolving a write livelocks)
bool vfs_trap_fault_is_write(void *context)
{
#if defined(_MSC_VER)
    const auto *record = static_cast<const EXCEPTION_RECORD *>(context);
    if (!record || record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return false;
    return record->ExceptionInformation[0] == 1;
#else
#if defined(REG_ERR)
    const auto *uc = static_cast<const ucontext_t *>(context);
    if (uc && (uc->uc_mcontext.gregs[REG_ERR] & 0x2))
        return true;
#endif
    return false;
#endif
}

void vfs_trap_set_backend(const vfs_trap_backend &backend)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_backend = backend;
}

void vfs_trap_set_zerolat(bool zerolat)
{
    s_zerolat = zerolat;
}

void vfs_trap_set_traps(bool enabled)
{
    s_traps = enabled;
}

void vfs_trap_stats_get(vfs_trap_stats *out)
{
    if (!out)
        return;
    out->faults = trap_counter_get(trap_faults);
    out->resolved = trap_counter_get(trap_resolved);
    out->zeroed = trap_counter_get(trap_zeroed);
    out->stubbed = trap_counter_get(trap_stubbed);
    out->stub_served = trap_counter_get(trap_stub_served);
    out->dedup_waits = trap_counter_get(trap_dedup_waits);
    out->chained = trap_counter_get(trap_chained);
    out->flips = trap_counter_get(trap_flips);
}

bool vfs_trap_install()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_handler_installed)
        return true;
#if defined(_MSC_VER)
    if (!AddVectoredExceptionHandler(1, reinterpret_cast<PVECTORED_EXCEPTION_HANDLER>(trap_veh)))
    {
        LOG_ERROR("vfs_trap: AddVectoredExceptionHandler failed");
        return false;
    }
#else
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = trap_sigaction;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    // deliver on the altstack when the process has one
    stack_t stack;
    if (sigaltstack(nullptr, &stack) == 0 && (stack.ss_flags & SS_DISABLE) == 0)
        action.sa_flags |= SA_ONSTACK;
    if (sigaction(SIGSEGV, &action, &s_previous_action) != 0)
    {
        LOG_ERROR("vfs_trap: sigaction(SIGSEGV) failed");
        return false;
    }
    s_previous_valid = true;
#endif
    s_report_counters = getenv("VFS_FAULT_COUNTERS") != nullptr;
    s_handler_installed = true;
    LOG_INFO("vfs_trap: fault handler installed");
    return true;
}

void vfs_trap_shutdown()
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (!s_handler_installed)
        return;
#if defined(_MSC_VER)
    RemoveVectoredExceptionHandler(reinterpret_cast<PVOID>(trap_veh));
#else
    if (s_previous_valid)
        sigaction(SIGSEGV, &s_previous_action, nullptr);
    s_previous_valid = false;
#endif
    s_handler_installed = false;
    // bounded wait for in-flight resolutions: they hold pins against the
    // mappings they touch, and erasing a mapping under its resolver is a
    // use-after-free
    const uint64_t deadline = trap_now_ms() + VFS_TRAP_SHUTDOWN_WAIT_MS;
    while (s_resolving > 0 && trap_now_ms() <= deadline)
        s_cv.wait_for(lock, std::chrono::milliseconds(50));
    // idle mappings leave the registry (forgotten, never unmapped: the game
    // may still hold the base pointers at shutdown and unmap them through
    // CMappedFile::Unload after this point - the pages leak by design).
    // Mappings with resolutions still in flight turn zombie: the mapping
    // object and its pages stay alive so no resolver is left holding freed
    // state; they leak for the process teardown instead (see vfs_trap.h)
    for (size_t i = 0; i < s_mappings.size();)
    {
        auto &m = *s_mappings[i];
        if (m.resolving > 0)
        {
            m.zombie = true;
            LOG_ERROR(
                "vfs_trap: shutdown zombie for %s (%u resolution(s) still in flight)",
                fs_path_ascii(m.path),
                unsigned(m.resolving));
            ++i;
        }
        else
        {
            s_mappings.erase(s_mappings.begin() + ptrdiff_t(i));
        }
    }
    const bool report = s_report_counters;
    lock.unlock();
    if (report)
    {
        // drain point (never in the fault path): preallocated counters only
        vfs_trap_stats stats;
        vfs_trap_stats_get(&stats);
        printf(
            "vfs_trap: faults %" PRIu64 " resolved %" PRIu64 " zeroed %" PRIu64 " stubbed %" PRIu64
            " stub_served %" PRIu64 " dedup_waits %" PRIu64 " chained %" PRIu64 " flips %" PRIu64
            "\n",
            stats.faults,
            stats.resolved,
            stats.zeroed,
            stats.stubbed,
            stats.stub_served,
            stats.dedup_waits,
            stats.chained,
            stats.flips);
        fflush(stdout);
    }
}

uint8_t *vfs_trap_map(
    const fs_path &path,
    const VfsPlan &plan,
    size_t file_index,
    const uint8_t *file_states,
    size_t *out_size)
{
    if (out_size)
        *out_size = 0;
    if (file_index >= plan.files.size())
        return nullptr;
    size_t size = 0;
    uint8_t *base = os_map_rw(path, &size);
    if (!base)
        return nullptr;
    // escape hatch: no protection, so no fault handler is needed at all -
    // the process keeps its default SIGSEGV/VEH disposition untouched
    if (s_traps && !vfs_trap_install())
    {
        os_unmap(base, size);
        return nullptr;
    }

    auto owned = std::make_unique<vfs_trap_mapping>();
    vfs_trap_mapping &m = *owned;
    m.base = base;
    m.size = size;
    m.path = path;
    m.plan = &plan;
    m.file_index = file_index;
    const auto &file = plan.files[file_index];
    m.is_pack = file.is_uop;
    m.stub_kind = vfs_stub_kind_for(file.name.c_str());

    // per-asset coverage model: packs cover [offset, offset + ml + payload)
    // (the whole block the store writer puts down); loose files resolve as one
    // whole-file range on first fault (HTTP Range fetch of the entire file;
    // idx-range streaming deferred)
    if (m.is_pack)
    {
        uint64_t table_end =
            sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * file.assets.size();
        m.table_end = table_end > size ? size : size_t(table_end);
        m.assets.reserve(file.assets.size());
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            const auto &asset = file.assets[ai];
            if (vfs_asset_is_hole(asset))
                continue; // reserved slot, no bytes, no coverage
            vfs_trap_asset range;
            range.plan_index = uint32_t(ai);
            range.start = asset.file_offset;
            range.end = asset.file_offset + vfs_asset_block_size(asset);
            range.expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
            m.assets.push_back(range);
        }
    }
    else
    {
        vfs_trap_asset range;
        range.plan_index = 0;
        range.start = 0;
        range.end = size; // coverage is the whole file: loose resolves whole
        range.expected = file.assets.empty() ? 0 : uint64_t(file.assets[0].cl);
        m.assets.push_back(range);
    }

    const size_t pages = trap_page_count(size);
    m.page_state.assign(pages, trap_page_none);
    m.page_pending.assign(pages, 0);
    m.page_stuck.assign(pages, 0);
    bool all_present = !m.assets.empty();
    for (size_t i = 0; i < m.assets.size(); ++i)
    {
        auto &asset = m.assets[i];
        asset.state =
            trap_asset_present_on_disk(m, i, file_states) ? trap_asset_present : trap_asset_absent;
        if (asset.state == trap_asset_absent)
        {
            all_present = false;
            const size_t first = trap_page_of(asset.start);
            const size_t last = trap_page_of(asset.end > 0 ? asset.end - 1 : 0);
            for (size_t page = first; page <= last && page < pages; ++page)
                m.page_pending[page]++;
        }
    }
    if (m.assets.empty())
        all_present = m.table_end >= size; // table-only pack (all holes)
    m.sealed = all_present;
    if (m.sealed)
        memset(m.page_state.data(), trap_page_readable, m.page_state.size());
    if (!s_traps && !m.sealed)
    {
        // trap-off: holes stay readable - pointer reads return the sparse
        // file's zeros; ensure/demand still fetch+flip (a no-op here)
        memset(m.page_state.data(), trap_page_readable, m.page_state.size());
    }

    // fault-safety ordering: the mapping enters the registry BEFORE the
    // first page is protected - the moment a hole becomes PROT_NONE a fault
    // can land, and it must find a resolvable mapping instead of chaining
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_mappings.push_back(std::move(owned)); // `m` stays valid (registry owns it)
    }

    if (!m.sealed && s_traps)
    {
        // every page with absent bytes stays protected; pages covered by the
        // entry table (and present blocks) map readable right away
        bool protect_failed = false;
        for (size_t page = 0; page < pages; ++page)
        {
            if (m.page_pending[page] != 0)
            {
                if (m.page_state[page] != trap_page_none)
                    continue; // a concurrent resolution settled this page already
                if (!os_protect_none(m.base + page * s_page_size, s_page_size))
                {
                    protect_failed = true;
                    break;
                }
            }
            else if (m.page_state[page] == trap_page_none)
            {
                m.page_state[page] = trap_page_readable;
            }
        }
        if (protect_failed)
        {
            LOG_ERROR("vfs_trap: could not protect page in %s", fs_path_ascii(path));
            bool leaked = false;
            std::unique_lock<std::mutex> lock(s_mutex);
            for (size_t i = 0; i < s_mappings.size(); ++i)
            {
                if (s_mappings[i].get() != &m)
                    continue;
                if (m.resolving > 0)
                {
                    // a resolver slipped in: keep the object alive (zombie)
                    m.zombie = true;
                    leaked = true;
                    LOG_ERROR("vfs_trap: map failure leaked %s (resolving)", fs_path_ascii(path));
                }
                else
                {
                    s_mappings.erase(s_mappings.begin() + ptrdiff_t(i));
                }
                break;
            }
            lock.unlock();
            if (!leaked)
                os_unmap(base, size);
            return nullptr;
        }
    }

    LOG_TRACE(
        "vfs_trap: mapped %s (%zu bytes, %zu assets, %s)",
        fs_path_ascii(path),
        size,
        m.assets.size(),
        m.sealed ? "sealed" : "streaming");

    if (out_size)
        *out_size = size;
    return base;
}

bool vfs_trap_unmap(uint8_t *base)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    for (size_t i = 0; i < s_mappings.size(); ++i)
    {
        if (s_mappings[i]->base != base)
            continue;
        auto &m = *s_mappings[i];
        if (m.zombie)
            return true; // already defunct: stays mapped for the process lifetime
        // wait for in-flight resolutions so the pages cannot outlive the
        // mapping; bounded: on expiry the mapping turns zombie instead of
        // being unmapped/erased under a live resolver
        const uint64_t deadline = trap_now_ms() + VFS_TRAP_WAIT_DEADLINE_MS;
        bool raced = m.resolving > 0;
        while (m.resolving > 0 && trap_now_ms() <= deadline)
            s_cv.wait_for(lock, std::chrono::milliseconds(50));
        if (m.resolving > 0)
        {
            m.zombie = true;
            LOG_ERROR(
                "vfs_trap: unmap zombie for %s (%u resolution(s) still in flight)",
                fs_path_ascii(m.path),
                unsigned(m.resolving));
            return true; // never hand a live range back for fs_unmap
        }
        if (raced)
        {
            // the last resolver dropped its pin just before returning from
            // the handler; give the faulting thread a grace period to
            // re-execute its instruction against the now-flipped page
            // before the pages disappear
            s_cv.wait_for(lock, std::chrono::milliseconds(VFS_TRAP_UNMAP_SETTLE_MS));
        }
        os_unmap(m.base, m.size);
        s_mappings.erase(s_mappings.begin() + ptrdiff_t(i));
        return true;
    }
    return false;
}

bool vfs_trap_contains(const uint8_t *base, uint64_t offset, uint64_t length)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto *m = trap_find_base(base);
    if (!m)
        return false;
    return offset < m->size && length <= m->size - offset;
}

bool vfs_trap_range_present(const uint8_t *base, uint64_t offset, uint64_t length)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto *m = trap_find_base(base);
    if (!m)
        return false;
    if (offset >= m->size)
        return false;
    const uint64_t end = offset + length > m->size ? m->size : offset + length;
    return trap_range_covered(*m, offset, end);
}

void vfs_trap_flip(uint8_t *base, uint64_t offset, uint64_t length)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    auto *m = trap_find_base(base);
    if (!m || m->sealed || offset >= m->size)
        return;
    const uint64_t end = offset + length > m->size ? m->size : offset + length;
    trap_flip_pages(*m, offset, end);
}

bool vfs_trap_resolve_range(uint8_t *base, uint64_t offset, uint64_t length)
{
    vfs_trap_mapping *m = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        m = trap_find_base(base);
        if (!m)
            return false;
        if (offset >= m->size)
            return false;
        if (m->sealed)
            return true;
        trap_pin(m);
    }
    const uint64_t end = offset + length > m->size ? m->size : offset + length;
    const bool ok = trap_resolve_range(*m, offset, end, true);
    trap_unpin(m);
    if (!ok)
        return false;
    trap_counter_inc(trap_resolved);
    return true;
}

vfs_trap_page_mark vfs_trap_page_mark_at(const uint8_t *base, uint64_t offset)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto *m = trap_find_base(base);
    if (!m || offset >= m->size)
        return vfs_trap_mark_none;
    if (m->sealed)
        return vfs_trap_mark_real;
    return vfs_trap_page_mark(m->page_state[trap_page_of(offset)]);
}

bool vfs_trap_asset_healed(const uint8_t *addr)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto *m = trap_find(addr);
    if (!m || m->sealed)
        return false;
    const uint64_t offset = uint64_t(addr - m->base);
    const size_t index = trap_first_candidate(*m, offset);
    if (index >= m->assets.size())
        return false;
    const auto &asset = m->assets[index];
    if (offset < asset.start || offset >= asset.end)
        return false;
    // the fetch landed, the writer put the real bytes down and the pages
    // flipped: the cached placeholder can be dropped on the next access
    return asset.stubbed != 0 && asset.state == trap_asset_present;
}

vfs_stub_kind vfs_trap_stub_kind_at(const uint8_t *addr)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto *m = trap_find(addr);
    if (!m)
        return vfs_stub_misc;
    return m->stub_kind;
}

bool vfs_trap_file_of(const uint8_t *base, const VfsPlan **out_plan, size_t *out_file_index)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const auto *m = trap_find_base(base);
    if (!m || m->zombie)
        return false;
    if (out_plan)
        *out_plan = m->plan;
    if (out_file_index)
        *out_file_index = m->file_index;
    return true;
}

bool vfs_trap_resolve_asset(const VfsPlan *plan, uint32_t file_index, uint32_t asset_index)
{
    if (plan == nullptr || file_index >= plan->files.size())
        return false;
    vfs_trap_mapping *m = nullptr;
    uint64_t start = 0, end = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (auto &mapping : s_mappings)
        {
            if (mapping->zombie || mapping->plan != plan || mapping->file_index != file_index)
                continue;
            for (size_t i = 0; i < mapping->assets.size(); ++i)
            {
                if (mapping->assets[i].plan_index != asset_index)
                    continue;
                m = mapping.get();
                start = mapping->assets[i].start;
                end = mapping->assets[i].end;
                break;
            }
            break;
        }
        if (m == nullptr)
            return false;
        if (m->sealed)
            return true;
        trap_pin(m);
    }
    // demand-tier context: blocking here is fine (no fault handler involved)
    const bool ok = trap_resolve_range(*m, start, end, true);
    trap_unpin(m);
    return ok;
}

size_t vfs_trap_page_size()
{
    return s_page_size;
}
