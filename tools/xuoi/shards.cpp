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

struct lang_type
{
    std::vector<astr_t> langs;
};

astr_t as_str(const lang_type &in)
{
    return join(in.langs, ',');
}

bool convert(lang_type &out, const char *raw)
{
    convert(out.langs, raw, ',');
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

const char *shard_getter(void *data, int idx)
{
    auto *items = (std::vector<shard::entry> *)data;
    assert(items);
    assert(idx < (int)items->size());
    return items->at(idx).shard_name.c_str();
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

// case-insensitive `prefix` check against the head of `value`
static bool starts_with_ci(const astr_t &value, const char *prefix)
{
    const astr_t p(prefix);
    if (value.size() < p.size())
        return false;
    astr_t head = value.substr(0, p.size());
    std::transform(head.begin(), head.end(), head.begin(), ::tolower);
    return head == p;
}

// case-insensitive whole-value compare
static bool equals_ci(const astr_t &value, const char *other)
{
    return value.size() == strlen(other) && starts_with_ci(value, other);
}

// tolerate the pre-2.0 published lists until they are republished (the
// prefix grammar is gone from the client, which only takes plain urls):
//   installer: `uo+<url>`/`xuo+<url>` carried a streamable product url, keep
//     the remainder; `website+`/`download+` never did, clear to a manual
//     install; the bare `latest` token (e.g. `uo+latest`) means "use the
//     client's default product", which is exactly what an empty installer
//     already means, clear it; already-plain urls are kept as-is
//   urlother: the old `Name+<url>` form keeps only the url, so the discord
//     sniff (url_is_discord) and the ui link work on both grammars
static int normalize_legacy_fields(shard::entry &e)
{
    int count = 0;
    if (starts_with_ci(e.shard_installer, "xuo+"))
    {
        e.shard_installer.erase(0, 4);
        ++count;
    }
    else if (starts_with_ci(e.shard_installer, "uo+"))
    {
        e.shard_installer.erase(0, 3);
        ++count;
    }
    else if (starts_with_ci(e.shard_installer, "website+") || starts_with_ci(e.shard_installer, "download+"))
    {
        // store/download links are not streamable product urls
        e.shard_installer.clear();
        ++count;
    }
    // `latest` is not a url: clearing it keeps clients from building a
    // garbage --streaming-product=latest / latest/patchlist request
    if (equals_ci(e.shard_installer, "latest"))
        e.shard_installer.clear();

    const auto plus = e.shard_urlother.find('+');
    if (plus != astr_t::npos && plus > 0)
    {
        const auto name = e.shard_urlother.substr(0, plus);
        const auto url = e.shard_urlother.substr(plus + 1);
        const bool plain_name = std::all_of(name.begin(), name.end(), [](char c) {
            return ::isalnum((unsigned char)c) || c == ' ';
        });
        if (plain_name && (starts_with_ci(url, "http://") || starts_with_ci(url, "https://")))
        {
            e.shard_urlother = url;
            ++count;
        }
    }
    return count;
}

void load_shards()
{
    s_shards = shard::data();
    auto custom = shard::default_entry();
    custom.shard_name = "<custom shard>";
    s_shards.entries.emplace_back(custom);
#if !defined(VALIDATOR)
    const auto fname = fs_path_join(xuol_data_path(), "shards.cfg");
    http_get_file(
        "https://raw.githubusercontent.com/crossuo/shards/release/shards.cfg",
        fs_path_ascii(fname));
#else
    const auto fname = fs_path_join(fs_path_current(), "shards.cfg");
#endif
    load_shard_file(fname, s_shards);

    // the published list may still use the removed prefix grammar
    int normalized = 0;
    for (auto &e : s_shards.entries)
        normalized += normalize_legacy_fields(e);
    if (normalized > 0)
        LOG_INFO("normalized %d legacy installer/urlother fields", normalized);

    // sort by tags 'highlight', then name alphabetically
    std::sort(
        s_shards.entries.begin() + 1, s_shards.entries.end(), [](const auto &a, const auto &b) {
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
    return { s.shard_name.c_str(),
             s.shard_loginserver.c_str(),
             s.shard_clienttype.c_str(),
             s.shard_clientversion.c_str(),
             s.shard_protocolversion.c_str(),
             s.shard_crypt,
             s.shard_url.c_str(),
             s.shard_urlregister.c_str(),
             s.shard_urlvideo.c_str(),
             s.shard_urlother,
             s.shard_installer.c_str() };
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
    fs_path_list(fs_path_from(s_cli["path"].get().string), configs);
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
        // optional urls: only validated when non-empty, must be http(s)
        if (!entry.shard_url.empty() && !valid_url(entry.shard_url))
        {
            LOG_ERROR("invalid field 'url', not a http(s) url: '%s'", entry.shard_url.c_str());
            error = true;
        }
        if (!entry.shard_urlforum.empty() && !valid_url(entry.shard_urlforum))
        {
            LOG_ERROR(
                "invalid field 'urlforum', not a http(s) url: '%s'", entry.shard_urlforum.c_str());
            error = true;
        }
        if (!entry.shard_urlregister.empty() && !valid_url(entry.shard_urlregister))
        {
            LOG_ERROR(
                "invalid field 'urlregister', not a http(s) url: '%s'",
                entry.shard_urlregister.c_str());
            error = true;
        }
        if (!entry.shard_urlvideo.empty() && !valid_url(entry.shard_urlvideo))
        {
            LOG_ERROR(
                "invalid field 'urlvideo', not a http(s) url: '%s'", entry.shard_urlvideo.c_str());
            error = true;
        }
        // installer/urlother are plain urls: empty is fine (manual install /
        // no community link), a non-empty value must be a http(s) url
        if (!entry.shard_urlother.empty() && !valid_url(entry.shard_urlother))
        {
            LOG_ERROR(
                "invalid field 'urlother', not a http(s) url: '%s'", entry.shard_urlother.c_str());
            error = true;
        }
        if (!entry.shard_installer.empty() && !valid_url(entry.shard_installer))
        {
            LOG_ERROR(
                "invalid field 'installer', not a http(s) url: '%s'",
                entry.shard_installer.c_str());
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