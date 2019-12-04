// MIT License
// Copyright (C) August 2016 Hotride

#include "Container.h"
#include "CrossUO.h"
#include "GameWindow.h"
#include "Sprite.h"
#include "Managers/ConfigManager.h"
#include <vector>

CContainerRect g_ContainerRect;
std::vector<CContainerOffset> g_ContainerOffset;

/*CONTAINER_OFFSET g_ContainerOffset[CONTAINERS_COUNT] =
{
	//Gump   OpenSnd  CloseSnd  X   Y   Width Height
	{ 0x0009, 0x0000, 0x0000, { 20, 85, 124, 196 } }, //corpse
	{ 0x003C, 0x0048, 0x0058, { 44, 65, 186, 159 } },
	{ 0x003D, 0x0048, 0x0058, { 29, 34, 137, 128 } },
	{ 0x003E, 0x002F, 0x002E, { 33, 36, 142, 148 } },
	{ 0x003F, 0x004F, 0x0058, { 19, 47, 182, 123 } },
	{ 0x0040, 0x002D, 0x002C, { 16, 51, 150, 140 } },
	{ 0x0041, 0x004F, 0x0058, { 35, 38, 145, 116 } },
	{ 0x0042, 0x002D, 0x002C, { 18, 105, 162, 178 } },
	{ 0x0043, 0x002D, 0x002C, { 16, 51, 181, 124 } },
	{ 0x0044, 0x002D, 0x002C, { 20, 10, 170, 100 } },
	{ 0x0048, 0x002F, 0x002E, { 16, 10, 154, 94 } },
	{ 0x0049, 0x002D, 0x002C, { 18, 105, 162, 178 } },
	{ 0x004A, 0x002D, 0x002C, { 18, 105, 162, 178 } },
	{ 0x004B, 0x002D, 0x002C, { 16, 51, 184, 124 } },
	{ 0x004C, 0x002D, 0x002C, { 46, 74, 196, 184 } },
	{ 0x004D, 0x002F, 0x002E, { 76, 12, 140, 68 } },
	{ 0x004E, 0x002D, 0x002C, { 24, 96, 140, 152 } }, //bugged
	{ 0x004F, 0x002D, 0x002C, { 24, 96, 140, 152 } }, //bugged
	{ 0x0051, 0x002F, 0x002E, { 16, 10, 154, 94 } },
	{ 0x091A, 0x0000, 0x0000, { 1, 13, 260, 199 } }, //game board
	{ 0x092E, 0x0000, 0x0000, { 1, 13, 260, 199 } }, //backgammon game
	{ 0x0104, 0x002F, 0x002E, { 0, 20, 168, 115 } },
	{ 0x0105, 0x002F, 0x002E, { 0, 20, 168, 115 } },
	{ 0x0106, 0x002F, 0x002E, { 0, 20, 168, 115 } },
	{ 0x0107, 0x002F, 0x002E, { 0, 20, 168, 115 } },
	{ 0x0108, 0x004F, 0x0058, { 0, 35, 150, 105 } },
	{ 0x0109, 0x002F, 0x002E, { 0, 20, 175, 105 } },
	{ 0x010A, 0x002F, 0x002E, { 0, 20, 175, 105 } },
	{ 0x010B, 0x002F, 0x002E, { 0, 20, 175, 105 } },
	{ 0x010C, 0x002F, 0x002E, { 0, 20, 168, 115 } },
	{ 0x010D, 0x002F, 0x002E, { 0, 20, 168, 115 } },
	{ 0x010E, 0x002F, 0x002E, { 0, 20, 168, 115 } }
};*/

void CContainerRect::Calculate(uint16_t gumpID)
{
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteGump(gumpID);
    if (spr != nullptr)
    {
        if (!g_ConfigManager.OffsetInterfaceWindows)
        {
            X = DefaultX;
            Y = DefaultY;
        }
        else
        {
            int passed = 0;
            for (int i = 0; i < 4 && (passed == 0); i++)
            {
                if (X + spr->Width + CONTAINERS_RECT_STEP > g_GameWindow.GetSize().Width)
                {
                    X = CONTAINERS_RECT_DEFAULT_POS;
                    if (Y + spr->Height + CONTAINERS_RECT_LINESTEP > g_GameWindow.GetSize().Height)
                    {
                        Y = CONTAINERS_RECT_DEFAULT_POS;
                    }
                    else
                    {
                        Y += CONTAINERS_RECT_LINESTEP;
                    }
                }
                else if (Y + spr->Height + CONTAINERS_RECT_STEP > g_GameWindow.GetSize().Height)
                {
                    if (X + spr->Width + CONTAINERS_RECT_LINESTEP > g_GameWindow.GetSize().Width)
                    {
                        X = CONTAINERS_RECT_DEFAULT_POS;
                    }
                    else
                    {
                        X += CONTAINERS_RECT_LINESTEP;
                    }
                    Y = CONTAINERS_RECT_DEFAULT_POS;
                }
                else
                {
                    passed = (int)i + 1;
                }
            }
            if (passed == 0)
            {
                MakeDefault();
            }
            else if (passed == 1)
            {
                X += CONTAINERS_RECT_STEP;
                Y += CONTAINERS_RECT_STEP;
            }
        }
    }
}
