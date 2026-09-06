// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <external/gfx/ui.h>

enum class ui_view : uint32_t
{
    accounts,
    updates,
    backups,
    shards,
    shard_picker,

    count,
};

struct ui_model
{
    ImVec2 area;
    ui_view view = ui_view::updates;
};

void ui_push(ui_model &m, ui_view v);
void ui_pop(ui_model &m);
