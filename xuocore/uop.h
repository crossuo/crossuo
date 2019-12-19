// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <common/str.h>

#define UOP_HASH(ph, sh) static_cast<uint64_t>(((uint64_t(ph) << 32) | sh))

void uop_populate_asset_names();
const std::string &uop_asset_name(const char *package, uint64_t hash, uint32_t *out_id);
