// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 CrossUO Contributors

// The 1.25.32 (1998) creation wizard opens modal "scrollchoice" list windows
// over the wizard panels (skills, professions, hair, beard, locations,
// servers). Geometry and behavior follow the original binary:
// - the popup is centered on the 640x480 frame (scrollchoice ctors
//   FUN_0041C940 / FUN_0041CD50: origin = 320-w/2, 240-h/2): the wide 0x4CC
//   art (400x293) at (120,94), the narrow 0x4CA art (231x293) at (205,94)
// - the row page (list/click rect) is rel (34,34)-(360,253) for the wide
//   window and (34,34)-(204,253) for the narrow one; the text rows start at
//   the page's left edge (rel x 34 - not inside the art frame)
// - the row pitch is the font-5 height (the original probes it with the
//   "Aq" ascent/descent template at 0x4A6214; FUN_0041D620: row =
//   (y-top)/pitch + scroll top; the renderer advances rows by the same
//   metric), which gives 9 visible rows
// - the thumb (0x4CB) is a real slider riding a vertical track at rel x 362
//   (wide) / 198 (narrow); the ctor maps value over end = span - knob height
//   (0x4137E5: end = 248-29 = 219), so the knob top rides 36..219 and the
//   slider length passed here is span - track start = 212 (crossuo's
//   CGUISlider subtracts the knob height internally)
// - the up/down arrows are hotspots over the art baked into the backdrop:
//   wide (354,4) 24x18 / (350,264) 22x22; narrow (185,2) 25x18 /
//   (178,264) 26x20. There is no close-X: the owning screen closes the
//   popup when a row is picked.
// The class stays alive across the screen's content rebuilds; scrolling
// refreshes the row textures and hitbox ids in place (no gump rebuild), and
// the owning screens apply picks / close the popup.

#pragma once

#include "Gump.h"
#include "../GUI/GUIHitBox.h"
#include "../GUI/GUISlider.h"
#include "../GUI/GUIText.h"
#include <vector>

class C1998ScrollChoice
{
    //original popup geometry (rel to the art origin, see the header comment)
    static const int PAGE_TOP = 34;
    static const int PAGE_BOTTOM = 253;
    static const int TRACK_TOP = 36;
    static const int TRACK_TRAVEL = 248;

    static constexpr uint16_t HUE_ROW = 0x0386;
    static constexpr uint16_t HUE_PICKED = 0x0021;

public:
    int VisibleRows() const { return m_visibleRows; }
    int Top() const { return m_top; }
    int MaxTop() const
    {
        const int maxTop = int(m_rows.size()) - m_visibleRows;
        return maxTop > 0 ? maxTop : 0;
    }

    bool IsThumb(int serial) const { return m_thumb != nullptr && serial == m_thumb->Serial; }

    bool IsRow(int serial) const
    {
        return serial >= m_rowBaseId && serial < m_rowBaseId + int(m_rows.size());
    }

