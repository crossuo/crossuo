// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "uop.h"
#include "enumlist.h" // rename this header
#include <external/lookup3.h>
#include <common/utils.h>
#include <common/str.h>
#include <unordered_map>

struct UopAssetName
{
    std::string name;
    uint32_t id = 0;
};

using HashToNameId = std::unordered_map<uint64_t, UopAssetName>;
using PackageHashTable = std::unordered_map<std::string, HashToNameId>;

static PackageHashTable s_package_hashes;

void uop_populate_asset_names()
{
    static const char *package[] = {
        "animationframe",
        // TODO: animation sequence
        "gumpartlegacymul",
        "artlegacymul",
        "multicollection",
        "soundlegacymul",
    };

    static const char *pack[] = {
        "animationlegacyframe", "gumpartlegacymul", "artlegacymul",
        "multicollection",      "soundlegacymul",
    };

    static const int package_size[] = {
        MAX_ANIMATIONS_DATA_INDEX_COUNT, MAX_GUMP_DATA_INDEX_COUNT,  MAX_STATIC_DATA_INDEX_COUNT,
        MAX_MULTI_DATA_INDEX_COUNT,      MAX_SOUND_DATA_INDEX_COUNT,
    };

    static const int group_size[] = {
        MAX_ANIMATION_GROUPS_COUNT, 1, 1, 1, 1,
    };

    static const char *pattern[] = {
        "%06d/%02d.bin", "%08d.tga", "%08d.tga", "%06d.bin", "%08d.dat",
    };

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

const std::string &uop_asset_name(const char *package, uint64_t hash, uint32_t *out_id)
{
    static std::string not_found;
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
