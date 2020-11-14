// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "Container.h"
#include "CrossUO.h"
#include "GameWindow.h"
#include "Sprite.h"
#include "Managers/ConfigManager.h"

std::vector<ContainerDefinition> g_Container;
static ContainerPosition s_ContainerPos;
static ContainerPosition s_ContainerDefaultPos;

ContainerPosition &ContainerDefaultPosition()
{
    return s_ContainerDefaultPos;
}

void ContainerPositionReset()
{
    s_ContainerPos.X = s_ContainerDefaultPos.X;
    s_ContainerPos.Y = s_ContainerDefaultPos.Y;
}

ContainerPosition ContainerNextPosition(uint16_t gumpID)
{
    const int defaultPos = 40;
    const int incrementStep = 20;
    const int incrementMaxWidth = 800;
    const auto spr = g_Game.ExecuteGump(gumpID);
    if (spr != nullptr)
    {
        if (!g_ConfigManager.OffsetInterfaceWindows)
        {
            ContainerPositionReset();
        }
        else
        {
            int passed = 0;
            for (int i = 0; i < 4 && (passed == 0); i++)
            {
                if (s_ContainerPos.X + spr->Width + incrementStep > g_GameWindow.GetSize().Width)
                {
                    s_ContainerPos.X = defaultPos;
                    if (s_ContainerPos.Y + spr->Height + incrementMaxWidth >
                        g_GameWindow.GetSize().Height)
                    {
                        s_ContainerPos.Y = defaultPos;
                    }
                    else
                    {
                        s_ContainerPos.Y += incrementMaxWidth;
                    }
                }
                else if (
                    s_ContainerPos.Y + spr->Height + incrementStep > g_GameWindow.GetSize().Height)
                {
                    if (s_ContainerPos.X + spr->Width + incrementMaxWidth >
                        g_GameWindow.GetSize().Width)
                    {
                        s_ContainerPos.X = defaultPos;
                    }
                    else
                    {
                        s_ContainerPos.X += incrementMaxWidth;
                    }
                    s_ContainerPos.Y = defaultPos;
                }
                else
                {
                    passed = (int)i + 1;
                }
            }
            if (passed == 0)
            {
                ContainerPositionReset();
            }
            else if (passed == 1)
            {
                s_ContainerPos.X += incrementStep;
                s_ContainerPos.Y += incrementStep;
            }
        }
    }

    return s_ContainerPos;
}

void ContainerInit()
{
    // clang-format off
                          // Gump OpenSnd CloseSnd minX minY maxX maxY
    g_Container.push_back({0x0009, 0x0000, 0x0000,  20,  85, 124, 196}); // corpse
    g_Container.push_back({0x003C, 0x0048, 0x0058,  44,  65, 186, 159});
    g_Container.push_back({0x003D, 0x0048, 0x0058,  29,  34, 137, 128});
    g_Container.push_back({0x003E, 0x002F, 0x002E,  33,  36, 142, 148});
    g_Container.push_back({0x003F, 0x004F, 0x0058,  19,  47, 182, 123});
    g_Container.push_back({0x0040, 0x002D, 0x002C,  16,  51, 150, 140});
    g_Container.push_back({0x0041, 0x004F, 0x0058,  35,  38, 145, 116});
    g_Container.push_back({0x0042, 0x002D, 0x002C,  18, 105, 162, 178});
    g_Container.push_back({0x0043, 0x002D, 0x002C,  16,  51, 181, 124});
    g_Container.push_back({0x0044, 0x002D, 0x002C,  20,  10, 170, 100});
    g_Container.push_back({0x0048, 0x002F, 0x002E,  16,  10, 154,  94});
    g_Container.push_back({0x0049, 0x002D, 0x002C,  18, 105, 162, 178});
    g_Container.push_back({0x004A, 0x002D, 0x002C,  18, 105, 162, 178});
    g_Container.push_back({0x004B, 0x002D, 0x002C,  16,  51, 184, 124});
    g_Container.push_back({0x004C, 0x002D, 0x002C,  46,  74, 196, 184});
    g_Container.push_back({0x004D, 0x002F, 0x002E,  76,  12, 140,  68});
    g_Container.push_back({0x004E, 0x002D, 0x002C,  24,  96, 140, 152}); // bugged
    g_Container.push_back({0x004F, 0x002D, 0x002C,  24,  96, 140, 152}); // bugged
    g_Container.push_back({0x0051, 0x002F, 0x002E,  16,  10, 154,  94});
    g_Container.push_back({0x091A, 0x0000, 0x0000,   1,  13, 260, 199}); // game board
    g_Container.push_back({0x092E, 0x0000, 0x0000,   1,  13, 260, 199}); // backgammon game
    g_Container.push_back({0x0104, 0x002F, 0x002E,   0,  20, 168, 115});
    g_Container.push_back({0x0105, 0x002F, 0x002E,   0,  20, 168, 115});
    g_Container.push_back({0x0106, 0x002F, 0x002E,   0,  20, 168, 115});
    g_Container.push_back({0x0107, 0x002F, 0x002E,   0,  20, 168, 115});
    g_Container.push_back({0x0108, 0x004F, 0x0058,   0,  35, 150, 105});
    g_Container.push_back({0x0109, 0x002F, 0x002E,   0,  20, 175, 105});
    g_Container.push_back({0x010A, 0x002F, 0x002E,   0,  20, 175, 105});
    g_Container.push_back({0x010B, 0x002F, 0x002E,   0,  20, 175, 105});
    g_Container.push_back({0x010C, 0x002F, 0x002E,   0,  20, 168, 115});
    g_Container.push_back({0x010D, 0x002F, 0x002E,   0,  20, 168, 115});
    g_Container.push_back({0x010E, 0x002F, 0x002E,   0,  20, 168, 115});
    // present boxes
    g_Container.push_back({0x0102, 0x004F, 0x0058,  15,  10, 245, 120});
    g_Container.push_back({0x011B, 0x004F, 0x0058,  15,  10, 220, 120});
    g_Container.push_back({0x011C, 0x004F, 0x0058,  10,  10, 220, 145});
    g_Container.push_back({0x011D, 0x004F, 0x0058,  10,  10, 220, 130});
    g_Container.push_back({0x011E, 0x004F, 0x0058,  15,  10, 290, 130});
    g_Container.push_back({0x011F, 0x004F, 0x0058,  15,  10, 220, 120});
    g_Container.push_back({0x0120, 0x004F, 0x0058,  15,  10, 220, 130});
    g_Container.push_back({0x0121, 0x004F, 0x0058,  15,  10, 220, 130});
    g_Container.push_back({0x0122, 0x004F, 0x0058,  15,  10, 220, 130});
    g_Container.push_back({0x777A, 0x004F, 0x0058,  15,  10, 220, 130});
    // secret chest
    g_Container.push_back({0x058E, 0x002D, 0x002C,  16,  51, 184, 124});
    // large gumps 
    g_Container.push_back({0X9CDD, 0x002D, 0x002C,  55,  65, 552, 310});
    g_Container.push_back({0X9CDF, 0x002D, 0x002C,  55,  65, 552, 310});
    g_Container.push_back({0x9CE3, 0x002D, 0x002C,  55,  65, 552, 310});
    g_Container.push_back({0X06E9, 0x002D, 0x002C,  70, 100, 330, 330});
    // clang-format on
}
