// GPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#pragma once

#include <stdio.h>
#include "config.h"

#if defined(XUO_WINDOWS)
#define os_path wstring
#define ToPath(x) ToWString(x)
#define StringFromPath(x) ToString(x)
#define CStringFromPath(x) ToString(x).c_str()
#define PATH_SEP ToPath("/")
#else
#define os_path string
#define ToPath(x) x
#define StringFromPath(x) x
#define CStringFromPath(x) x.c_str()
#define PATH_SEP string("/")
#endif

enum fs_mode
{
    FS_READ = 0x01,
    FS_WRITE = 0x02,
};

void fs_case_insensitive_init(const os_path &path);
os_path fs_insensitive(const os_path &path);

FILE *fs_open(const os_path &path_str, fs_mode mode);
void fs_close(FILE *fp);
size_t fs_size(FILE *fp);

bool fs_path_exists(const os_path &path_str);
bool fs_path_create(const os_path &path_str);
os_path fs_path_current();

unsigned char *fs_map(const os_path &path, size_t *length);
void fs_unmap(unsigned char *ptr, size_t length);
