// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "shards.h"
#include <vector>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <external/inih.h>
#include "http.h"
#include "common.h"

extern bool valid_url(const std::string &url);
extern void open_url(const std::string &url);

namespace shard
{
#include "cfg_converters.h"

struct url_other
{
    std::string name;
    std::string type;
    std::string url;
};

std::string as_str(const url_other &in)
{
    return in.type + "+" + in.url;
}

bool convert(url_other &out, const char *raw)
{
    if (!raw || !raw[0])
        return true;

    auto v = split(raw, '+');
    if (v.size() != 2)
    {
        LOG_ERROR("parsing urlother field: '%s'", raw);
        return false;
    }

    if (!valid_url(v[1]))
    {
        LOG_ERROR("invalid url in urlother field: '%s'", v[1].c_str());
        return false;
    }

    out.type = v[0];
    out.name = "Goto: " + v[0];
    out.url = v[1];
    return true;
}

enum class tag_install_type : uint8_t
{
    website,
    download_zip,
    download_exe,
    download_web,
    uo_latest,
    uo_patch_server,
    xuo_patch_server,
};

struct tag_install
{
    std::string url;
    tag_install_type type;
};

std::string as_str(const tag_install &in)
{
    std::string r = "uo+latest";
    switch (in.type)
    {
        case tag_install_type::website:
            r = "website+" + in.url;
            break;
        case tag_install_type::download_zip:
            r = "download+zip+" + in.url;
            break;
        case tag_install_type::download_web:
            r = "download+web+" + in.url;
            break;
        case tag_install_type::download_exe:
            r = "download+exe+" + in.url;
            break;
        case tag_install_type::uo_patch_server:
            r = "uo+" + in.url;
            break;
        case tag_install_type::xuo_patch_server:
            r = "xuo+" + in.url;
            break;
        case tag_install_type::uo_latest:
            break;
    }
    return r;
}

bool convert(tag_install &out, const char *raw)
{
    if (!raw || !raw[0])
        return true;

    auto v = split(raw, '+');
    const auto size = int(v.size());
    if (size < 2 || size > 3)
    {
        LOG_ERROR("parsing installer field (%d): '%s'", size, raw);
        return false;
    }

    auto type = v[0];
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    auto sub = v[1];
    std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
    auto url = v[size - 1];
    bool latest = sub == "latest";
    if (!latest && !valid_url(url))
    {
        LOG_ERROR("invalid url in installer field: '%s'", url.c_str());
        return false;
    }

    if (type == "website")
        out.type = tag_install_type::website;
    if (type == "download")
    {
        if (sub == "exe")
            out.type = tag_install_type::download_exe;
        else if (sub == "zip")
            out.type = tag_install_type::download_zip;
        else if (sub == "web")
            out.type = tag_install_type::download_web;
        else
        {
            LOG_ERROR("invalid download type in installer field: '%s'", sub.c_str());
            return false;
        }
    }
    if (type == "uo")
    {
        if (latest)
        {
            out.type = tag_install_type::uo_latest;
            url = "";
        }
        else
            out.type = tag_install_type::uo_patch_server;
    }
    if (type == "xuo")
        out.type = tag_install_type::xuo_patch_server;
    out.url = url;
    return true;
}

struct tag_data
{
    std::vector<std::string> tags;
    bool is_highlighted;
    bool is_pvp;
    bool is_rp;
};

std::string as_str(const tag_data &in)
{
    // FIXME: default value is being lost somewhere in the way to the save
    auto d = in;
    if (d.tags.empty())
        d.tags.push_back("pvp");
    return join(d.tags, ',');
}

bool convert(tag_data &out, const char *raw)
{
    convert(out.tags, raw, ',');
    auto has_tag = [](auto &v, const char *tag) {
        return (std::find(v.begin(), v.end(), tag) != v.end());
    };
    out.is_highlighted = has_tag(out.tags, "highlighted");
    out.is_pvp = has_tag(out.tags, "pvp");
    out.is_rp = has_tag(out.tags, "rp");
    return true;
}

struct lang_type
{
    std::vector<std::string> langs;
};

std::string as_str(const lang_type &in)
{
    return join(in.langs, ',');
}

bool convert(lang_type &out, const char *raw)
{
    convert(out.langs, raw, ',');
    return true;
}

}; // namespace shard

#define CFG_NAME shard
#define CFG_DEFINITION "cfg_shard.h"
#include "cfg_loader.h"

// model

static shard::data s_shards;
static std::unordered_map<std::string, int> s_shard_by_loginserver;

bool shard_getter(void *data, int idx, const char **out_text)
{
    auto *items = (std::vector<shard::entry> *)data;
    assert(items);
    assert(idx < items->size());
    if (out_text)
        *out_text = items->at(idx).shard_name.c_str();
    return true;
}

