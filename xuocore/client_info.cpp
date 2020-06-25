// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>
#if defined(NO_FS)
#include <sys/stat.h>
#else
#include <common/fs.h>
#endif // #if defined(NO_FS)

#include <external/xxhash.h>
#include <common/checksum.h>
#include <common/utils.h>

#include "client_info.h"
#include "client_db.h"

#define READ_U8(p) (((unsigned char *)(p))[0])
#define READ_U16(p) ((((unsigned char *)(p))[0]) | ((((unsigned char *)(p))[1]) << 8))
#define READ_U32(p)                                                                                \
    ((((unsigned char *)(p))[0]) | ((((unsigned char *)(p))[1]) << 8) |                            \
     ((((unsigned char *)(p))[2]) << 16) | ((((unsigned char *)(p))[3]) << 24))

static const unsigned char *find_version_data(const unsigned char *buf)
{
    if (READ_U16(buf) != 0x5A4D) // MZ IMAGE_DOS_HEADER
        return nullptr;

    const auto *inh = buf + READ_U32(buf + 0x3C); // PE IMAGE_NT_HEADERS32
    if (READ_U16(inh) != 0x4550)
        return nullptr;

    const auto *ifh = inh + 4; // IMAGE_FILE_HEADER
    const uint16_t numSections = READ_U16(ifh + 2);
    const uint16_t iohSize = READ_U16(ifh + 16);
    if (numSections == 0 || iohSize == 0)
        return nullptr;

    const auto *ioh = ifh + 20; // IMAGE_OPTIONAL_HEADER32
    const uint16_t magic = READ_U16(ioh);
    const auto *idd = ioh + (magic == 0x10b ? 96 : 112); // IMAGE_DATA_DIRECTORY 32/64
    const uint32_t va_res = READ_U32(idd + 8 * 2);
    const auto *ish = ioh + iohSize; // IMAGE_SECTION_HEADER
    for (int i = 0; i < numSections; ++i)
    {
        const auto *sec = ish + 40 * i; // IMAGE_SECTION_HEADER*
        char section[9] = {};
        memcpy(section, sec, sizeof(section));
        if (strcmp(section, ".rsrc") != 0)
            continue;

        const uint32_t va_sec = READ_U32(sec + 12);
        const auto *raw = buf + READ_U32(sec + 20);
        const auto *res_sec = raw + (va_res - va_sec);
        uint16_t num_named = READ_U16(res_sec + 12);
        uint16_t num_id = READ_U16(res_sec + 14);
        for (int j = 0; j < num_named + num_id; ++j)
        {
            // IMAGE_RESOURCE_DIRECTORY + IMAGE_RESOURCE_DIRECTORY_ENTRY[]
            const auto *res = res_sec + 16 + 8 * j;
            const uint32_t name = READ_U32(res);
            if (name != 16) // RT_VERSION
                continue;

            uint32_t offset = READ_U32(res + 4);
            if ((offset & 0x80000000) == 0) // valid resource
                return nullptr;

            // IMAGE_RESOURCE_DIRECTORY + IMAGE_RESOURCE_DIRECTORY_ENTRY[]
            const auto *ver_dir = res_sec + (offset & 0x7FFFFFFF);
            num_named = READ_U16(ver_dir + 12);
            num_id = READ_U16(ver_dir + 14);
            if (num_named == 0 && num_id == 0)
                return nullptr;

            res = ver_dir + 16;
            offset = READ_U32(res + 4);
            if ((offset & 0x80000000) == 0) // valid resource
                return nullptr;

            // IMAGE_RESOURCE_DIRECTORY + IMAGE_RESOURCE_DIRECTORY_ENTRY[]
            ver_dir = res_sec + (offset & 0x7FFFFFFF);
            num_named = READ_U16(ver_dir + 12);
            num_id = READ_U16(ver_dir + 14);
            if (num_named == 0 && num_id == 0)
                return nullptr;

            res = ver_dir + 16;
            offset = READ_U32(res + 4);
            if ((offset & 0x80000000) != 0) // valid resource
                return nullptr;

            ver_dir = res_sec + offset;
            const uint32_t verVa = READ_U32(ver_dir);
            const auto *verPtr = raw + (verVa - va_sec);
            return verPtr;
        }
        return nullptr;
    }
    return nullptr;
}

