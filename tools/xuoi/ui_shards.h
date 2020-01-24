// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

struct ui_model;

int shard_picked();
bool ui_shards_combo(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    int popup_max_height_in_items = -1);
void ui_shards(ui_model &m, bool picker = false);
