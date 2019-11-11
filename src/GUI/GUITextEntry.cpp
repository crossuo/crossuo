// MIT License
// Copyright (C) August 2016 Hotride

#include "GUITextEntry.h"
#include "../Point.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"
#include "../Managers/MouseManager.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGUITextEntry::CGUITextEntry(
    int serial,
    uint16_t color,
    uint16_t colorSelected,
    uint16_t colorFocused,
    int x,
    int y,
    int maxWidth,
    bool unicode,
    uint8_t font,
    TEXT_ALIGN_TYPE align,
    uint16_t textFlags,
    int maxLength)
    : CBaseGUI(GOT_TEXTENTRY, serial, 0, color, x, y)
    , ColorSelected(colorSelected)
    , ColorFocused(colorFocused)
    , Unicode(unicode)
    , Font(font)
    , Align(align)
    , TextFlags(textFlags)
    , m_Entry(maxLength, maxWidth, maxWidth)
{
}

CGUITextEntry::~CGUITextEntry()
{
    DEBUG_TRACE_FUNCTION;
    m_Entry.Clear();
}

bool CGUITextEntry::EntryPointerHere()
{
    DEBUG_TRACE_FUNCTION;
    return (g_EntryPointer == &m_Entry);
}

CSize CGUITextEntry::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    return CSize(m_Entry.m_Texture.Width, m_Entry.m_Texture.Height);
}

void CGUITextEntry::SetGlobalColor(bool use, int color, int selected, int focused)
{
    DEBUG_TRACE_FUNCTION;
    UseGlobalColor = use;

    if (use)
    {
        GlobalColorR = ToColorR(color);
        GlobalColorG = ToColorG(color);
        GlobalColorB = ToColorB(color);
        GlobalColorA = ToColorA(color);

        if (GlobalColorA == 0u)
        {
            GlobalColorA = 0xFF;
        }

        GlobalColorSelectedR = ToColorR(selected);
        GlobalColorSelectedG = ToColorG(selected);
        GlobalColorSelectedB = ToColorB(selected);
        GlobalColorSelectedA = ToColorA(selected);

        if (GlobalColorSelectedA == 0u)
        {
            GlobalColorSelectedA = 0xFF;
        }

        GlobalColorFocusedR = ToColorR(focused);
        GlobalColorFocusedG = ToColorG(focused);
        GlobalColorFocusedB = ToColorB(focused);
        GlobalColorFocusedA = ToColorA(focused);

        if (GlobalColorFocusedA == 0u)
        {
            GlobalColorFocusedA = 0xFF;
        }
    }
}

void CGUITextEntry::OnClick(CGump *gump, int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    m_Entry.OnClick(gump, Font, Unicode, x, y, Align, TextFlags);
}

void CGUITextEntry::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUITextEntry::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

void CGUITextEntry::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t color = Color;

    if (!UseGlobalColor)
    {
        if (Focused || &m_Entry == g_EntryPointer)
        {
            color = ColorFocused;
        }
        else if (
            g_GumpSelectedElement == this ||
            (CheckOnSerial && g_CurrentCheckGump != nullptr &&
             g_CurrentCheckGump == g_SelectedObject.Gump && Serial == g_SelectedObject.Serial &&
             g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsGUI()))
        {
            color = ColorSelected;
        }
    }

    if ((color != 0u) && Unicode)
    {
        color++;
    }

    if (Unicode)
    {
        m_Entry.PrepareToDrawW(Font, color, Align, TextFlags);
    }
    else
    {
        m_Entry.PrepareToDrawA(Font, color, Align, TextFlags);
    }
}

void CGUITextEntry::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    int y = m_Y;
    uint16_t color = Color;

    if (Focused || &m_Entry == g_EntryPointer)
    {
        if (UseGlobalColor)
        {
#ifndef NEW_RENDERER_ENABLED
            glColor4ub(
                GlobalColorFocusedR, GlobalColorFocusedG, GlobalColorFocusedB, GlobalColorFocusedA);
#else
            RenderAdd_SetColor(
                g_renderCmdList,
                &SetColorCmd({ GlobalColorFocusedR / 255.f,
                               GlobalColorFocusedG / 255.f,
                               GlobalColorFocusedB / 255.f,
                               GlobalColorFocusedA / 255.f }));
#endif
        }
        else
        {
            color = ColorFocused;
        }

        y += FocusedOffsetY;
    }
    else if (
        g_GumpSelectedElement == this ||
        (CheckOnSerial && g_CurrentCheckGump != nullptr &&
         g_CurrentCheckGump == g_SelectedObject.Gump && Serial == g_SelectedObject.Serial &&
         g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsGUI()))
    {
        if (UseGlobalColor)
        {
#ifndef NEW_RENDERER_ENABLED
            glColor4ub(
                GlobalColorSelectedR,
                GlobalColorSelectedG,
                GlobalColorSelectedB,
                GlobalColorSelectedA);
#else
            RenderAdd_SetColor(
                g_renderCmdList,
                &SetColorCmd({ GlobalColorSelectedR / 255.f,
                               GlobalColorSelectedG / 255.f,
                               GlobalColorSelectedB / 255.f,
                               GlobalColorSelectedA / 255.f }));
#endif
        }
        else
        {
            color = ColorSelected;
        }
    }
    else if (UseGlobalColor)
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4ub(GlobalColorR, GlobalColorG, GlobalColorB, GlobalColorA);
#else
        RenderAdd_SetColor(
            g_renderCmdList,
            &SetColorCmd({ GlobalColorR / 255.f,
                           GlobalColorG / 255.f,
                           GlobalColorB / 255.f,
                           GlobalColorA / 255.f }));
#endif
    }

    if ((color != 0u) && Unicode)
    {
        color++;
    }

    if (checktrans)
    {
#ifndef NEW_RENDERER_ENABLED
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else
        RenderAdd_SetBlend(
            g_renderCmdList,
            &BlendStateCmd(
                BlendFactor::BlendFactor_SrcAlpha, BlendFactor::BlendFactor_OneMinusSrcAlpha));
#endif

        if (Unicode)
        {
            m_Entry.DrawW(Font, color, m_X, y, Align, TextFlags);
        }
        else
        {
            m_Entry.DrawA(Font, color, m_X, y, Align, TextFlags);
        }

#ifndef NEW_RENDERER_ENABLED
        glDisable(GL_BLEND);

        glEnable(GL_STENCIL_TEST);
#else
        RenderAdd_DisableBlend(g_renderCmdList);
        // FIXME renderer - what were the original values for stencil func, op, ref and mask?
        RenderAdd_SetStencil(g_renderCmdList, &StencilStateCmd());
#endif

        if (Unicode)
        {
            m_Entry.DrawW(Font, color, m_X, y, Align, TextFlags);
        }
        else
        {
            m_Entry.DrawA(Font, color, m_X, y, Align, TextFlags);
        }

#ifndef NEW_RENDERER_ENABLED
        glDisable(GL_STENCIL_TEST);
#else
        RenderAdd_DisableStencil(g_renderCmdList);
#endif
    }
    else
    {
        if (Unicode)
        {
            m_Entry.DrawW(Font, color, m_X, y, Align, TextFlags);
        }
        else
        {
            m_Entry.DrawA(Font, color, m_X, y, Align, TextFlags);
        }
    }

    if (UseGlobalColor)
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
        RenderAdd_SetColor(g_renderCmdList, &SetColorCmd({ 1.f, 1.f, 1.f, 1.f }));
#endif
    }
}

bool CGUITextEntry::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < m_Entry.m_Texture.Width && y < m_Entry.m_Texture.Height);
}
