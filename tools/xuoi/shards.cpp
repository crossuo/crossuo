// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include "shards.h"
#include <vector>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <external/inih.h>
#include <xuocore/http.h>
#include <xuocore/common.h>

namespace xuol {

extern const fs_path &xuol_data_path();

namespace shard
{
#include "cfg_converters.h"

struct tag_data
{
    std::vector<astr_t> tags;
    bool is_highlighted;
    bool is_pvp;
    bool is_rp;
};

astr_t as_str(const tag_data &in)
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

}; // namespace shard

bool url_is_discord(const astr_t &url)
{
    // a discord community link is detected from the url itself (the old
    // `Name+url` grammar carried the name); hosts are case-insensitive
    astr_t lower(url);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("discord.gg/") != astr_t::npos ||
           lower.find("discord.com/") != astr_t::npos;
}

#define CFG_NAME shard
#define CFG_DEFINITION "cfg_shard.h"
#include "cfg_loader.h"

// model

static shard::data s_shards;
static std::unordered_map<astr_t, int> s_shard_by_loginserver;

static astr_t s_shards_cfg_override; // set via xuol_set_shards_cfg, see shards.h

void xuol_set_shards_cfg(const astr_t &path)
{
    s_shards_cfg_override = path;
}

const char *shard_getter(void *data, int idx)
{
    auto *items = (std::vector<shard::entry> *)data;
    assert(items);
    assert(idx < (int)items->size());
    return items->at(idx).shard_shardname.c_str();
}

static void load_shard_file(const fs_path &fname, shard::data &data, const char *source = "")
{
    LOG_INFO("loading shards from %s%s", fs_path_ascii(fname), source);
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
    custom.shard_shardname = "<custom shard>";
    s_shards.entries.emplace_back(custom);
    fs_path fname;
    if (!s_shards_cfg_override.empty())
    {
        // a local list replaces the download; a missing/unopenable file
        // leaves fname empty and falls through to the online path below
        fname = fs_path_from(s_shards_cfg_override);
        if (!fs_path_exists(fname))
        {
            LOG_ERROR(
                "shards-cfg: cannot open '%s' - falling back to the online list",
                fs_path_ascii(fname));
            fname = fs_path();
        }
        else
        {
            load_shard_file(fname, s_shards, " (local)");
        }
    }
    if (fs_path_empty(fname))
    {
#if !defined(VALIDATOR)
        fname = fs_path_join(xuol_data_path(), "shards.cfg");
        http_get_file(
            "https://raw.githubusercontent.com/crossuo/shards/release/shards.cfg",
            fs_path_ascii(fname));
#else
        fname = fs_path_join(fs_path_current(), "shards.cfg");
#endif
        load_shard_file(fname, s_shards);
    }

    // sort by tags 'highlight', then name alphabetically
    std::sort(
        s_shards.entries.begin() + 1, s_shards.entries.end(), [](const auto &a, const auto &b) {
            const auto ah = a.shard_tags.is_highlighted;
            const auto bh = b.shard_tags.is_highlighted;
            if (ah != bh)
                return ah && !bh;
            return a.shard_shardname < b.shard_shardname;
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
    for (size_t i = 1; i < s_shards.entries.size(); ++i)
    {
        const auto &e = s_shards.entries[i];
        shard::write(fp, e, "Shard");
    }
}

int shard_count()
{
    return (int)s_shards.entries.size();
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
    assert(id < (int)s_shards.entries.size());
    const auto &s = s_shards.entries[id];
    return { s.shard_shardname.c_str(),
             s.shard_loginserver.c_str(),
             s.shard_language.c_str(),
             s.shard_reportlanguage.c_str(),
             s.shard_seedoverride.c_str(),
             s.shard_systemcheckoverride.c_str(),
             s.shard_clienttype.c_str(),
             s.shard_clientversion.c_str(),
             s.shard_protocolclientversion.c_str(),
             s.shard_crypt,
             s.shard_autoacct,
             s.shard_shardweb.c_str(),
             s.shard_shardaccount.c_str(),
             s.shard_shardvideo.c_str(),
             s.shard_shardother,
             s.shard_shardcommunity,
             s.shard_producturl.c_str() };
}

} // namespace xuol

#if defined(VALIDATOR)

#include <external/popts.h>
using namespace xuol;

static void print_banner()
{
    fprintf(stdout, "shardchk - crossuo launcher shard validator 0.0.1\n");
    fprintf(stdout, "Copyright (c) 2020 Danny Angelo Carminati Grein\n");
    fprintf(
        stdout,
        "License AGPLv3+: GNU Affero GPL version 3 or later <http://gnu.org/licenses/agpl.html>.\n");
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
    // fs_path_list only walks directories: a plain file path would be
    // silently swallowed and the validator would pass without checking
    // anything - accept both single files and directories
    const auto seed = fs_path_from(s_cli["path"].get().string);
    if (fs_path_exists(seed) && !fs_path_is_dir(seed))
        configs.emplace_back(seed);
    else
        fs_path_list(seed, configs);
    bool error = false;
    for (const auto &cfg : configs)
    {
        shard::data shard;
        shard.ignore_empty_on_save = true;

        LOG_INFO("validating '%s'", fs_path_ascii(cfg));
        load_shard_file(cfg, shard);
        error |= shard.error;

        // a stray file (non-shard or comment-only) parses to zero entries:
        // nothing to validate here, skip it instead of touching entries[0].
        // not a validation failure, just nothing to validate.
        if (shard.entries.empty())
        {
            LOG_WARN("no [Shard] entry in '%s', skipping", fs_path_ascii(cfg));
            continue;
        }

        if (shard.entries.size() != 1)
        {
            LOG_ERROR("invalid file, contains multiple entries.");
            error = true;
        }
        for (const auto &e : shard.errors)
            LOG_ERROR("%s", e.c_str());

        auto entry = shard.entries[0];
        if (entry.shard_shardname.empty())
        {
            LOG_ERROR("missing required field 'shardname'");
            error = true;
        }
        // optional urls: only validated when non-empty, must be http(s)
        if (!entry.shard_shardweb.empty() && !valid_url(entry.shard_shardweb))
        {
            LOG_ERROR(
                "invalid field 'shardweb', not a http(s) url: '%s'", entry.shard_shardweb.c_str());
            error = true;
        }
        if (!entry.shard_shardcommunity.empty() && !valid_url(entry.shard_shardcommunity))
        {
            LOG_ERROR(
                "invalid field 'shardcommunity', not a http(s) url: '%s'",
                entry.shard_shardcommunity.c_str());
            error = true;
        }
        if (!entry.shard_shardaccount.empty() && !valid_url(entry.shard_shardaccount))
        {
            LOG_ERROR(
                "invalid field 'shardaccount', not a http(s) url: '%s'",
                entry.shard_shardaccount.c_str());
            error = true;
        }
        if (!entry.shard_shardvideo.empty() && !valid_url(entry.shard_shardvideo))
        {
            LOG_ERROR(
                "invalid field 'shardvideo', not a http(s) url: '%s'",
                entry.shard_shardvideo.c_str());
            error = true;
        }
        // producturl/shardother are plain urls: empty is fine (manual install /
        // no community link), a non-empty value must be a http(s) url
        if (!entry.shard_shardother.empty() && !valid_url(entry.shard_shardother))
        {
            LOG_ERROR(
                "invalid field 'shardother', not a http(s) url: '%s'", entry.shard_shardother.c_str());
            error = true;
        }
        // producturl additionally accepts a deployment-local reference
        // ("local:<path>"): xuoproxy mounts the folder server-side and
        // rewrites the served shards.cfg entry to a same-origin
        // /data/<mount>/ product url (shell.html passes it through)
        const bool product_local = entry.shard_producturl.rfind("local:", 0) == 0;
        if (
            !entry.shard_producturl.empty() && !product_local &&
            !valid_url(entry.shard_producturl))
        {
            LOG_ERROR(
                "invalid field 'producturl', not a http(s) url: '%s'",
                entry.shard_producturl.c_str());
            error = true;
        }
        if (entry.shard_loginserver.empty())
        {
            LOG_ERROR("missing required field 'loginserver'");
            error = true;
        }
        else
        {
            // shape check only, the client defaults the port if missing so
            // published entries without a port must keep working (LOG_WARN)
            const auto &ls = entry.shard_loginserver;
            const auto comma = ls.find(',');
            bool ok = comma != astr_t::npos && comma + 1 < ls.size();
            if (ok)
            {
                for (size_t i = comma + 1; i < ls.size(); ++i)
                {
                    if (ls[i] < '0' || ls[i] > '9')
                    {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok)
                LOG_WARN("field 'loginserver' does not look like host,port: '%s'", ls.c_str());
        }
        // byte-list keys, same grammar as crossuo.cfg: "<0xaa> <0xbb> ..."
        // seedoverride must carry the full 4-byte login seed
        if (!entry.shard_seedoverride.empty())
        {
            bool ok = true;
            const auto &so = entry.shard_seedoverride;
            size_t i = 0, count = 0;
            while (ok && i < so.size())
            {
                while (i < so.size() && (so[i] == ' ' || so[i] == '\t'))
                    ++i;
                if (i >= so.size())
                    break;
                if (i + 3 >= so.size() || so[i] != '0' || (so[i + 1] != 'x' && so[i + 1] != 'X') ||
                    !isxdigit(so[i + 2]) || !isxdigit(so[i + 3]))
                {
                    ok = false;
                    break;
                }
                ++count;
                i += 4;
            }
            if (!ok || count != 4)
                LOG_ERROR(
                    "invalid field 'seedoverride', expected 4 bytes like"
                    " '0x00 0x01 0x48 0xaa': '%s'",
                    so.c_str());
            if (!ok || count != 4)
                error = true;
        }
        // systemcheckoverride just needs well-formed bytes, any length
        if (!entry.shard_systemcheckoverride.empty())
        {
            bool ok = true;
            const auto &sc = entry.shard_systemcheckoverride;
            size_t i = 0;
            while (ok && i < sc.size())
            {
                while (i < sc.size() && (sc[i] == ' ' || sc[i] == '\t'))
                    ++i;
                if (i >= sc.size())
                    break;
                if (i + 3 >= sc.size() || sc[i] != '0' || (sc[i + 1] != 'x' && sc[i + 1] != 'X') ||
                    !isxdigit(sc[i + 2]) || !isxdigit(sc[i + 3]))
                {
                    ok = false;
                    break;
                }
                i += 4;
            }
            if (!ok)
            {
                LOG_ERROR(
                    "invalid field 'systemcheckoverride', expected bytes like"
                    " '0x03 0xb1': '%s'",
                    sc.c_str());
                error = true;
            }
        }
        if (entry.shard_clientversion.empty())
        {
            LOG_ERROR("missing required field 'clientversion'");
            error = true;
        }
        // autoacct=no without a registration page leaves the user nowhere to
        // go (the desktop menu and the web picker hide their Create-account
        // action when shardaccount is empty) - advisory only, the entry still
        // loads
        if (!entry.shard_autoacct && entry.shard_shardaccount.empty())
        {
            LOG_WARN(
                "field 'autoacct' is 'no' but no 'shardaccount' is set: clients"
                " have no registration page to offer");
        }

        if (!error)
            entries.insert(entries.end(), shard.entries.begin(), shard.entries.end());
    }

    if (!error)
    {
        s_shards.entries = entries;
        std::sort(
            s_shards.entries.begin(), s_shards.entries.end(), [](const auto &a, const auto &b) {
                return a.shard_shardname < b.shard_shardname;
            });

        auto fp = fopen("shards.cfg", "wb");
        write_shards(fp);
        fclose(fp);
    }

    return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif // defined(VALIDATOR)