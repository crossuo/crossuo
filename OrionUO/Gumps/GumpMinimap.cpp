// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GumpMinimap.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGumpMinimap::CGumpMinimap(short x, short y, bool minimized)
    : CGump(GT_MINIMAP, 0, x, y)
{
    DEBUG_TRACE_FUNCTION;
    Minimized = minimized;
    m_Locker.Serial = ID_GMM_LOCK_MOVING;
    GenerateMap();
}

CGumpMinimap::~CGumpMinimap()
{
    DEBUG_TRACE_FUNCTION;
    m_Texture.Clear();
}

void CGumpMinimap::CalculateGumpState()
{
    DEBUG_TRACE_FUNCTION;
    bool minimized = Minimized;
    Minimized = false;

    CGump::CalculateGumpState();

    Minimized = minimized;
}

void CGumpMinimap::GenerateMap()
{
    DEBUG_TRACE_FUNCTION;

    /*const Wisp::CPoint2Di foliageOffsetTable[17 * 3] =
	{
		Wisp::CPoint2Di(0, 0),
		Wisp::CPoint2Di(-2, 1),
		Wisp::CPoint2Di(-2, -2),
		Wisp::CPoint2Di(-1, -1),
		Wisp::CPoint2Di(-1, 0),
		Wisp::CPoint2Di(-1, 1),
		Wisp::CPoint2Di(-1, 2),
		Wisp::CPoint2Di(-1, -1),
		Wisp::CPoint2Di(0, 1),
		Wisp::CPoint2Di(0, 2),
		Wisp::CPoint2Di(0, -2),
		Wisp::CPoint2Di(1, -1),
		Wisp::CPoint2Di(1, 0),
		Wisp::CPoint2Di(1, 1),
		Wisp::CPoint2Di(1, -1),
		Wisp::CPoint2Di(2, 0),
		Wisp::CPoint2Di(2, 0),

		Wisp::CPoint2Di(0, -1),
		Wisp::CPoint2Di(-2, 0),
		Wisp::CPoint2Di(-2, -1),
		Wisp::CPoint2Di(-1, 0),
		Wisp::CPoint2Di(-1, 1),
		Wisp::CPoint2Di(-1, 2),
		Wisp::CPoint2Di(-1, -2),
		Wisp::CPoint2Di(0, -1),
		Wisp::CPoint2Di(0, 1),
		Wisp::CPoint2Di(0, 2),
		Wisp::CPoint2Di(0, -2),
		Wisp::CPoint2Di(1, -1),
		Wisp::CPoint2Di(1, 0),
		Wisp::CPoint2Di(1, 1),
		Wisp::CPoint2Di(1, 0),
		Wisp::CPoint2Di(2, 1),
		Wisp::CPoint2Di(2, 0),

		Wisp::CPoint2Di(0, -1),
		Wisp::CPoint2Di(-2, 1),
		Wisp::CPoint2Di(-2, -2),
		Wisp::CPoint2Di(-1, -1),
		Wisp::CPoint2Di(-1, 0),
		Wisp::CPoint2Di(-1, 1),
		Wisp::CPoint2Di(-1, 2),
		Wisp::CPoint2Di(-1, -1),
		Wisp::CPoint2Di(0, 1),
		Wisp::CPoint2Di(0, -2),
		Wisp::CPoint2Di(1, -1),
		Wisp::CPoint2Di(1, 0),
		Wisp::CPoint2Di(1, 1),
		Wisp::CPoint2Di(1, 2),
		Wisp::CPoint2Di(1, -1),
		Wisp::CPoint2Di(2, 1),
		Wisp::CPoint2Di(2, 0)
	};*/

    const Wisp::CPoint2Di originalOffsetTable[2] = { Wisp::CPoint2Di(0, 0),
                                                              Wisp::CPoint2Di(0, 1) };

    if (g_Player != nullptr)
    {
        LastX = g_Player->GetX();
        LastY = g_Player->GetY();
    }

    m_Texture.Clear();

    ushort gumpID = 0x1393 - (int)Minimized;
    CIndexObject &io = g_Orion.m_GumpDataIndex[gumpID];

    int gumpWidth = io.Width;
    int gumpHeight = io.Height;

    vector<uint16_t> data = g_UOFileReader.GetGumpPixels(io);

    if (!data.size())
        return;

    int blockOffsetX = gumpWidth / 4;
    int blockOffsetY = gumpHeight / 4;

    int gumpCenterX = gumpWidth / 2;
    int gumpCenterY = gumpHeight / 2;

    //0xFF080808 - pixel32
    //0x8421 - pixel16

    int minBlockX = (LastX - blockOffsetX) / 8 - 1;
    int minBlockY = (LastY - blockOffsetY) / 8 - 1;
    int maxBlockX = ((LastX + blockOffsetX) / 8) + 1;
    int maxBlockY = ((LastY + blockOffsetY) / 8) + 1;

    if (minBlockX < 0)
        minBlockX = 0;

    if (minBlockY < 0)
        minBlockY = 0;

    int map = g_MapManager.GetActualMap();
    uint maxBlockIndex = g_MapManager.MaxBlockIndex;
    int mapBlockHeight = g_MapBlockSize[map].Height;

    for (int i = minBlockX; i <= maxBlockX; i++)
    {
        uint blockIndexOffset = i * mapBlockHeight;

        for (int j = minBlockY; j <= maxBlockY; j++)
        {
            uint blockIndex = blockIndexOffset + j;

            if (blockIndex >= maxBlockIndex)
                break;

            RADAR_MAP_BLOCK mb = { 0 };
            g_MapManager.GetRadarMapBlock(i, j, mb);

            CMapBlock *mapBlock = g_MapManager.GetBlock(blockIndex);

            int realBlockX = (i * 8);
            int realBlockY = (j * 8);

            for (int x = 0; x < 8; x++)
            {
                int px = ((realBlockX + (int)x) - LastX) + gumpCenterX;

                for (int y = 0; y < 8; y++)
                {
                    int py = (realBlockY + (int)y) - LastY;

                    int gx = px - py;
                    int gy = px + py;

                    uint color = mb.Cells[x][y].Graphic;
                    char &isLand = mb.Cells[x][y].IsLand;

                    if (mapBlock != nullptr)
                    {
                        ushort multiColor = mapBlock->GetRadarColor((int)x, (int)y);

                        if (multiColor)
                        {
                            color = multiColor;
                            isLand = false;
                        }
                    }

                    if (!isLand)
                        color = g_Orion.GetSeasonGraphic(color) + 0x4000;
                    else
                        color = g_Orion.GetLandSeasonGraphic(color);

                    int tableSize = 2;
                    const Wisp::CPoint2Di *table = &originalOffsetTable[0];

                    /*if (color > 0x4000 && ::IsFoliage(g_Orion.GetStaticFlags(color - 0x4000)))
					{
						tableSize = 17;
						table = &foliageOffsetTable[((color - 0x4000) % 3) * tableSize];
					}*/

                    color = 0x8000 | g_ColorManager.GetRadarColorData(color);

                    CreatePixels(data, color, gx, gy, gumpWidth, gumpHeight, table, tableSize);
                }
            }
        }
    }

    g_GL_BindTexture16(m_Texture, gumpWidth, gumpHeight, &data[0]);

    WantUpdateContent = true;
}