static void load_shard_file(const fs_path &fname, shard::data &data)
{
    LOG_INFO("loading shards from %s", fs_path_ascii(fname));
    auto fp = fs_open(fname, FS_READ);
    if (!fp)
        return;
    shard::cfg(fp, data);
    fs_close(fp);
}

void load_shards()
{
    s_shards = shard::data();
    auto custom = shard::default_entry();
    custom.shard_name = "<custom shard>";
    s_shards.entries.emplace_back(custom);
    const auto fname = fs_path_join(fs_path_current(), "shards.cfg");
#if !defined(VALIDATOR)
    http_get_file(
        "https://github.com/crossuo/shards/releases/download/latest/shards.cfg",
        fs_path_ascii(fname));
#endif
    load_shard_file(fname, s_shards);

    // sort by tags 'highlight', then name alphabetically
    std::sort(s_shards.entries.begin(), s_shards.entries.end(), [](const auto &a, const auto &b) {
        const auto ah = a.shard_tags.is_highlighted;
        const auto bh = b.shard_tags.is_highlighted;
        if (ah != bh)
            return ah && !bh;
        return a.shard_name < b.shard_name;
    });

    int i = 0;
    for (auto &e : s_shards.entries)
    {
        s_shard_by_loginserver[e.shard_loginserver] = i++;
        shard::dump(&e);
    }
}

void write_shards(void *_fp)
{
    if (s_shards.entries.size() < 2) // <custom> shard is first
        return;

    auto fp = (FILE *)_fp;
    for (int i = 1; i < s_shards.entries.size(); ++i)
    {
        const auto &e = s_shards.entries[i];
        shard::write(fp, e, "Shard");
    }
}

int shard_index_by_loginserver(const char *login_server)
{
    auto it = s_shard_by_loginserver.find(login_server);
    if (it != s_shard_by_loginserver.end())
        return (*it).second;
    return 0;
}

shard_data shard_by_id(int id)
{
    assert(id < s_shards.entries.size());
    const auto &s = s_shards.entries[id];
    return { s.shard_loginserver.c_str(),
             s.shard_clienttype.c_str(),
             s.shard_clientversion.c_str() };
}

#if defined(VALIDATOR)

#include <external/popts.h>

static void print_banner()
{
    fprintf(stdout, "shardchk - crossuo launcher shard validator 0.0.1\n");
    fprintf(stdout, "Copyright (c) 2019 Danny Angelo Carminati Grein\n");
    fprintf(
        stdout, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n");
    fprintf(stdout, "\n");
}

static po::parser s_cli;
static bool init_cli(int argc, char *argv[])
{
    s_cli["help"].abbreviation('h').description("print this help screen");
    s_cli["path"].abbreviation('p').type(po::string).description("path");
    s_cli(argc, argv);
    return s_cli["help"].size() == 0;
}

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv) || !s_cli["path"].was_set())
    {
        print_banner();
        s_cli.print_help(std::cout);
        return 0;
    }

    std::vector<shard::entry> entries;
    entries.emplace_back(shard::default_entry());

    std::vector<fs_path> configs;
    fs_path_list(fs_path_from(s_cli["path"].get().string), configs);
    bool error = false;
    for (const auto &cfg : configs)
    {
        shard::data shard;
        shard.ignore_empty_on_save = true;

        LOG_INFO("validating '%s'", fs_path_ascii(cfg));
        load_shard_file(cfg, shard);
        error |= shard.error;
        if (shard.entries.size() != 1)
        {
            LOG_ERROR("invalid file, contains multiple entries.");
            error = true;
        }
        for (const auto &e : shard.errors)
            LOG_ERROR("%s", e.c_str());

        auto entry = shard.entries[0];
        if (entry.shard_name.empty())
        {
            LOG_ERROR("missing required field 'name'");
            error = true;
        }
        if (entry.shard_servertype.empty())
        {
            LOG_ERROR("missing required field 'servertype'");
            error = true;
        }
        /*if (entry.shard_url.empty())
        {
            LOG_ERROR("missing required field 'url'");
            error = true;
        }*/
        if (entry.shard_loginserver.empty())
        {
            LOG_ERROR("missing required field 'loginserver'");
            error = true;
        }
        if (entry.shard_clientversion.empty())
        {
            LOG_ERROR("missing required field 'clientversion'");
            error = true;
        }

        if (!error)
            entries.insert(entries.end(), shard.entries.begin(), shard.entries.end());
    }

    if (!error)
    {
        s_shards.entries = entries;
        std::sort(
            s_shards.entries.begin(), s_shards.entries.end(), [](const auto &a, const auto &b) {
                return a.shard_name < b.shard_name;
            });

        auto fp = fopen("shards.cfg", "wb");
        write_shards(fp);
        fclose(fp);
    }

    return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif // defined(VALIDATOR)