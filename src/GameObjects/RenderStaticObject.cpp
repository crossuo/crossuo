// MIT License
// Copyright (C) August 2016 Hotride

#include "RenderStaticObject.h"
#include "GamePlayer.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include <xuocore/uodata.h>
#include <xuocore/mulstruct.h>
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/GameScreen.h"
#include "../TextEngine/TextContainer.h"
#include "../TextEngine/TextData.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

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
    m_TiledataPtr = &g_Data.m_Static[graphic];

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

            offset += text.m_TextSprite.Height;

            text.RealDrawX = text.GetX() - (text.m_TextSprite.Width / 2);
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

            offset += text.m_TextSprite.Height;

            text.RealDrawX = DrawX - (text.m_TextSprite.Width / 2);
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
    //int maxY = minY + g_RenderBounds.GameWindowHeight;

    for (CTextData *item = (CTextData *)m_TextControl->m_Items; item != nullptr;
         item = (CTextData *)item->m_Next)
    {
        CTextData &text = *item;

        if (text.Timer < g_Ticks)
        {
            continue;
        }

        int startX = text.RealDrawX;
        int endX = startX + text.m_TextSprite.Width;

        if (startX < minX)
        {
            text.RealDrawX += minX - startX;
        }

        if (endX > maxX)
        {
            text.RealDrawX -= endX - maxX;
        }

        int startY = text.RealDrawY;
        //int endY = startY + text.m_TextSprite.Height;

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

        long long flags = g_Game.GetStaticFlags(graphic);

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
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
#if UO_DEBUG_INFO != 0
    g_RenderedObjectsCountInGameWindow++;
#endif

    bool useAlpha = (m_DrawTextureColor[3] != 0xFF);

    if (useAlpha)
    {
#ifndef NEW_RENDERER_ENABLED
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(
            m_DrawTextureColor[0],
            m_DrawTextureColor[1],
            m_DrawTextureColor[2],
            m_DrawTextureColor[3]);
#else
        RenderAdd_SetBlend(g_renderCmdList, &BlendStateCmd(BlendFunc::SrcAlpha_OneMinusSrcAlpha));
        RenderAdd_SetColor(
            g_renderCmdList,
            &SetColorCmd({ m_DrawTextureColor[0] / 255.f,
                           m_DrawTextureColor[1] / 255.f,
                           m_DrawTextureColor[2] / 255.f,
                           m_DrawTextureColor[3] / 255.f }));
#endif
    }

    if (g_UseCircleTrans)
    {
        g_Game.DrawStaticArtAnimatedTransparent(RenderGraphic, RenderColor, x, y);
    }
    else
    {
        g_Game.DrawStaticArtAnimated(RenderGraphic, RenderColor, x, y);
    }

    if (useAlpha)
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
#else
        RenderAdd_SetColor(g_renderCmdList, &SetColorCmd(g_ColorWhite));
        RenderAdd_DisableBlend(g_renderCmdList);
#endif
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

    if (!g_UseCircleTrans && g_Game.StaticPixelsInXYAnimated(RenderGraphic, x, y))
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

        g_Game.AddJournalMessage(msg, "You see: ");
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
                return g_Game.InTileFilter(Graphic);
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
        return g_Game.InTileFilter(Graphic);
    }

    return true;
}
