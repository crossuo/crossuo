// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpMinimap.h"
#include "../OrionUO.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ColorManager.h"
#include "../Managers/MapManager.h"
#include "../Managers/UOFileReader.h"
#include "../GameObjects/GameItem.h"
#include "../GameObjects/MapBlock.h"
#include "../GameObjects/GamePlayer.h"

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

    /*const CPoint2Di foliageOffsetTable[17 * 3] =
	{
		CPoint2Di(0, 0),
		CPoint2Di(-2, 1),
		CPoint2Di(-2, -2),
		CPoint2Di(-1, -1),
		CPoint2Di(-1, 0),
		CPoint2Di(-1, 1),
		CPoint2Di(-1, 2),
		CPoint2Di(-1, -1),
		CPoint2Di(0, 1),
		CPoint2Di(0, 2),
		CPoint2Di(0, -2),
		CPoint2Di(1, -1),
		CPoint2Di(1, 0),
		CPoint2Di(1, 1),
		CPoint2Di(1, -1),
		CPoint2Di(2, 0),
		CPoint2Di(2, 0),

		CPoint2Di(0, -1),
		CPoint2Di(-2, 0),
		CPoint2Di(-2, -1),
		CPoint2Di(-1, 0),
		CPoint2Di(-1, 1),
		CPoint2Di(-1, 2),
		CPoint2Di(-1, -2),
		CPoint2Di(0, -1),
		CPoint2Di(0, 1),
		CPoint2Di(0, 2),
		CPoint2Di(0, -2),
		CPoint2Di(1, -1),
		CPoint2Di(1, 0),
		CPoint2Di(1, 1),
		CPoint2Di(1, 0),
		CPoint2Di(2, 1),
		CPoint2Di(2, 0),

		CPoint2Di(0, -1),
		CPoint2Di(-2, 1),
		CPoint2Di(-2, -2),
		CPoint2Di(-1, -1),
		CPoint2Di(-1, 0),
		CPoint2Di(-1, 1),
		CPoint2Di(-1, 2),
		CPoint2Di(-1, -1),
		CPoint2Di(0, 1),
		CPoint2Di(0, -2),
		CPoint2Di(1, -1),
		CPoint2Di(1, 0),
		CPoint2Di(1, 1),
		CPoint2Di(1, 2),
		CPoint2Di(1, -1),
		CPoint2Di(2, 1),
		CPoint2Di(2, 0)
	};*/

    const CPoint2Di originalOffsetTable[2] = { CPoint2Di(0, 0), CPoint2Di(0, 1) };

    if (g_Player != nullptr)
    {
        LastX = g_Player->GetX();
        LastY = g_Player->GetY();
    }

    m_Texture.Clear();

    uint16_t gumpID = 0x1393 - (int)Minimized;
    CIndexObject &io = g_Orion.m_GumpDataIndex[gumpID];

    int gumpWidth = io.Width;
    int gumpHeight = io.Height;

    vector<uint16_t> data = g_UOFileReader.GetGumpPixels(io);

    if (data.empty())
    {
        return;
    }

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
    {
        minBlockX = 0;
    }

    if (minBlockY < 0)
    {
        minBlockY = 0;
    }

    int map = g_MapManager.GetActualMap();
    uint32_t maxBlockIndex = g_MapManager.MaxBlockIndex;
    int mapBlockHeight = g_MapBlockSize[map].Height;

    for (int i = minBlockX; i <= maxBlockX; i++)
    {
        uint32_t blockIndexOffset = i * mapBlockHeight;

        for (int j = minBlockY; j <= maxBlockY; j++)
        {
            uint32_t blockIndex = blockIndexOffset + j;

            if (blockIndex >= maxBlockIndex)
            {
                break;
            }

            RADAR_MAP_BLOCK mb = {};
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

                    uint32_t color = mb.Cells[x][y].Graphic;
                    char &isLand = mb.Cells[x][y].IsLand;

                    if (mapBlock != nullptr)
                    {
                        uint16_t multiColor = mapBlock->GetRadarColor((int)x, (int)y);

                        if (multiColor != 0u)
                        {
                            color = multiColor;
                            isLand = 0;
                        }
                    }

                    if (isLand == 0)
                    {
                        color = g_Orion.GetSeasonGraphic(color) + 0x4000;
                    }
                    else
                    {
                        color = g_Orion.GetLandSeasonGraphic(color);
                    }

                    int tableSize = 2;
                    const CPoint2Di *table = &originalOffsetTable[0];

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
    const CPoint2Di *table,
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
        {
            continue;
        }

        int gy = py;

        if (gy < 0 || gy >= height)
        {
            break;
        }

        int block = (gy * width) + gx;

        if (data[block] == 0x8421)
        {
            data[block] = color;
        }
    }
}

void CGumpMinimap::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (g_Player->GetX() != LastX || g_Player->GetY() != LastY || m_Texture.Texture == 0)
    {
        GenerateMap();
    }
    else if ((m_Count == 0u) || m_Count == 6 || WantRedraw)
    {
        WantUpdateContent = true;
    }

    static uint32_t ticks = 0;

    if (ticks < g_Ticks)
    {
        m_Count += 7;
        ticks = g_Ticks + 300;
    }

    if (m_Count > 12)
    {
        m_Count = 0;
    }
}

void CGumpMinimap::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t graphic = 0x1393 - (int)Minimized;

    CGLTexture *th = g_Orion.ExecuteGump(graphic);

    if (th == nullptr)
    {
        return;
    }

    if (m_Items == nullptr)
    {
        m_DataBox = (CGUIDataBox *)Add(new CGUIDataBox());
        m_Body = (CGUIGumppic *)Add(new CGUIGumppic(graphic, 0, 0));
        m_Body->SelectOnly = true;
    }
    else
    {
        m_DataBox->Clear();
    }

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
            {
                continue; //multi
            }

            if (go->NPC && !go->IsPlayer())
            {
                uint16_t color =
                    g_ConfigManager.GetColorByNotoriety(go->GameCharacterPtr()->Notoriety);

                if (color != 0u)
                {
                    uint32_t pcl = g_ColorManager.GetPolygoneColor(16, color);

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
    {
        LockMoving = !LockMoving;
    }
}

bool CGumpMinimap::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;

    g_Orion.OpenMinimap();

    return true;
}
