// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>

namespace xuol {

namespace shard
{
// install source of the shard data, as parsed from `installer = <type>+<url>`
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
} // namespace shard

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
    const char *urlother_name;
    const char *urlother_url;
    shard::tag_install_type install_type;
    const char *install_url;
};

bool shard_getter(void *data, int idx, const char **out_text);
void load_shards();
int shard_count();
int shard_index_by_loginserver(const char *login_server);
shard_data shard_by_id(int id);

} // namespace xuol
