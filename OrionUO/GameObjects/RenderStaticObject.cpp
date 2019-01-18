// MIT License
// Copyright (C) August 2016 Hotride

#include "RenderStaticObject.h"
#include "GamePlayer.h"
#include "../OrionUO.h"
#include "../SelectedObject.h"
#include "../plugin/mulstruct.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/GameScreen.h"
#include "../TextEngine/TextContainer.h"
#include "../TextEngine/TextData.h"

CRenderStaticObject::CRenderStaticObject(
    RENDER_OBJECT_TYPE renderType,
    int serial,
    uint16_t graphic,
    uint16_t color,
    short x,
    short y,
    char z)
    : CMapObject(renderType, serial, graphic, color, x, y, z)
{
    DEBUG_TRACE_FUNCTION;
    m_TiledataPtr = &g_Orion.m_StaticData[graphic];

    if (m_TiledataPtr->Height > 5)
    {
        CanBeTransparent = 1;
    }
    else if (IsRoof() || (IsSurface() && IsBackground()) || IsWall())
    {
        CanBeTransparent = 1;
    }
    else if (m_TiledataPtr->Height == 5 && IsSurface() && !IsBackground())
    {
        CanBeTransparent = 1;
    }
    else
    {
        CanBeTransparent = 0;
    }

    if (renderType == ROT_GAME_OBJECT)
    {
        m_TextControl = new CTextContainer(5);

        if (IsSurface() || (IsBackground() && IsUnknown2()))
        {
            CanBeTransparent |= 0x10;
        }
    }
    else
    {
        if (IsWet())
        {
            m_DrawTextureColor[3] = 0xFF;
        }

        if (IsTranslucent())
        {
            m_DrawTextureColor[3] = TRANSLUCENT_ALPHA;
        }

        m_TextControl = new CTextContainer(1);

        if (IsSurface() || (IsBackground() && IsUnknown2()) || IsRoof())
        {
            CanBeTransparent |= 0x10;
        }
    }
}

CRenderStaticObject::~CRenderStaticObject()
{
    DEBUG_TRACE_FUNCTION;
    if (m_TextControl != nullptr)
    {
        delete m_TextControl;
        m_TextControl = nullptr;
    }
}

void CRenderStaticObject::UpdateTextCoordinates()
{
    int offset = 0;

    if (IsGameObject() && ((CGameObject *)this)->Container != 0xFFFFFFFF)
    {
        for (CTextData *item = (CTextData *)m_TextControl->Last(); item != nullptr;
             item = (CTextData *)item->m_Prev)
        {
            CTextData &text = *item;

            if ((offset == 0) && text.Timer < g_Ticks)
            {
                continue;
            }

            offset += text.m_Texture.Height;

            text.RealDrawX = text.GetX() - (text.m_Texture.Width / 2);
            text.RealDrawY = text.GetY() - offset;
        }
    }
    else
    {
        int y = DrawY - (m_TiledataPtr->Height + 20);

        for (CTextData *item = (CTextData *)m_TextControl->Last(); item != nullptr;
             item = (CTextData *)item->m_Prev)
        {
            CTextData &text = *item;

            if ((offset == 0) && text.Timer < g_Ticks)
            {
                continue;
            }

            offset += text.m_Texture.Height;

            text.RealDrawX = DrawX - (text.m_Texture.Width / 2);
            text.RealDrawY = y - offset;
        }
    }
}

void CRenderStaticObject::FixTextCoordinates()
{
    if (IsGameObject() && ((CGameObject *)this)->Container != 0xFFFFFFFF)
    {
        return;
    }

    int offsetY = 0;

    int minX = g_RenderBounds.GameWindowPosX;
    int maxX = minX + g_RenderBounds.GameWindowWidth;
    int minY = g_RenderBounds.GameWindowPosY;
    int maxY = minY + g_RenderBounds.GameWindowHeight;

    for (CTextData *item = (CTextData *)m_TextControl->m_Items; item != nullptr;
         item = (CTextData *)item->m_Next)
    {
        CTextData &text = *item;

        if (text.Timer < g_Ticks)
        {
            continue;
        }

        int startX = text.RealDrawX;
        int endX = startX + text.m_Texture.Width;

        if (startX < minX)
        {
            text.RealDrawX += minX - startX;
        }

        if (endX > maxX)
        {
            text.RealDrawX -= endX - maxX;
        }

        int startY = text.RealDrawY;
        int endY = startY + text.m_Texture.Height;

        if (startY < minY && (offsetY == 0))
        {
            offsetY = minY - startY;
        }

        if (offsetY != 0)
        {
            text.RealDrawY += offsetY;
        }
    }
}

