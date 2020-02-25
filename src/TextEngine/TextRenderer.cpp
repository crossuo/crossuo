// MIT License
// Copyright (C) August 2016 Hotride

#include "TextRenderer.h"
#include "TextData.h"
#include "../SelectedObject.h"
#include "../Managers/ColorManager.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/GameScreen.h"
#include "../GameObjects/GameWorld.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CTextRenderer g_WorldTextRenderer;

CTextRenderer::CTextRenderer()
{
    m_TextItems = this; // FIXME wtf?
}

CTextRenderer::~CTextRenderer()
{
    m_TextItems = nullptr;
}

CRenderTextObject *CTextRenderer::AddText(CRenderTextObject *obj)
{
    if (obj != nullptr)
    {
        CRenderTextObject *item = m_TextItems;

        if (item != nullptr)
        {
            if (item->m_NextDraw != nullptr)
            {
                CRenderTextObject *next = item->m_NextDraw;

                item->m_NextDraw = obj;
                obj->m_PrevDraw = item;
                obj->m_NextDraw = next;
                next->m_PrevDraw = obj;
            }
            else
            {
                item->m_NextDraw = obj;
                obj->m_PrevDraw = item;
                obj->m_NextDraw = nullptr;
            }
        }
    }

    return obj;
}

void CTextRenderer::ToTop(CRenderTextObject *obj)
{
    obj->UnlinkDraw();

    CRenderTextObject *next = m_TextItems->m_NextDraw;

    m_TextItems->m_NextDraw = obj;
    obj->m_PrevDraw = m_TextItems;
    obj->m_NextDraw = next;

    if (next != nullptr)
    {
        next->m_PrevDraw = obj;
    }
}

bool CTextRenderer::InRect(CTextData *text, CRenderWorldObject *rwo)
{
    bool result = false;
    CTextImageBounds rect(text);

    for (auto it = m_TextRect.begin(); it != m_TextRect.end(); ++it)
    {
        if (it->InRect(rect))
        {
            if (rwo == nullptr || rwo->TextCanBeTransparent(it->Text))
            {
                result = true;
                break;
            }
        }
    }

    AddRect(rect);

    return result;
}

bool CTextRenderer::ProcessTextRemoveBlending(CTextData &text)
{
    if (g_ConfigManager.RemoveTextWithBlending)
    {
        int delta = text.Timer - g_Ticks;

        if (delta >= 0 && delta <= 1000)
        {
            delta = delta / 10;

            if (delta > 100)
            {
                delta = 100;
            }

            if (delta < 1)
            {
                delta = 0;
            }

            delta = (255 * delta) / 100;

            if (!text.Transparent || delta <= 0x7F)
            {
                text.Alpha = (uint8_t)delta;
            }

            text.Transparent = true;

            return true;
        }
    }

    return false;
}

bool CTextRenderer::CalculatePositions(bool noCalculate)
{
    bool changed = false;

    if (!noCalculate)
    {
        ClearRect();
    }

    for (m_DrawPointer = m_TextItems; m_DrawPointer != nullptr;
         m_DrawPointer = m_DrawPointer->m_NextDraw)
    {
        if (!noCalculate && m_DrawPointer->IsText())
        {
            CTextData &text = *(CTextData *)m_DrawPointer;

            if (text.Timer >= g_Ticks)
            {
                CRenderWorldObject *rwo = nullptr;

                if (text.Type == TT_OBJECT)
                {
                    rwo = g_World->FindWorldObject(text.Serial);
                }

                bool transparent = InRect((CTextData *)m_DrawPointer, rwo);

                if (text.Transparent != transparent)
                {
                    changed = true;
                }

                text.Transparent = transparent;

                ProcessTextRemoveBlending(text);
            }
        }

        if (m_DrawPointer->m_NextDraw == nullptr)
        {
            break;
        }
    }

    return changed;
}

void CTextRenderer::Draw()
{
    ScopedPerfMarker(__FUNCTION__);

    CalculatePositions(true);

    for (CRenderTextObject *item = m_DrawPointer; item != nullptr; item = item->m_PrevDraw)
    {
        if (!item->IsText())
        {
            continue;
        }

        CTextData &text = *(CTextData *)item;

        if (text.Timer >= g_Ticks)
        {
            uint16_t textColor = text.Color;

            auto uniformValue = SDM_NO_COLOR;
            if (textColor != 0u)
            {
                g_ColorManager.SendColorsToShader(textColor);

                uniformValue = SDM_COLORED;
                if (text.Unicode)
                {
                    uniformValue = SDM_TEXT_COLORED_NO_BLACK;
                }
                else if (text.Font != 5 && text.Font != 8)
                {
                    uniformValue = SDM_PARTIAL_HUE;
                }
            }

#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, uniformValue);

#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = uniformValue;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

            if (text.Transparent)
            {
                uint8_t alpha = text.Alpha;

                if (alpha == 0xFF)
                {
                    alpha = 0x7F;
                }

#ifndef NEW_RENDERER_ENABLED
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4ub(0xFF, 0xFF, 0xFF, alpha);
#else
                RenderAdd_SetBlend(
                    g_renderCmdList,
                    BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                                   BlendFactor::BlendFactor_OneMinusSrcAlpha });
                RenderAdd_SetColor(
                    g_renderCmdList, SetColorCmd{ { 1.f, 1.f, 1.f, alpha / 255.f } });
#endif

                text.m_TextSprite.Draw(text.RealDrawX, text.RealDrawY);

#ifndef NEW_RENDERER_ENABLED
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glDisable(GL_BLEND);
#else
                RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
                RenderAdd_DisableBlend(g_renderCmdList);
#endif
            }
            else
            {
                text.m_TextSprite.Draw(text.RealDrawX, text.RealDrawY);
            }
        }
    }
}

