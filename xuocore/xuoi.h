// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include "mft.h"

mft_result xuoi_product_install(mft_config &cfg, const char *product_url, const char *product_file);
void xuoi_listing_save(const fs_path &path, const mft_entries_map &data);
int xuoi_listing_load(const fs_path &path, mft_entries_map &data);
int xuoi_diff(mft_product &prod, const mft_entries_map &data1, const mft_entries_map &data2);