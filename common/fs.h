/*
The MIT License (MIT)
Copyright (c) 2017 Danny Angelo Carminati Grein
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef FS_HEADER
#define FS_HEADER

#include <stdio.h>
#include <stdint.h>
#include <string>

#ifndef FS_MAX_PATH
#ifdef MAX_PATH
#define FS_MAX_PATH MAX_PATH
#else
#define FS_MAX_PATH 512
#endif
#endif

enum fs_mode
{
    FS_READ = 0x01,
    FS_WRITE = 0x02,
};

enum fs_type
{
    FS_FILE = 0x00,
    FS_DIR = 0x01,
};

#if defined(_MSC_VER)
struct fs_path
{
    std::wstring real_path;
    mutable std::string temp_path;
};
#else
struct fs_path
{
    std::string real_path;
};
#endif // defined(_MSC_VER)

void fs_case_insensitive_init(const fs_path &path);
fs_path fs_insensitive(const fs_path &path);

fs_path fs_path_from(const std::string &s);
fs_path fs_path_from(const std::wstring &w);
fs_path fs_path_from(const char *w);
fs_path const &fs_path_from(fs_path const &p);

bool fs_path_equal(const fs_path &a, const fs_path &b);

const char *fs_path_ascii(const fs_path &path);
const std::wstring fs_path_wstr(const fs_path &path);
const std::string fs_path_str(const fs_path &path);
bool fs_path_empty(const fs_path &path);

FILE *fs_open(const fs_path &path, fs_mode mode);
void fs_close(FILE *fp);
size_t fs_size(FILE *fp);

fs_path fs_directory(const fs_path &path);
fs_type fs_path_type(const fs_path &path);
bool fs_path_is_dir(const fs_path &path);
bool fs_path_is_file(const fs_path &path);
bool fs_path_exists(const fs_path &path);
bool fs_path_create(const fs_path &path);
fs_path fs_path_current();

unsigned char *fs_map(const fs_path &path, size_t *length);
void fs_unmap(unsigned char *ptr, size_t length);

void fs_append(fs_path &target, const fs_path &other);

template <typename T>
fs_path fs_join_path(T t)
{
    return fs_path_from(t);
}

template <typename H, typename... T>
fs_path fs_join_path(H head, T... tail)
{
    auto r = fs_path_from(head);
    fs_append(r, fs_join_path(tail...));
    return r;
}

#if (defined(FS_IMPLEMENTATION_PRIVATE) || defined(FS_IMPLEMENTATION)) && !defined(FS_IMPLEMENTED)
#define FS_IMPLEMENTED

#ifdef FS_IMPLEMENTATION_PRIVATE
#define FS_PRIVATE static
#else
#define FS_PRIVATE
#endif

// if log.h was included before we try to use it, otherwise we do simple fprintf
#ifdef LOG_HEADER

#ifndef FS_LOG_ERROR
#define FS_LOG_ERROR LOG_ERROR
#endif

#else // LOG_HEADER

#ifndef FS_LOG_ERROR
#define FS_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__);
#endif

#endif // LOG_HEADER

#include <locale>
#include <codecvt>

static std::string fs_wstr_to_str(const std::wstring &wstr)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstr.c_str());
}

static std::wstring fs_str_to_wstr(const std::string &str)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str.c_str());
}

FS_PRIVATE fs_path const &fs_path_from(fs_path const &p)
{
    return p;
}

#if defined(_MSC_VER)

#include <Windows.h>
#include <Shlwapi.h>
#include <assert.h>
#include <algorithm>
#pragma comment(lib, "Shlwapi.lib")

struct fs_path;
enum fs_mode;

FS_PRIVATE fs_path fs_path_from(const std::string &s)
{
    return { fs_str_to_wstr(s), s };
}

FS_PRIVATE fs_path fs_path_from(const std::wstring &w)
{
    return { w, fs_wstr_to_str(w) };
}

FS_PRIVATE fs_path fs_path_from(const char *w)
{
    return { fs_str_to_wstr(w), w };
}

FS_PRIVATE const char *fs_path_ascii(const fs_path &path)
{
    return path.temp_path.c_str();
}

FS_PRIVATE const std::wstring fs_path_wstr(const fs_path &path)
{
    return path.real_path;
}

FS_PRIVATE const std::string fs_path_str(const fs_path &path)
{
    return path.temp_path;
}

FS_PRIVATE bool fs_path_empty(const fs_path &path)
{
    return path.real_path.empty();
}

FS_PRIVATE bool fs_path_equal(const fs_path &a, const fs_path &b)
{
    return a.real_path == b.real_path;
}

FS_PRIVATE void fs_append(fs_path &target, const fs_path &other)
{
    target.real_path += L"\\" + other.real_path;
    target.temp_path += "\\" + other.temp_path;
}

FS_PRIVATE void fs_case_insensitive_init(const fs_path &path)
{
}

FS_PRIVATE fs_path fs_insensitive(const fs_path &path)
{
    return path;
}

FS_PRIVATE FILE *fs_open(const fs_path &path, fs_mode mode)
{
    // we do not support text mode, any decent modern text editor can deal with it
    std::wstring m;
    m = mode & FS_WRITE ? m + L"w" : m;
    m = mode & FS_READ ? m + L"r" : m;
    m += L"b";

    const auto &p = fs_path_wstr(path);

    FILE *f;
    _wfopen_s(&f, p.c_str(), m.c_str());
    return f;
}

FS_PRIVATE void fs_close(FILE *fp)
{
    assert(fp);
    fclose(fp);
}

FS_PRIVATE size_t fs_size(FILE *fp)
{
    assert(fp);
    return GetFileSize(fp, nullptr);
}

FS_PRIVATE fs_path fs_directory(const fs_path &path)
{
    if (fs_path_exists(path) && fs_path_is_dir(path))
        return path;

    auto copy = path.real_path;
    std::transform(
        copy.begin(), copy.end(), copy.begin(), [](auto c) { return c == L'/' ? '\\' : c; });
    auto name = copy.c_str();
    auto *last = wcsrchr(name, L'\\');
    if (last != nullptr)
    {
        return fs_path_from(std::wstring(name, last - name));
    }

    return fs_path_from(name);
}

FS_PRIVATE fs_type fs_path_type(const fs_path &path)
{
    const auto &p = fs_path_wstr(path);
    auto attr = GetFileAttributesW(p.c_str());
    assert(attr != INVALID_FILE_ATTRIBUTES);
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? FS_DIR : FS_FILE;
}

FS_PRIVATE bool fs_path_exists(const fs_path &path)
{
    const auto &p = fs_path_wstr(path);
    const bool r = PathFileExistsW(p.c_str()) != 0u;
    return r;
}

FS_PRIVATE bool fs_path_create(const fs_path &path)
{
    const auto &p = fs_path_wstr(path);
    return CreateDirectoryW(p.c_str(), nullptr) != 0u;
}

FS_PRIVATE fs_path fs_path_current()
{
    wchar_t path[FS_MAX_PATH];
    GetCurrentDirectoryW(FS_MAX_PATH, &path[0]);
    return fs_path_from(path);
}

FS_PRIVATE unsigned char *fs_map(const fs_path &path, size_t *length)
{
    const auto &p = fs_path_wstr(path);

    assert(length);
    unsigned char *ptr = nullptr;
    HANDLE map = 0;
    auto fd = CreateFileW(
        p.c_str(),
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

    map = CreateFileMappingA(fd, nullptr, PAGE_READONLY, 0, DWORD(size), nullptr);
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

FS_PRIVATE void fs_unmap(unsigned char *ptr, size_t length)
{
    assert(ptr);
    (void)length;
    UnmapViewOfFile(ptr);
}

#else // defined(_MSC_VER)

#include <stdio.h>
#include <ctype.h>
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <algorithm>
#include <vector>

FS_PRIVATE fs_path fs_path_from(const std::string &s)
{
    return fs_path{ s };
}

FS_PRIVATE fs_path fs_path_from(const std::wstring &w)
{
    return { fs_wstr_to_str(w) };
}

FS_PRIVATE fs_path fs_path_from(const char *w)
{
    return { w };
}

FS_PRIVATE const char *fs_path_ascii(const fs_path &path)
{
    return path.real_path.c_str();
}

FS_PRIVATE const std::wstring fs_path_wstr(const fs_path &path)
{
    return fs_str_to_wstr(path.real_path);
}

FS_PRIVATE const std::string fs_path_str(const fs_path &path)
{
    return path.real_path;
}

FS_PRIVATE bool fs_path_empty(const fs_path &path)
{
    return path.real_path.empty();
}

FS_PRIVATE bool fs_path_equal(const fs_path &a, const fs_path &b)
{
    return a.real_path == b.real_path;
}

FS_PRIVATE void fs_append(fs_path &target, const fs_path &other)
{
    target.real_path += "/" + other.real_path;
}

static std::vector<fs_path> s_files;
static std::vector<fs_path> s_lower;

FS_PRIVATE fs_path fs_insensitive(const fs_path &path)
{
    auto p = path.real_path;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    auto it = std::find_if(s_lower.begin(), s_lower.end(), [&p](const auto &lower) {
        return p.compare(lower.real_path) == 0;
    });
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
            std::string p = path;
            s_files.emplace_back(fs_path_from(p));
            std::transform(p.begin(), p.end(), p.begin(), ::tolower);
            s_lower.emplace_back(fs_path_from(p));
        }
    }
    closedir(dir);
}

FS_PRIVATE void fs_case_insensitive_init(const fs_path &path)
{
    s_files.reserve(1024);
    s_lower.reserve(1024);
    fs_list_recursive(fs_path_ascii(path));
}

FS_PRIVATE FILE *fs_open(const fs_path &path, fs_mode mode)
{
    std::string m;
    m = (mode & FS_WRITE) != 0 ? m + "w" : m;
    m = (mode & FS_READ) != 0 ? m + "r" : m;

    const char *fname = fs_path_ascii(path);
    const char *mstr = m.c_str();
    auto fp = fopen(fname, mstr);
    if (fp == nullptr)
    {
        FS_LOG_ERROR("loading file: %s (%d)", strerror(errno), errno);
        return nullptr;
    }

    return fp;
}

FS_PRIVATE void fs_close(FILE *fp)
{
    assert(fp != nullptr);
    fclose(fp);
}

FS_PRIVATE size_t fs_size(FILE *fp)
{
    assert(fp != nullptr);
    auto pos = ftell(fp);
    fseek(fp, 0, SEEK_END);

    auto size = ftell(fp);
    fseek(fp, pos, SEEK_SET);

    return size;
}

FS_PRIVATE fs_path fs_directory(const fs_path &path)
{
    if (fs_path_exists(path) && fs_path_is_dir(path))
        return path;

    const char *name = fs_path_ascii(path);
    char *last = strrchr(name, '/');
    if (last != nullptr)
    {
        std::string dir(name, last - name);
        return { dir };
    }

    return { name };
}

FS_PRIVATE fs_type fs_path_type(const fs_path &path)
{
    assert(!path.real_path.empty());
    const char *name = fs_path_ascii(path);
    struct stat file;
    stat(name, &file);
    return (file.st_mode & S_IFREG) ? FS_FILE : FS_DIR;
}

FS_PRIVATE bool fs_path_exists(const fs_path &path)
{
    assert(!path.real_path.empty());
    struct stat buffer;
    auto r = stat(fs_path_ascii(path), &buffer) == 0;
    return r;
}

FS_PRIVATE bool fs_path_create(const fs_path &path)
{
    assert(!path.real_path.empty());
    if (fs_path_exists(path))
    {
        return true;
    }

    std::string copy = path.real_path.c_str();
    const char *segment = copy.data();
    const char *start = segment;
    do
    {
        char *sp = strchr(segment, '/');
        if (!sp)
            break;
        *sp = '\0';
        mkdir(start, 0777);
        *sp = '/';
        segment = sp + 1;
    } while (1);

    return mkdir(start, 0777) == 0;
}

FS_PRIVATE fs_path fs_path_current()
{
    char *currdir = getcwd(0, 0);
    fs_path path = fs_path_from(currdir);
    free(currdir);
    return path;
}

#define USE_MMAP 1
FS_PRIVATE unsigned char *fs_map(const fs_path &path, size_t *length)
{
    unsigned char *ptr = nullptr;
#if USE_MMAP
    int fd = open(fs_path_ascii(path), O_RDONLY);
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
    FILE *fd = fopen(fs_path_ascii(path), "rb");
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

FS_PRIVATE void fs_unmap(unsigned char *ptr, size_t length)
{
    assert(ptr);
#if USE_MMAP
    munmap(ptr, length);
#else
    UNUSED(length);
    free(ptr);
#endif
}

#endif // #if defined(_MSC_VER)

FS_PRIVATE bool fs_path_is_dir(const fs_path &path)
{
    return fs_path_type(path) == FS_DIR;
}

FS_PRIVATE bool fs_path_is_file(const fs_path &path)
{
    return fs_path_type(path) == FS_FILE;
}

#endif // FS_IMPLEMENTATION

#endif // FS_HEADER