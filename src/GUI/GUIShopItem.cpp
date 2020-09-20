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
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

CGUIShopItem::CGUIShopItem(
    int serial,
    uint16_t graphic,
    uint16_t color,
    int count,
    int price,
    const astr_t &name,
    int x,
    int y)
    : CBaseGUI(GOT_SHOPITEM, serial, graphic, color, x, y)
    , Count(count)
    , Price(price)
    , Name(name)
{
    MoveOnDrag = true;

    CreateCountText(0);
    CreateNameText();
    UpdateOffsets();
}

CGUIShopItem::~CGUIShopItem()
{
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
            case AG_HIGH:
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
    uint16_t textColor = 0x021F;

    if (Selected)
    {
        textColor = 0x0021;
    }

    auto str = Name + " at " + str_from(Price) + "gp";
    g_FontManager.GenerateA(9, m_NameText, str, textColor, 90);
}

void CGUIShopItem::CreateCountText(int lostCount)
{
    uint16_t textColor = 0x021F;

    if (Selected)
    {
        textColor = 0x0021;
    }

    g_FontManager.GenerateA(9, m_CountText, str_from(Count - lostCount), textColor);
}

void CGUIShopItem::PrepareTextures()
{
    if (Serial >= 0x40000000)
    {
        g_Game.ExecuteStaticArt(Graphic);
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
            case AG_HIGH:
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
        const auto ticks = SDL_GetTicks() + 60000;
        g_AnimationManager.ExecuteAnimation({ 1, group, Graphic }, ticks);
    }

    g_Game.ExecuteGump(0x0039);
    g_Game.ExecuteGump(0x003A);
    g_Game.ExecuteGump(0x003B);
}

void CGUIShopItem::SetShaderMode()
{
    if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
    }
}

void CGUIShopItem::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    CGLTexture *th = nullptr;

#ifndef NEW_RENDERER_ENABLED
    glTranslatef((float)m_X, (float)m_Y, 0.0f);

    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { (float)m_X, (float)m_Y, 0.f } });

    ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
    cmd.value.asInt1 = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

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
        uint8_t group = 0;
        switch (g_AnimationManager.GetGroupIndex(Graphic))
        {
            case AG_LOW:
            {
                group = LAG_STAND;
                break;
            }
            case AG_HIGH:
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

        const auto anim = g_AnimationManager.ExecuteAnimation({ 1, group, Graphic }, g_Ticks);
        if (anim != nullptr && anim->FrameCount != 0)
        {
            assert(anim->Frames[0].UserData);
            const auto *originalTexture = (CSprite *)anim->Frames[0].UserData;
            if (originalTexture && originalTexture->Texture != nullptr)
            {
#ifndef NEW_RENDERER_ENABLED
                CGLTexture tex;
                tex.Texture = originalTexture->Texture->Texture;
                if (originalTexture->Width > 35)
                {
                    tex.Width = 35;
                }
                else
                {
                    tex.Width = originalTexture->Width;
                }

                if (originalTexture->Height > 35)
                {
                    tex.Height = 35;
                }
                else
                {
                    tex.Height = originalTexture->Height;
                }

                g_GL.Draw(tex, 2, m_ImageOffset);
                tex.Texture = 0;
#else
                auto quadCmd = DrawQuadCmd{
                    originalTexture->Texture->Texture,
                    2,
                    m_ImageOffset,
                    originalTexture->Width > 35 ? 35 : uint32_t(originalTexture->Width),
                    originalTexture->Height > 35 ? 35 : uint32_t(originalTexture->Height)
                };

                RenderAdd_DrawQuad(g_renderCmdList, quadCmd);
#endif
                //originalTexture.Draw(2, m_ImageOffset, checktrans);
            }
        }
    }

#ifndef NEW_RENDERER_ENABLED
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    cmd.value.asInt1 = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
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
#ifndef NEW_RENDERER_ENABLED
    glTranslatef((float)-m_X, (float)-m_Y, 0.0f);
#else
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { (float)-m_X, (float)-m_Y, 0.0f } });
#endif
}

bool CGUIShopItem::Select()
{
    const int x = g_MouseManager.Position.X - m_X;
    const int y = g_MouseManager.Position.Y - m_Y;
    return (x >= 0 && y >= -10 && x < 200 && y < m_MaxOffset);
}
