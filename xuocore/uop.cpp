// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "uop.h"
#include "enumlist.h" // rename this header
#include <external/lookup3.h>
#include <common/utils.h>
#include <common/str.h>
#include <unordered_map>

struct UopAssetName
{
    astr_t name;
    uint32_t id = 0;
};

using HashToNameId = std::unordered_map<uint64_t, UopAssetName>;
using PackageHashTable = std::unordered_map<astr_t, HashToNameId>;

static PackageHashTable s_package_hashes;

// FIXME: This should be interpreted from Asset::AssetMap
void uop_populate_asset_names()
{
    static const char *package[] = { "animationframe",  "gumpartlegacymul", "artlegacymul",
                                     "multicollection", "soundlegacymul",   "animationsequence" };

    static const char *pack[] = {
        "animationlegacyframe", "gumpartlegacymul", "artlegacymul",
        "multicollection",      "soundlegacymul",   "animationsequence",
    };
    static_assert(countof(pack) == countof(package), "missing pack entry");

    static const int package_size[] = {
        MAX_ANIMATIONS_DATA_INDEX_COUNT, MAX_GUMP_DATA_INDEX_COUNT,  MAX_STATIC_DATA_INDEX_COUNT,
        MAX_MULTI_DATA_INDEX_COUNT,      MAX_SOUND_DATA_INDEX_COUNT, MAX_ANIMATIONS_DATA_INDEX_COUNT
    };
    static_assert(countof(package_size) == countof(package), "missing package_size entry");

    static const int group_size[] = { MAX_ANIMATION_GROUPS_COUNT, 1, 1, 1, 1, 1 };
    static_assert(countof(group_size) == countof(package), "missing group_size entry");

    static const char *pattern[] = {
        "%06d/%02d.bin", "%08d.tga", "%08d.tga", "%06d.bin", "%08d.dat", "%08d.bin",
    };
    static_assert(countof(pattern) == countof(package), "missing pattern entry");

    static const char *format = "build/%s/%s";
    char build[200];
    char file[100];

    for (int p = 0; p < countof(package); ++p)
    {
        const auto package_name = package[p];
        auto &table = s_package_hashes[package_name];
        for (int asset = 0; asset < package_size[p]; ++asset)
        {
            for (int group = 0; group < group_size[p]; ++group)
            {
                snprintf(file, sizeof(file), pattern[p], asset, group);
                snprintf(build, sizeof(build), format, pack[p], file);
                uint32_t ph = 0, sh = 0;
                hashlittle2(build, strlen(build), &ph, &sh);
                const uint64_t hash = UOP_HASH(ph, sh);
                UopAssetName name;
                name.name = build;
                name.id = uint32_t(asset);
                table[hash] = name;
                //LOG_DEBUG("%s.uop: %s == %016" PRIx64 "", package_name, build, hash);
            }
        }
    }
}

const astr_t &uop_asset_name(const char *package, uint64_t hash, uint32_t *out_id)
{
    static astr_t not_found;
    if (!package)
    {
        return not_found;
    }

    char tmp[200];
    const auto len = strlen(package);
    assert(len < sizeof(tmp));
    memcpy(tmp, package, len);
    str_lower(tmp);

    const auto ext = strchr(tmp, '.');
    if (ext != nullptr)
    {
        *ext = '\0';
    }

    if (strstr(tmp, "animationframe") != nullptr)
    {
        tmp[strlen(tmp) - 1] = '\0'; // animationframe1..4 -> animationframe
    }

    const auto &pit = s_package_hashes.find(tmp);
    if (pit == s_package_hashes.end())
    {
        return not_found;
    }

    const auto &table = pit->second;
    const auto &it = table.find(hash);
    if (it == table.end())
    {
        return not_found;
    }

    const auto &obj = it->second;
    if (out_id)
    {
        *out_id = obj.id;
    }
    return obj.name;
}