static int get_version(const unsigned char *version, int &offset)
{
#define PAD(x) (((x) + 3) & 0xFFFFFFFC)
    offset = PAD(offset);
    const uint16_t len = READ_U16(version + offset);
    offset += 2;
    const uint16_t val_len = READ_U16(version + offset);
    offset += 2;
    const uint16_t type = READ_U16(version + offset);
    offset += 2;

    char info[200] = {};
    int i = 0;
    for (; i < sizeof(info); ++i)
    {
        uint16_t c = READ_U16(version + offset);
        offset += 2;
        info[i] = checked_cast<char>(c);
        if (!c)
            break;
    }

    offset = PAD(offset);
    if (type != 0) // TEXT
    {
        for (i = 0; i < val_len; ++i)
        {
            // skip
            READ_U16(version + offset);
            offset += 2;
        }
    }
    else
    {
        if (strcmp(info, "VS_VERSION_INFO") == 0)
        {
            const auto *fixed = version + offset;
            // file
            //const uint16_t a = READ_U16(fixed + 10);
            //const uint16_t b = READ_U16(fixed + 8);
            //const uint16_t c = READ_U16(fixed + 14);
            //const uint16_t d = READ_U16(fixed + 12);
            // prod
            const uint16_t a = READ_U16(fixed + 18);
            const uint16_t b = READ_U16(fixed + 16);
            const uint16_t c = READ_U16(fixed + 22);
            const uint16_t d = READ_U16(fixed + 20);
            return VERSION(a, b, c, d);
        }
        offset += val_len;
    }

    while (offset < len)
    {
        get_version(version, offset);
    }
    offset = PAD(offset);
    return 0;
#undef PAD
}

void client_version_string(uint32_t version, char *output, int maxlen)
{
    int a = (version >> 24) & 0xff;
    int b = (version >> 16) & 0xff;
    int c = (version >> 8) & 0xff;
    int d = (version & 0xff);
    char p2[8];
    if (d >= 'a' && d <= 'z')
    {
        snprintf(p2, sizeof(p2), "%c", d);
    }
    else
    {
        snprintf(p2, sizeof(p2), ".%d", d);
    }
    snprintf(output, maxlen, "%d.%d.%d%s", a, b, c, p2);
}

int client_version(const char *filename, client_info &info)
{
    crc32_init();

#if defined(NO_FS)
    struct stat st;
    if (stat(filename, &st) < 0)
        return -1;

    const size_t len = st.st_size;
    auto *buf = (char *)malloc(len);
    FILE *f = fopen(filename, "rb");
    if (!f)
        return -2;

    fread(buf, 1, len, f);
    fclose(f);
#else
    fs_path file = fs_path_from(filename);
    if (!fs_path_exists(file))
        return -3;
    size_t len = 0;
    auto *buf = fs_map(file, &len);
    if (!len || !buf)
    {
        if (buf)
            fs_unmap(buf, len);
        return -4;
    }
#endif // defined(NO_FS)
    const auto *data = find_version_data(buf);
    if (data)
    {
        int offset = 0;
        info.version = get_version(data, offset);
    }
    info.xxh3 = XXH64(static_cast<void *>(buf), len, 0x2593);
    info.crc32 = crc32_checksum(buf, len);

    bool ret = 0;
    for (int i = 0; client_db[i].xxh3; ++i)
    {
        if (client_db[i].xxh3 != info.xxh3)
            continue;
        if (!info.version)
            info.version = client_db[i].version;
        ret = 1;
        break;
    }
#if defined(NO_FS)
    free(buf);
#else
    fs_unmap(buf, len);
#endif // defined(NO_FS)

    return ret;
}
