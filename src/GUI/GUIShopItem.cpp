// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIShopItem.h"
#include <SDL_timer.h>
#include <xuocore/uodata.h>
#include "../CrossUO.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/ColorManager.h"
#include "../Managers/FontsManager.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGUIShopItem::CGUIShopItem(
    int serial,
    uint16_t graphic,
    uint16_t color,
    int count,
    int price,
    const std::string &name,
    int x,
    int y)
    : CBaseGUI(GOT_SHOPITEM, serial, graphic, color, x, y)
    , Count(count)
    , Price(price)
    , Name(name)
{
    DEBUG_TRACE_FUNCTION;
    MoveOnDrag = true;

    CreateCountText(0);
    CreateNameText();
    UpdateOffsets();
}

CGUIShopItem::~CGUIShopItem()
{
    DEBUG_TRACE_FUNCTION;
    m_NameText.Clear();
    m_CountText.Clear();
}

void CGUIShopItem::UpdateOffsets()
{
    if (Serial >= 0x40000000)
    {
        auto spr = g_Game.ExecuteStaticArt(Graphic);
        if (spr != nullptr)
        {
            m_MaxOffset = spr->Height;
        }
    }
    else
    {
        uint8_t group = 0;
        switch (g_AnimationManager.GetGroupIndex(Graphic))
        {
            case AG_LOW:
            {
                group = LAG_STAND;
                break;
            }
            case AG_HIGHT:
            {
                group = HAG_STAND;
                break;
            }
            case AG_PEOPLE:
            {
                group = PAG_STAND;
                break;
            }
            default:
                break;
        }

        auto dims = g_AnimationManager.GetAnimationDimensions(0, Graphic, 1, group, false);
        if (dims.Height != 0)
        {
            m_MaxOffset = dims.Height;
            if (m_MaxOffset > 35)
            {
                m_MaxOffset = 35;
            }
        }
    }

    if (m_MaxOffset < m_NameText.Height)
    {
        m_ImageOffset = ((m_NameText.Height - m_MaxOffset) / 2);
        m_MaxOffset = m_NameText.Height;
    }
    else
    {
        m_TextOffset = ((m_MaxOffset - m_NameText.Height) / 2);
    }
}

void CGUIShopItem::OnClick()
{
    DEBUG_TRACE_FUNCTION;
    Selected = false;

    for (CBaseGUI *item = this; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_SHOPITEM && ((CGUIShopItem *)item)->Selected)
        {
            ((CGUIShopItem *)item)->Selected = false;
            ((CGUIShopItem *)item)->CreateNameText();
        }
    }

    for (CBaseGUI *item = this; item != nullptr; item = (CBaseGUI *)item->m_Prev)
    {
        if (item->Type == GOT_SHOPITEM && ((CGUIShopItem *)item)->Selected)
        {
            ((CGUIShopItem *)item)->Selected = false;
            ((CGUIShopItem *)item)->CreateNameText();
        }
    }

    Selected = true;
    CreateNameText();
}

void CGUIShopItem::CreateNameText()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t textColor = 0x021F;

    if (Selected)
    {
        textColor = 0x0021;
    }

    auto str = Name + " at " + std::to_string(Price) + "gp";
    g_FontManager.GenerateA(9, m_NameText, str, textColor, 90);
}

void CGUIShopItem::CreateCountText(int lostCount)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t textColor = 0x021F;

    if (Selected)
    {
        textColor = 0x0021;
    }

    g_FontManager.GenerateA(9, m_CountText, std::to_string(Count - lostCount), textColor);
}

void CGUIShopItem::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;

    if (Serial >= 0x40000000)
    {
        g_Game.ExecuteStaticArt(Graphic);
    }
    else
    {
        uint8_t animGroup = 0;
        switch (g_AnimationManager.GetGroupIndex(Graphic))
        {
            case AG_LOW:
            {
                animGroup = LAG_STAND;
                break;
            }
            case AG_HIGHT:
            {
                animGroup = HAG_STAND;
                break;
            }
            case AG_PEOPLE:
            {
                animGroup = PAG_STAND;
                break;
            }
            default:
                break;
        }
        auto direction = g_AnimationManager.ExecuteAnimation(animGroup, 1, Graphic);
        direction.LastAccessTime = SDL_GetTicks() + 60000;
    }

    g_Game.ExecuteGump(0x0039);
    g_Game.ExecuteGump(0x003A);
    g_Game.ExecuteGump(0x003B);
}