void CTextRenderer::Select(CGump *gump)
{
    if (gump != nullptr)
    {
        CalculatePositions(true);
    }
    else
    {
        CalculateWorldPositions(true);
    }

    int renderIndex = g_GameScreen.RenderIndex - 1;
    if (renderIndex < 1)
    {
        renderIndex = 99;
    }

    for (CRenderTextObject *item = m_DrawPointer; item != nullptr; item = item->m_PrevDraw)
    {
        if (!item->IsText())
        {
            continue;
        }

        CTextData &text = *(CTextData *)item;
        if (text.Timer >= g_Ticks)
        {
            if (gump == nullptr &&
                (text.Owner == nullptr || text.Owner->UseInRender != renderIndex))
            {
                continue;
            }

            if (text.m_TextSprite.Select(text.RealDrawX, text.RealDrawY))
            {
                g_SelectedObject.Init(item, gump);
            }
        }
    }
}

bool CTextRenderer::CalculateWorldPositions(bool noCalculate)
{
    bool changed = false;

    if (!noCalculate)
    {
        ClearRect();
    }

    for (m_DrawPointer = m_TextItems; m_DrawPointer != nullptr;
         m_DrawPointer = m_DrawPointer->m_NextDraw)
    {
        if (!noCalculate && m_DrawPointer->IsText())
        {
            CTextData &text = *(CTextData *)m_DrawPointer;

            if (text.Timer >= g_Ticks)
            {
                if (text.Owner != nullptr)
                {
                    text.Transparent = InRect((CTextData *)m_DrawPointer, text.Owner);

                    ProcessTextRemoveBlending(text);
                }
            }
        }

        if (m_DrawPointer->m_NextDraw == nullptr)
        {
            break;
        }
    }

    return changed;
}

void CTextRenderer::WorldDraw()
{
    CalculateWorldPositions(true);

    int renderIndex = g_GameScreen.RenderIndex - 1;

    if (renderIndex < 1)
    {
        renderIndex = 99;
    }

    for (CRenderTextObject *item = m_DrawPointer; item != nullptr; item = item->m_PrevDraw)
    {
        if (!item->IsText())
        {
            continue;
        }

        CTextData &text = *(CTextData *)item;

        if (text.Type != TT_SYSTEM && text.Timer >= g_Ticks)
        {
            CRenderWorldObject *rwo = text.Owner;

            if (rwo == nullptr || rwo->UseInRender != renderIndex)
            {
                continue;
            }

            uint16_t textColor = text.Color;

            if (text.Type == TT_OBJECT && g_SelectedObject.Object == item &&
                (((CGameObject *)rwo)->NPC || ((CGameObject *)rwo)->IsCorpse()))
            {
                textColor = 0x0035;
            }

            auto uniformValue = SDM_NO_COLOR;
            if (textColor != 0u)
            {
                g_ColorManager.SendColorsToShader(textColor);

                uniformValue = SDM_COLORED;
                if (text.Unicode)
                {
                    uniformValue = SDM_TEXT_COLORED_NO_BLACK;
                }
                else if (text.Font != 5 && text.Font != 8)
                {
                    uniformValue = SDM_PARTIAL_HUE;
                }
            }
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = uniformValue;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

            if (text.Transparent)
            {
                uint8_t alpha = text.Alpha;

                if (alpha == 0xFF)
                {
                    alpha = 0x7F;
                }

#ifndef NEW_RENDERER_ENABLED
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4ub(0xFF, 0xFF, 0xFF, alpha);
#else
                RenderAdd_SetBlend(
                    g_renderCmdList,
                    BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                                   BlendFactor::BlendFactor_OneMinusSrcAlpha });
                RenderAdd_SetColor(
                    g_renderCmdList, SetColorCmd{ { 1.f, 1.f, 1.f, alpha / 255.f } });
#endif

                text.m_TextSprite.Draw(text.RealDrawX, text.RealDrawY);

#ifndef NEW_RENDERER_ENABLED
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glDisable(GL_BLEND);
#else
                RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
                RenderAdd_DisableBlend(g_renderCmdList);
#endif
            }
            else
            {
                text.m_TextSprite.Draw(text.RealDrawX, text.RealDrawY);
            }
        }
    }
}
