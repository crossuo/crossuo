// MIT License
// Copyright (C) August 2016 Hotride

#include "LandObject.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"

CLandObject::CLandObject(int serial, uint16_t graphic, uint16_t color, short x, short y, char z)
    : CMapObject(ROT_LAND_OBJECT, serial, 0, color, x, y, z)
    , MinZ(z)
    , AverageZ(z)
{
    DEBUG_TRACE_FUNCTION;
    OriginalGraphic = graphic;
    UpdateGraphicBySeason();

    m_DrawTextureColor[3] = 0xFF;

    LAND_TILES &tile = g_Game.m_LandData[graphic];

    IsStretched = ((tile.TexID == 0u) && ::IsWet(tile.Flags));

    memset(&m_Rect, 0, sizeof(m_Rect));
    memset(&m_Normals[0], 0, sizeof(m_Normals));

#if UO_DEBUG_INFO != 0
    g_LandObjectsCount++;
#endif
}

CLandObject::~CLandObject()
{
    DEBUG_TRACE_FUNCTION;
    if (PositionBuffer != 0)
    {
        glDeleteBuffers(1, &PositionBuffer);
        PositionBuffer = 0;
    }

    if (VertexBuffer != 0)
    {
        glDeleteBuffers(1, &VertexBuffer);
        VertexBuffer = 0;
    }

    if (NormalBuffer != 0)
    {
        glDeleteBuffers(1, &NormalBuffer);
        NormalBuffer = 0;
    }
}

void CLandObject::UpdateGraphicBySeason()
{
    DEBUG_TRACE_FUNCTION;
    Graphic = g_Game.GetLandSeasonGraphic(OriginalGraphic);
    NoDrawTile = (Graphic == 2);
}

int CLandObject::GetDirectionZ(int direction)
{
    DEBUG_TRACE_FUNCTION;
    switch (direction)
    {
        case 1:
            return (m_Rect.h / 4);
        case 2:
            return (m_Rect.w / 4);
        case 3:
            return (m_Rect.y / 4);
        default:
            break;
    }

    return m_Z;
}

int CLandObject::CalculateCurrentAverageZ(int direction)
{
    DEBUG_TRACE_FUNCTION;
    int result = GetDirectionZ(((uint8_t)(direction >> 1) + 1) & 3);

    if ((direction & 1) != 0)
    {
        return result;
    }

    return (result + GetDirectionZ(direction >> 1)) >> 1;
}

void CLandObject::UpdateZ(int zTop, int zRight, int zBottom)
{
    DEBUG_TRACE_FUNCTION;
    if (IsStretched)
    {
        Serial = ((m_Z + zTop + zRight + zBottom) / 4);

        m_Rect.x = m_Z * 4 + 1;
        m_Rect.y = zTop * 4;
        m_Rect.w = zRight * 4;
        m_Rect.h = zBottom * 4 + 1;

        if (abs(m_Z - zRight) <= abs(zBottom - zTop))
        {
            AverageZ = (m_Z + zRight) >> 1;
        }
        else
        {
            AverageZ = (zBottom + zTop) >> 1;
        }

        MinZ = m_Z;

        if (zTop < MinZ)
        {
            MinZ = zTop;
        }

        if (zRight < MinZ)
        {
            MinZ = zRight;
        }

        if (zBottom < MinZ)
        {
            MinZ = zBottom;
        }
    }
}

void CLandObject::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Z <= g_MaxGroundZ)
    {
        uint16_t objColor = 0;

        if (g_DeveloperMode == DM_DEBUGGING && g_SelectedObject.Object == this)
        {
            objColor = SELECT_LAND_COLOR;
        }

#if UO_DEBUG_INFO != 0
        g_RenderedObjectsCountInGameWindow++;
#endif

        if (!IsStretched)
        {
            g_Game.DrawLandArt(Graphic, objColor, x, y);
        }
        else
        {
            g_Game.DrawLandTexture(this, objColor, x, y);
        }
    }
}

void CLandObject::Select(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Z <= g_MaxGroundZ)
    {
        if (!IsStretched)
        {
            if (g_Game.LandPixelsInXY(Graphic, x, y))
            {
                g_SelectedObject.Init(this);
            }
        }
        else
        {
            if (g_Game.LandTexturePixelsInXY(x, y + (m_Z * 4), m_Rect))
            {
                g_SelectedObject.Init(this);
            }
        }
    }
}