void CGUIShopItem::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;

    if (Color != 0)
    {
#ifndef NEW_RENDERER_ENABLED
        if (PartialHue)
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
        }
        else
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
        }
#else
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
        RenderAdd_SetShaderUniform(
            g_renderCmdList,
            &ShaderUniformCmd(g_ShaderDrawMode, &uniformValue, ShaderUniformType::Int1));
#endif

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        auto uniformValue = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(
            g_renderCmdList,
            &ShaderUniformCmd(g_ShaderDrawMode, &uniformValue, ShaderUniformType::Int1));
#endif
    }
}

void CGUIShopItem::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = nullptr;

    glTranslatef((GLfloat)m_X, (GLfloat)m_Y, 0.0f);

    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);

    m_NameText.Draw(52, m_TextOffset);
    m_CountText.Draw(196 - m_CountText.Width, (m_MaxOffset / 2) - (m_CountText.Height / 2));

    SetShaderMode();

    if (Serial >= 0x40000000)
    {
        auto spr = g_Game.ExecuteStaticArt(Graphic);
        if (spr != nullptr && spr->Texture != nullptr)
        {
            th = spr->Texture;
            th->Draw(2, m_ImageOffset, checktrans);
        }
    }
    else
    {
        uint8_t animGroup = 0;
        switch (g_AnimationManager.GetGroupIndex(Graphic))
        {
            case AG_LOW:
            {
                animGroup = LAG_STAND;
                break;
            }
            case AG_HIGHT:
            {
                animGroup = HAG_STAND;
                break;
            }
            case AG_PEOPLE:
            {
                animGroup = PAG_STAND;
                break;
            }
            default:
                break;
        }

        CTextureAnimationGroup &group = g_Index.m_Anim[Graphic].m_Groups[animGroup];
        CTextureAnimationDirection &direction = group.m_Direction[1];
        if (direction.FrameCount != 0)
        {
            assert(direction.m_Frames[0].UserData);
            auto originalTexture = *(CSprite *)direction.m_Frames[0].UserData;
            if (originalTexture.Texture != nullptr)
            {
#ifndef NEW_RENDERER_ENABLED
                CGLTexture tex;
                tex.Texture = originalTexture.Texture->Texture;
                if (originalTexture.Width > 35)
                {
                    tex.Width = 35;
                }
                else
                {
                    tex.Width = originalTexture.Width;
                }

                if (originalTexture.Height > 35)
                {
                    tex.Height = 35;
                }
                else
                {
                    tex.Height = originalTexture.Height;
                }

                g_GL.Draw(tex, 2, m_ImageOffset);
                tex.Texture = 0;
#else
                auto quadCmd = DrawQuadCmd(
                    originalTexture.Texture->Texture,
                    2,
                    m_ImageOffset,
                    originalTexture.Width > 35 ? 35 : originalTexture.Width,
                    originalTexture.Height > 35 ? 35 : originalTexture.Height);

                RenderAdd_DrawQuad(g_renderCmdList, &quadCmd, 1);
#endif
                //originalTexture.Draw(2, m_ImageOffset, checktrans);
            }
        }
    }

    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
    auto spr = g_Game.ExecuteGump(0x0039);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        spr->Texture->Draw(2, m_MaxOffset, checktrans);
    }

    spr = g_Game.ExecuteGump(0x003A);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        spr->Texture->Draw(32, m_MaxOffset, 140, 0, checktrans);
    }

    spr = g_Game.ExecuteGump(0x003B);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        spr->Texture->Draw(166, m_MaxOffset, checktrans);
    }
    glTranslatef((GLfloat)-m_X, (GLfloat)-m_Y, 0.0f);
}

bool CGUIShopItem::Select()
{
    DEBUG_TRACE_FUNCTION;
    const int x = g_MouseManager.Position.X - m_X;
    const int y = g_MouseManager.Position.Y - m_Y;
    return (x >= 0 && y >= -10 && x < 200 && y < m_MaxOffset);
}