void CGumpMinimap::CreatePixels(
    vector<uint16_t> &data,
    int color,
    int x,
    int y,
    int width,
    int height,
    const Wisp::CPoint2Di *table,
    int count)
{
    int px = x;
    int py = y;

    for (int i = 0; i < count; i++)
    {
        px += table[i].X;
        py += table[i].Y;
        int gx = px;

        if (gx < 0 || gx >= width)
            continue;

        int gy = py;

        if (gy < 0 || gy >= height)
            break;

        int block = (gy * width) + gx;

        if (data[block] == 0x8421)
            data[block] = color;
    }
}

void CGumpMinimap::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (g_Player->GetX() != LastX || g_Player->GetY() != LastY || m_Texture.Texture == 0)
        GenerateMap();
    else if (!m_Count || m_Count == 6 || WantRedraw)
        WantUpdateContent = true;

    static uint ticks = 0;

    if (ticks < g_Ticks)
    {
        m_Count += 7;
        ticks = g_Ticks + 300;
    }

    if (m_Count > 12)
        m_Count = 0;
}

void CGumpMinimap::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    ushort graphic = 0x1393 - (int)Minimized;

    CGLTexture *th = g_Orion.ExecuteGump(graphic);

    if (th == nullptr)
        return;

    if (m_Items == nullptr)
    {
        m_DataBox = (CGUIDataBox *)Add(new CGUIDataBox());
        m_Body = (CGUIGumppic *)Add(new CGUIGumppic(graphic, 0, 0));
        m_Body->SelectOnly = true;
    }
    else
        m_DataBox->Clear();

    m_Body->Graphic = graphic;

    int playerX = g_Player->GetX();
    int playerY = g_Player->GetY();

    int gumpWidth = th->Width;
    int gumpHeight = th->Height;

    int gumpCenterX = (gumpWidth / 2) - 1;
    int gumpCenterY = (gumpHeight / 2) - 1;

    m_DataBox->Add(new CGUIExternalTexture(&m_Texture, false, 0, 0));

    if (m_Count < 6)
    {
        QFOR(go, g_World->m_Items, CGameObject *)
        {
            if (go->Container != 0xFFFFFFFF || ((CGameItem *)go)->MultiBody)
                continue; //multi

            if (go->NPC && !go->IsPlayer())
            {
                ushort color =
                    g_ConfigManager.GetColorByNotoriety(go->GameCharacterPtr()->Notoriety);

                if (color)
                {
                    uint pcl = g_ColorManager.GetPolygoneColor(16, color);

                    int x = go->GetX() - playerX;
                    int y = go->GetY() - playerY;

                    int gx = x - y;
                    int gy = x + y;

                    m_DataBox->Add(new CGUIColoredPolygone(
                        0, 0, gumpCenterX + gx, gumpCenterY + gy, 2, 2, pcl));
                }
            }
        }

        m_DataBox->Add(new CGUIColoredPolygone(0, 0, gumpCenterX, gumpCenterY, 2, 2, 0xFFFFFFFF));
    }
}

void CGumpMinimap::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GMM_LOCK_MOVING)
        LockMoving = !LockMoving;
}

bool CGumpMinimap::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;

    g_Orion.OpenMinimap();

    return true;
}

