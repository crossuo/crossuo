// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

namespace xuol {

struct ui_model;

extern const char *client_types[];
extern const char *client_types_cfg[];

void load_accounts();
void write_accounts(void *fp);
void ui_accounts(ui_model &m);
// launches the client for the shard list entry (no account required);
// standalone hosts spawn the client, the embedded host applies it live
void account_launch_shard(int shard_index);

} // namespace xuol
