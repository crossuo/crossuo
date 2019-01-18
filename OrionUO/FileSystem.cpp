// GPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#include "FileSystem.h"

#if defined(ORION_WINDOWS)

void fs_case_insensitive_init(const os_path &path)
{
}

os_path fs_insensitive(const os_path &path)
{
    return path;
}

FILE *fs_open(const os_path &path_str, fs_mode mode)
{
    // we do not support text mode, any decent modern text editor can deal with it
    wstring m;
    m = mode & FS_WRITE ? m + L"w" : m;
    m = mode & FS_READ ? m + L"r" : m;
    m += L"b";

    FILE *f;
    _wfopen_s(&f, path_str.c_str(), m.c_str());
    return f;
}

void fs_close(FILE *fp)
{
    assert(fp);
    fclose(fp);
}

size_t fs_size(FILE *fp)
{
    assert(fp);
    return GetFileSize(fp, nullptr);
}

bool fs_path_exists(const os_path &path_str)
{
    return PathFileExistsW(path_str.c_str());
}

bool fs_path_create(const os_path &path_str)
{
    return CreateDirectoryW(path_str.c_str(), nullptr);
}

os_path fs_path_current()
{
    wstring path;
    path.reserve(MAX_PATH);
    GetCurrentDirectoryW(MAX_PATH, &path[0]);
    return path;
}

unsigned char *fs_map(const os_path &path, size_t *length)
{
    assert(length);
    unsigned char *ptr = nullptr;
    HANDLE map = 0;
    auto fd = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (fd == INVALID_HANDLE_VALUE)
        return nullptr;

    size_t size = GetFileSize(fd, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0)
        goto fail;

    map = CreateFileMappingA(fd, nullptr, PAGE_READONLY, 0, size, nullptr);
    if (!map)
        goto fail;

    ptr = (unsigned char *)MapViewOfFile(map, FILE_MAP_READ, 0, 0, size);
    CloseHandle(map);
fail:
    CloseHandle(fd);

    if (length)
        *length = size;
    return ptr;
}

void fs_unmap(unsigned char *ptr, size_t length)
{
    assert(ptr);
    UNUSED(length);
    UnmapViewOfFile(ptr);
}

#else

#include "Misc.h"

#include <stdio.h>
#include <string>
#include <utime.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <locale>
#include <codecvt>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

static std::vector<string> s_files;
static std::vector<string> s_lower;

string fs_insensitive(const string &path)
{
    string p = path;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    auto it = std::find(s_lower.begin(), s_lower.end(), p);
    if (it != s_lower.end())
    {
        int i = it - s_lower.begin();
        return s_files[i];
    }
    return path;
}

static void fs_list_recursive(const char *name)
{
    DIR *dir;
    struct dirent *entry;
    if (!(dir = opendir(name)))
    {
        return;
    }

    char path[512]{};
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            fs_list_recursive(path);
        }
        else
        {
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            int len = strlen(path);
            s_files.emplace_back(string(path, len));
            auto p = string(path, len);
            std::transform(p.begin(), p.end(), p.begin(), ::tolower);
            s_lower.emplace_back(p);
            //fprintf(stdout, ">> %s\n", p.c_str());
        }
    }
    closedir(dir);
}

void fs_case_insensitive_init(const string &path)
{
    s_files.reserve(1024);
    s_lower.reserve(1024);
    fs_list_recursive(path.c_str());
}

FILE *fs_open(const string &path_str, fs_mode mode)
{
    string m;
    m = (mode & FS_WRITE) != 0 ? m + "w" : m;
    m = (mode & FS_READ) != 0 ? m + "r" : m;

    const char *fname = path_str.c_str();
    const char *mstr = m.c_str();
    auto fp = fopen(fname, mstr);
    if (fp == nullptr)
    {
        LOG("Error loading file: %s (%d)", strerror(errno), errno);
        return nullptr;
    }

    return fp;
}

void fs_close(FILE *fp)
{
    assert(fp != nullptr);
    fclose(fp);
}

size_t fs_size(FILE *fp)
{
    assert(fp != nullptr);
    auto pos = ftell(fp);
    fseek(fp, 0, SEEK_END);

    auto size = ftell(fp);
    fseek(fp, pos, SEEK_SET);

    return size;
}

bool fs_path_exists(const string &path_str)
{
    assert(!path_str.empty());
    struct stat buffer;
    return stat(path_str.c_str(), &buffer) == 0;
}

bool fs_path_create(const string &path_str)
{
    assert(!path_str.empty());

    if (fs_path_exists(path_str))
    {
        return false;
    }

    return mkdir(path_str.c_str(), 0777) == 0;
}

string fs_path_current()
{
    char *currdir = getcwd(0, 0);
    string path{ currdir };
    free(currdir);

    return path;
}

#define USE_MMAP 1
unsigned char *fs_map(const os_path &path, size_t *length)
{
    unsigned char *ptr = nullptr;
#if USE_MMAP
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return nullptr;
    }

    size_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0)
    {
        goto fail;
    }

    ptr = (unsigned char *)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        ptr = nullptr;
    }
fail:
    close(fd);
#else
    FILE *fd = fopen(path.c_str(), "rb");
    if (!fd)
        return nullptr;

    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd);
    if (size <= 0)
        goto fail;

    rewind(fd);
    ptr = (unsigned char *)malloc(size);
    if (!ptr)
        goto fail;

    if (fread(ptr, size, 1, fd) != 1)
    {
        free(ptr);
        ptr = nullptr;
    }
fail:
    fclose(fd);
#endif

    if (length != nullptr)
    {
        *length = size;
    }
    return ptr;
}

void fs_unmap(unsigned char *ptr, size_t length)
{
    assert(ptr);
#if USE_MMAP
    munmap(ptr, length);
#else
    UNUSED(length);
    free(ptr);
#endif
}

#endif
