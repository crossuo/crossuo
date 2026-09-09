// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// fixture_gen: writes the synthetic mini product tree (test_product.h) to a
// directory for manual runs, e.g. a local xuoi --full against the fixture
// server:
//
//   python3 tests/vfs/http_server.py --root /tmp/vuoi_fixture &
//   fixture_gen /tmp/vuoi_fixture http://127.0.0.1:<port>/blobs/
//   xuoi --full --path /tmp/vuoi_install --product http://127.0.0.1:<port>/prod.xml
//
// argv[1] = output root, optional argv[2] = repo base url to patch in
// (defaults to the offline "*.test" urls of a plain test product)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "test_product.h"

// test_product.h reports failures through the vfs test runner; provide a
// stub for the standalone generator
bool vfs_test_add(const char *name, vfs_test_fn fn)
{
    (void)name;
    (void)fn;
    return true;
}

void vfs_test_fail(const char *file, int line, const char *format, ...)
{
    (void)file;
    (void)line;
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: fixture_gen <output-root> [repo-base-url]\n");
        return 1;
    }
    test_product_data prod;
    prod.root = fs_path_from(argv[1]);
    prod.blobs = fs_path_join(prod.root, "blobs");
    prod.install = fs_path_join(prod.root, "install");
    fs_path_create(prod.root);
    test_fill_product(prod);
    if (!test_write_product(prod))
    {
        fprintf(stderr, "fixture_gen: could not write the product\n");
        return 1;
    }
    if (argc >= 3)
    {
        const astr_t base = argv[2];
        if (!test_product_patch_repos(prod, base, base))
        {
            fprintf(stderr, "fixture_gen: could not patch the repo urls\n");
            return 1;
        }
    }
    printf("fixture: %s\n", fs_path_ascii(prod.root));
    return 0;
}
