// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#pragma once

#include <string.h>

#include <external/lookup3.h>

#include <common/str.h>

typedef void (*vfs_test_fn)();

// ---------------------------------------------------------------------------
// shared protocol helpers (mirror the production scheme)
// ---------------------------------------------------------------------------

// Name hashing exactly as production does it (xuocore/mft.cpp:421): lowercase
// a copy of the name, then lookup3 hashlittle2. (ph, sh) are the two words the
// patch servers use in remote file names, "%08x%08x" (ph, sh).
inline void vfs_name_hash(const char *name, uint32_t &ph, uint32_t &sh)
{
    char tmp[512] = {};
    const size_t len = strlen(name);
    memcpy(tmp, name, len);
    str_lower(tmp);
    hashlittle2(tmp, len, &ph, &sh);
}

// The hash stored in UOP entry tables and looked up by
// CUopMappedFile::GetAsset (xuocore/uodata.cpp:216): (sh << 32) | ph.
inline uint64_t vfs_uop_hash(const char *name)
{
    uint32_t ph = 0, sh = 0;
    vfs_name_hash(name, ph, sh);
    return (static_cast<uint64_t>(sh) << 32) | ph;
}

// Registers a test function; used by the VFS_TEST macro at namespace scope.
bool vfs_test_add(const char *name, vfs_test_fn fn);

// Runs all registered tests, printing "PASS <name>" or "FAIL <name>" per test
// plus a one-line summary. Returns 0 when every test passed, nonzero otherwise.
int vfs_test_run_all();

// Records a failure for the currently running test (file:line + message).
void vfs_test_fail(const char *file, int line, const char *format, ...);

// Defines a named test. The body returns early on the first failed TEST_CHECK.
#define VFS_TEST(name)                                                                             \
    static void name();                                                                            \
    static const bool name##_added = vfs_test_add(#name, name);                                    \
    static void name()

// Plain check macros (no framework, no exceptions): on failure they record
// file:line + a message and return from the test body.
#define TEST_CHECK(cond)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            vfs_test_fail(__FILE__, __LINE__, "check failed: %s", #cond);                          \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define TEST_CHECK_MSG(cond, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            vfs_test_fail(__FILE__, __LINE__, __VA_ARGS__);                                        \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define TEST_CHECK_STR(actual, expected)                                                           \
    do                                                                                             \
    {                                                                                              \
        const astr_t actual_ = (actual);                                                           \
        const astr_t expected_ = (expected);                                                       \
        if (actual_ != expected_)                                                                  \
        {                                                                                          \
            vfs_test_fail(                                                                         \
                __FILE__,                                                                          \
                __LINE__,                                                                          \
                "check failed: \"%s\" != \"%s\"",                                                  \
                actual_.c_str(),                                                                   \
                expected_.c_str());                                                                \
            return;                                                                                \
        }                                                                                          \
    } while (0)
