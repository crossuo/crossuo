// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// Minimal test runner for the vfs tests: a registry of named test functions
// run in registration order. Plain checks, no test framework, no exceptions.

#include "test_main.h"

#include <stdarg.h>
#include <stdio.h>
#include <vector>

struct vfs_test
{
    const char *name;
    vfs_test_fn fn;
};

static std::vector<vfs_test> &vfs_tests()
{
    static std::vector<vfs_test> tests;
    return tests;
}

static bool s_current_failed = false;
static astr_t s_current_message;

bool vfs_test_add(const char *name, vfs_test_fn fn)
{
    vfs_tests().push_back({ name, fn });
    return true;
}

void vfs_test_fail(const char *file, int line, const char *format, ...)
{
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s:%d: ", file, line);
    char message[512] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message) - 1, format, args);
    va_end(args);
    s_current_failed = true;
    s_current_message = astr_t(prefix) + message;
}

int vfs_test_run_all()
{
    const auto &tests = vfs_tests();
    size_t failed = 0;
    for (const auto &test : tests)
    {
        s_current_failed = false;
        s_current_message.clear();
        test.fn();
        if (s_current_failed)
        {
            ++failed;
            printf("FAIL %s\n  %s\n", test.name, s_current_message.c_str());
        }
        else
        {
            printf("PASS %s\n", test.name);
        }
        fflush(stdout);
    }
    printf("%zu/%zu tests passed\n", tests.size() - failed, tests.size());
    return failed == 0 ? 0 : 1;
}

int main()
{
    return vfs_test_run_all();
}
