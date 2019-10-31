// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <external/gfx/ui.h>

enum class ui_view : uint32_t
{
    accounts,
    updates,
    backups,
    shards,

    count,
};

struct ui_model
{
    ImVec2 area;
    ui_view view = ui_view::updates;
};
