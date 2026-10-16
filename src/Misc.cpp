// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "Misc.h"

#include "Config.h"
#include "revision.h"

#include <common/logging/logging.h>
// no STR_IMPLEMENTATION here: the game links xuocore (mft.cpp), which already
// provides the str implementations - two copies broke the shared lib link
// when the streaming wiring started pulling xuocore into the game
#include <common/str.h>

SDL_threadID g_MainThread;

int CalculatePercents(int max, int current, int maxValue)
{
    if (max > 0)
    {
        max = (current * 100) / max;

        if (max > 100)
        {
            max = 100;
        }

        if (max > 1)
        {
            max = (maxValue * max) / 100;
        }
    }

    return max;
}

astr_t ClientWindowTitle()
{
    astr_t title = CLIENT_TITLE;
    if (!g_Config.ShardName.empty())
    {
        title += " [" + g_Config.ShardName + "]";
    }
    return title;
}