bool CRenderStaticObject::IsNoDrawTile(uint16_t graphic)
{
    switch (graphic)
    {
        case 0x0001:
        case 0x21BC:
        case 0x9E4C:
        case 0x9E64:
        case 0x9E65:
        case 0x9E7D:
            return true;
        default:
            break;
    }

    if (graphic != 0x63D3)
    {
        if (graphic >= 0x2198 && graphic <= 0x21A4)
        {
            return true;
        }

        long long flags = g_Orion.GetStaticFlags(graphic);

        if (!::IsNoDiagonal(flags) ||
            (::IsAnimated(flags) && g_Player != nullptr && g_Player->Race == RT_GARGOYLE))
        {
            return false;
        }
    }

    return true;
}

void CRenderStaticObject::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
#if UO_DEBUG_INFO != 0
    g_RenderedObjectsCountInGameWindow++;
#endif

    bool useAlpha = (m_DrawTextureColor[3] != 0xFF);

    if (useAlpha)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(
            m_DrawTextureColor[0],
            m_DrawTextureColor[1],
            m_DrawTextureColor[2],
            m_DrawTextureColor[3]);
    }

    if (g_UseCircleTrans)
    {
        g_Orion.DrawStaticArtAnimatedTransparent(RenderGraphic, RenderColor, x, y);
    }
    else
    {
        g_Orion.DrawStaticArtAnimated(RenderGraphic, RenderColor, x, y);
    }

    if (useAlpha)
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
    }

    if (IsLightSource() && g_GameScreen.UseLight)
    {
        g_GameScreen.AddLight(this, this, x, y);
    }
}

void CRenderStaticObject::Select(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (m_DrawTextureColor[3] != 0xFF)
    {
        if (!IsTranslucent() || m_DrawTextureColor[3] != TRANSLUCENT_ALPHA)
        {
            return;
        }
    }

    if (!g_UseCircleTrans && g_Orion.StaticPixelsInXYAnimated(RenderGraphic, x, y))
    {
        g_SelectedObject.Init(this);
    }
}

void CRenderStaticObject::AddText(CTextData *msg)
{
    DEBUG_TRACE_FUNCTION;
    if (m_TextControl != nullptr)
    {
        msg->Owner = this;
        m_TextControl->Add(msg);
        Changed = true;

        g_Orion.AddJournalMessage(msg, "You see: ");
    }
}

bool CRenderStaticObject::TextCanBeTransparent(CRenderTextObject *text)
{
    DEBUG_TRACE_FUNCTION;
    bool result = true;

    QFOR(item, m_TextControl->m_Items, CTextData *)
    {
        if (text == item)
        {
            result = false;
            break;
        }
    }

    return result;
}

bool CRenderStaticObject::TranparentTest(int playerZPlus5)
{
    DEBUG_TRACE_FUNCTION;
    bool result = true;

    if (m_Z <= playerZPlus5 - m_TiledataPtr->Height)
    {
        result = false;
    }
    else if (playerZPlus5 < m_Z && ((CanBeTransparent & 0xF) == 0))
    {
        result = false;
    }

    return result;
}

bool CRenderStaticObject::CheckDrawFoliage()
{
    if (IsFoliage())
    {
        if (g_Season < ST_WINTER)
        {
            if (g_ConfigManager.GetDrawStumps())
            {
                return g_Orion.InTileFilter(Graphic);
            }

            return true;
        }

        return false;
    }

    return true;
}

bool CRenderStaticObject::CheckDrawVegetation()
{
    if (g_ConfigManager.GetNoVegetation() && Vegetation)
    {
        return g_Orion.InTileFilter(Graphic);
    }

    return true;
}
