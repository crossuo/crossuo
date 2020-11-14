// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <vector>

struct ContainerDefinition
{
    uint16_t Gump = 0;
    uint16_t OpenSound = 0;
    uint16_t CloseSound = 0;
    int MinX = 0;
    int MinY = 0;
    int MaxX = 0;
    int MaxY = 0;
};

struct ContainerPosition
{
    uint16_t X = 40;
    uint16_t Y = 40;
};

void ContainerInit();
void ContainerPositionReset();
ContainerPosition ContainerNextPosition(uint16_t gumpID);
ContainerPosition &ContainerDefaultPosition();

extern std::vector<ContainerDefinition> g_Container;
