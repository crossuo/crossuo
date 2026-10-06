// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <common/str.h> // astr_t

namespace xuol {

// a snapshot of a shard entry, enough to build a launch profile from it;
// the pointers refer to the loaded shard list and stay valid until the
// next load_shards()
struct shard_data
{
    const char *name;
    const char *loginserver;
    const char *clienttype;
    const char *clientversion;
    bool crypt;
    const char *url;         // website
    const char *urlregister; // account registration page
    const char *urlvideo;    // video/trailer url ("" = none)
    astr_t urlother;         // plain community url ("" = none); a discord
                             // link is detected from the url itself
                             // (contains discord.gg/ or discord.com/)
    const char *install_url; // plain data product url ("" = manual install,
                             // the client falls back to its default)
};

// true when the url looks like a discord community link
bool url_is_discord(const astr_t &url);

bool shard_getter(void *data, int idx, const char **out_text);
void load_shards();
int shard_count();
int shard_index_by_loginserver(const char *login_server);
shard_data shard_by_id(int id);

} // namespace xuol
