// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

struct shard_data
{
    const char *loginserver;
    const char *clienttype;
    const char *clientversion;
};

bool shard_getter(void *data, int idx, const char **out_text);
void load_shards();
int shard_index_by_loginserver(const char *login_server);
shard_data shard_by_id(int id);