    //adds the whole popup on the caller's current page; the ids must come
    //from the owning screen's disjoint popup block (never overlapping its
    //panel buttons - the wizard popups live on separate gump pages)
    void Create(
        CGump *gump, int rowBaseId, int arrowUpId, int arrowDownId, int thumbId,
        bool wideBackdrop, const std::vector<astr_t> &rows, int topIndex, int selectedIndex = -1)
    {
        m_rowBaseId = rowBaseId;
        m_rows = rows;
        m_selected = selectedIndex;
        m_top = 0;

        //backdrop: the wide list window sits centered at (120,94), the
        //narrow one at (205,94) (original centered positions)
        int bx = 120;
        const int by = 94;
        int left = 34;
        int right = 360;
        int trackX = 362;
        if (wideBackdrop)
        {
            gump->Add(new CGUIGumppic(0x04CC, bx, by));
        }
        else
        {
            bx = 205;
            right = 204;
            trackX = 198;
            gump->Add(new CGUIGumppic(0x04CA, bx, by));
        }
        m_left = bx + left;
        m_topAbs = by + PAGE_TOP;

        //row pitch: the original's font-5 cell - the "Aq" ascent/descent
        //probe (0x4A6214) measures 25px, giving 9 visible rows in the
        //34..253 window. The host font's rendered-string height runs
        //smaller, which once left the short style lists with nothing to
        //scroll (MaxTop clamped to 0)
        m_pitch = 25;
        m_visibleRows = 9;

        //up/down arrows: hotspots over the arrows baked into the backdrop
        //art (original hotspot rects)
        if (wideBackdrop)
        {
            gump->Add(new CGUIHitBox(arrowUpId, bx + 354, by + 4, 24, 18, true));
            gump->Add(new CGUIHitBox(arrowDownId, bx + 350, by + 264, 22, 22, true));
        }
        else
        {
            gump->Add(new CGUIHitBox(arrowUpId, bx + 185, by + 2, 25, 18, true));
            gump->Add(new CGUIHitBox(arrowDownId, bx + 178, by + 264, 26, 20, true));
        }

        //thumb on the vertical track: the original slider ctor stores end =
        //span - knob height (0x4137E5: 248 - 29 = 219), so the knob top
        //travels 36..219. crossuo's CGUISlider already subtracts the knob
        //height inside CalculateOffset, therefore the length here is the
        //remaining travel 248 - 36 = 212 (not the full span); max value is
        //the row count (FUN_00413C00 / the arrow callbacks 0x411B70/B0)

        //one hitbox + one text object per visible slot; scrolling only
        //rewrites their ids / textures (below). The rows go in BEFORE the
        //thumb so the knob wins the x-overlap on the narrow popup (last-
        //wins hit testing; the original draws its thumb over the rows too)
        m_hits.clear();
        m_texts.clear();
        for (int i = 0; i < m_visibleRows; i++)
        {
            const int y = m_topAbs + i * m_pitch;
            CGUIText *text = new CGUIText(HUE_ROW, m_left, y);
            m_texts.push_back((CGUIText *)gump->Add(text));
            m_hits.push_back(
                (CGUIHitBox *)gump->Add(new CGUIHitBox(rowBaseId + topIndex + i, m_left, y, right - left, m_pitch, true)));
        }

        m_thumb = (CGUISlider *)gump->Add(new CGUISlider(
            thumbId, 0x04CB, 0x04CB, 0x04CB, 0, false, true, bx + trackX, by + TRACK_TOP,
            TRACK_TRAVEL - TRACK_TOP, 0, int(rows.size()), topIndex));

        SetTop(topIndex);
    }

    //drops the cached child pointers (the gump's Clear() freed them); a
    //closed popup keeps its rows for IsRow but must never be treated as
    //having live children - stale m_thumb made IsThumb read freed memory
    //and SetTop crash on freed text pointers
    void Reset()
    {
        m_thumb = nullptr;
        m_hits.clear();
        m_texts.clear();
        m_top = 0;
    }

    //rewrites the windowed rows in place for the new top (no rebuild)
    void SetTop(int topIndex)
    {
        const int count = int(m_rows.size());
        if (topIndex > MaxTop())
        {
            topIndex = MaxTop();
        }
        if (topIndex < 0)
        {
            topIndex = 0;
        }
        m_top = topIndex;

        for (int i = 0; i < int(m_texts.size()); i++)
        {
            const int index = topIndex + i;
            CGUIText *text = m_texts[i];
            if (index < count)
            {
                text->Color = index == m_selected ? HUE_PICKED : HUE_ROW;
                text->CreateTextureA(5, m_rows[index]);
                //center the (tightly rendered) line in its pitch cell
                text->SetY(m_topAbs + i * m_pitch + (m_pitch - text->m_Texture.Height) / 2);
                m_hits[i]->Serial = m_rowBaseId + index;
                m_hits[i]->Visible = true;
                text->Visible = true;
            }
            else
            {
                m_hits[i]->Visible = false;
                text->Visible = false;
            }
        }

        if (m_thumb != nullptr)
        {
            m_thumb->Value = topIndex;
            m_thumb->CalculateOffset();
        }
    }

    //reads the dragged thumb position back into the row window
    void SyncThumb()
    {
        if (m_thumb != nullptr)
        {
            SetTop(m_thumb->Value);
        }
    }

private:
    int m_rowBaseId = 0;
    int m_top = 0;
    int m_left = 0;
    int m_topAbs = 0;
    int m_pitch = 25;
    int m_visibleRows = 9;
    int m_selected = -1;
    std::vector<astr_t> m_rows;
    CGUISlider *m_thumb = nullptr;
    std::vector<CGUIHitBox *> m_hits;
    std::vector<CGUIText *> m_texts;
};
