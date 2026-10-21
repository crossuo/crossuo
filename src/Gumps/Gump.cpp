// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "Gump.h"
#include <algorithm> // std::max
#include "../CrossUO.h"
#include "../Point.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../ClickObject.h"
#include "../GameWindow.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/ConfigManager.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../ScreenStages/BaseScreen.h"
#include "../ScreenStages/GameScreen.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CGump *g_ResizedGump = nullptr;
CGump *g_CurrentCheckGump = nullptr;

#if defined(USE_GL3) || defined(USE_GLES)
// Gumps paint straight into the scene under a scissor (no per-gump
// framebuffer pass/blit). XUO_GUMP_LEGACY=1 opts back into the per-gump
// framebuffer path. Read once; the coordinate-space consequences for child
// items are visible through CGump::DirectDrawActive().
static const bool s_gumpDirectDraw = getenv("XUO_GUMP_LEGACY") == nullptr;
#endif

bool CGump::DirectDrawActive()
{
#if defined(USE_GL3) || defined(USE_GLES)
    return s_gumpDirectDraw;
#else
    return false;
#endif
}

CPoint2Di CGump::GetDrawOriginOffset() const
{
    // Scissor boxes are authored in gump-local coordinates and must be mapped
    // into the space the items were actually drawn in:
    //  - framebuffer pass: items are baked gump-local with a translation of
    //    -GumpRect.Position, so the box shifts by -GumpRect.Position too;
    //  - direct pass: items are drawn at g_GumpTranslate + raw coordinate (the
    //    framebuffer path's GumpRect.Position cancels on both sides, see
    //    CGump::Draw), so the box shifts by the gump position alone.
    if (DirectDrawActive())
    {
        return CPoint2Di((int)g_GumpTranslate.X, (int)g_GumpTranslate.Y);
    }

    return CPoint2Di(-GumpRect.Position.X, -GumpRect.Position.Y);
}

// The gump currently being drawn. CGUIScissor boxes are authored gump-local and
// need that gump's origin to land on the right pixels, but a scissor's
// GumpParent is only assigned as a side effect of GetItemsSize, which walks the
// visible page alone - pages rebuilt on the fly (eg.
// CGumpOptions::RedrawMacroData) never get adopted. Fall back to this so the
// origin is always available while drawing.
static CGump *s_currentDrawGump = nullptr;

CGump *CGump::CurrentDrawGump()
{
    return s_currentDrawGump;
}

#if defined(USE_GL3) || defined(USE_GLES)
// Applies the active gump's draw origin to any scissor pushed while that gump is
// being drawn, so gump item code can keep authoring boxes in gump-local
// coordinates without knowing which pass is active. (GL1 has no scissor origin
// provider in the renderer.)
static void GumpScissorOriginProvider(int *x, int *y)
{
    if (s_currentDrawGump == nullptr)
    {
        return;
    }

    const CPoint2Di origin = s_currentDrawGump->GetDrawOriginOffset();
    *x += origin.X;
    *y += origin.Y;
}
#endif // USE_GL3 || USE_GLES

CGump::CGump()
    : CGump(GT_NONE, 0, 0, 0)
{
}

CGump::CGump(GUMP_TYPE type, uint32_t serial, int x, int y)
    : CRenderObject(serial, 0, 0, x, y)
    , GumpType(type)
{
}

CGump::~CGump()
{
    if (Blocked)
    {
        g_GrayMenuCount--;
        if (g_GrayMenuCount <= 0)
        {
            g_GrayMenuCount = 0;
            g_GameState = GS_GAME;
            g_CurrentScreen = &g_GameScreen;
        }
    }

    if (g_ClickObject.Gump == this)
    {
        g_ClickObject.Clear();
    }

    if (g_SelectedObject.Gump == this)
    {
        g_SelectedObject.Clear();
    }

    if (g_LastSelectedObject.Gump == this)
    {
        g_LastSelectedObject.Clear();
    }

    if (g_PressedObject.LeftGump == this)
    {
        g_PressedObject.ClearLeft();
    }
    if (g_PressedObject.RightGump == this)
    {
        g_PressedObject.ClearRight();
    }
    if (g_PressedObject.MidGump == this)
    {
        g_PressedObject.ClearMid();
    }
}

void CGump::GUMP_DIRECT_HTML_LINK_EVENT_C
{
    g_FontManager.GoToWebLink(link);
    TRACE(Client, "OnDirectHTMLLink(%i)", link);
}

void CGump::FixCoordinates()
{
    const int gumpOffsetX = 40;
    const int gumpOffsetY = 40;
    int maxX = g_GameWindow.GetSize().Width - gumpOffsetX;
    int maxY = g_GameWindow.GetSize().Height - gumpOffsetY;

    if (Minimized && GumpType != GT_MINIMAP)
    {
        if (MinimizedX + GumpRect.Position.X > maxX)
        {
            WantRedraw = true;
            MinimizedX = maxX;
        }

        if (MinimizedX < GumpRect.Position.X &&
            MinimizedX + GumpRect.Position.X + GumpRect.Size.Width - gumpOffsetX < 0)
        {
            WantRedraw = true;
            MinimizedX = gumpOffsetX - (GumpRect.Position.X + GumpRect.Size.Width);
        }

        if (MinimizedY + GumpRect.Position.Y > maxY)
        {
            WantRedraw = true;
            MinimizedY = maxY;
        }

        if (MinimizedY < GumpRect.Position.Y &&
            MinimizedY + GumpRect.Position.Y + GumpRect.Size.Height - gumpOffsetY < 0)
        {
            WantRedraw = true;
            MinimizedY = gumpOffsetY - (GumpRect.Position.Y + GumpRect.Size.Height);
        }
    }
    else
    {
        if (m_X + GumpRect.Position.X > maxX)
        {
            WantRedraw = true;
            m_X = maxX;
        }

        if (m_X < GumpRect.Position.X &&
            m_X + GumpRect.Position.X + GumpRect.Size.Width - gumpOffsetX < 0)
        {
            WantRedraw = true;
            m_X = gumpOffsetX - (GumpRect.Position.X + GumpRect.Size.Width);
        }

        if (m_Y + GumpRect.Position.Y > maxY)
        {
            WantRedraw = true;
            m_Y = maxY;
        }

        if (m_Y < GumpRect.Position.Y &&
            m_Y + GumpRect.Position.Y + GumpRect.Size.Height - gumpOffsetY < 0)
        {
            WantRedraw = true;
            m_Y = gumpOffsetY - (GumpRect.Position.Y + GumpRect.Size.Height);
        }
    }
}

bool CGump::CanBeMoved() const
{
    bool result = true;

    if (NoMove)
    {
        result = false;
    }
    else if (g_ConfigManager.LockGumpsMoving)
    {
        result = !LockMoving;
    }

    return result;
}

void CGump::DrawLocker()
{
    if ((m_Locker.Serial != 0u) && g_ShowGumpLocker)
    {
        g_TextureGumpState[LockMoving].Draw(m_Locker.GetX(), m_Locker.GetY());
    }
}

bool CGump::SelectLocker()
{
    return (
        (m_Locker.Serial != 0u) && g_ShowGumpLocker &&
        g_Game.PolygonePixelsInXY(m_Locker.GetX(), m_Locker.GetY(), 10, 14));
}

bool CGump::TestLockerClick()
{
    bool result =
        ((m_Locker.Serial != 0u) && g_ShowGumpLocker && g_PressedObject.LeftObject == &m_Locker);

    if (result)
    {
        OnButton(m_Locker.Serial);
    }

    return result;
}

void CGump::CalculateGumpState()
{
    g_GumpPressed =
        (!g_ObjectInHand.Enabled &&
         g_PressedObject.LeftGump == this /*&& g_SelectedObject.Gump() == this*/);
    g_GumpSelectedElement = ((g_SelectedObject.Gump == this) ? g_SelectedObject.Object : nullptr);
    g_GumpPressedElement = nullptr;

    CRenderObject *leftObj = g_PressedObject.LeftObject;

    if (g_GumpPressed && leftObj != nullptr)
    {
        if (leftObj == g_SelectedObject.Object)
        {
            g_GumpPressedElement = leftObj;
        }
        else if (leftObj->IsGUI() && ((CBaseGUI *)leftObj)->IsPressedOuthit())
        {
            g_GumpPressedElement = leftObj;
        }
    }

    if (CanBeMoved() && g_GumpPressed && !g_ObjectInHand.Enabled &&
        ((g_PressedObject.LeftSerial == 0u) || g_GumpPressedElement == nullptr ||
         g_PressedObject.TestMoveOnDrag()))
    {
        g_GumpMovingOffset = g_MouseManager.LeftDroppedOffset();
    }
    else
    {
        g_GumpMovingOffset.Reset();
    }

    if (Minimized)
    {
        g_GumpTranslate.X = (float)(MinimizedX + g_GumpMovingOffset.X);
        g_GumpTranslate.Y = (float)(MinimizedY + g_GumpMovingOffset.Y);
    }
    else
    {
        g_GumpTranslate.X = (float)(m_X + g_GumpMovingOffset.X);
        g_GumpTranslate.Y = (float)(m_Y + g_GumpMovingOffset.Y);
    }
}

void CGump::ProcessListing()
{
    if (g_PressedObject.LeftGump != nullptr && !g_PressedObject.LeftGump->NoProcess &&
        g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI())
    {
        CBaseGUI *item = (CBaseGUI *)g_PressedObject.LeftObject;

        if (item->IsControlHTML())
        {
            if (item->Type == GOT_BUTTON)
            {
                ((CGUIHTMLButton *)item)->Scroll(item->Color != 0, SCROLL_LISTING_DELAY / 7);
                g_PressedObject.LeftGump->WantRedraw = true;
                g_PressedObject.LeftGump->OnScrollButton();
            }
            else if (item->Type == GOT_HITBOX)
            {
                ((CGUIHTMLHitBox *)item)->Scroll(item->Color != 0, SCROLL_LISTING_DELAY / 7);
                g_PressedObject.LeftGump->WantRedraw = true;
                g_PressedObject.LeftGump->OnScrollButton();
            }
        }
        else if (item->Type == GOT_BUTTON && ((CGUIButton *)item)->ProcessPressedState)
        {
            g_PressedObject.LeftGump->WantRedraw = true;
            g_PressedObject.LeftGump->OnButton(item->Serial);
        }
        else if (item->Type == GOT_MINMAXBUTTONS)
        {
            ((CGUIMinMaxButtons *)item)->Scroll(SCROLL_LISTING_DELAY / 2);
            g_PressedObject.LeftGump->OnScrollButton();
            g_PressedObject.LeftGump->WantRedraw = true;
        }
        else if (item->Type == GOT_COMBOBOX)
        {
            CGUIComboBox *combo = (CGUIComboBox *)item;

            if (combo->ListingTimer < g_Ticks)
            {
                int index = combo->StartIndex;
                int direction = combo->ListingDirection;

                if (direction == 1 && index > 0)
                {
                    index--;
                }
                else if (direction == 2 && index + 1 < combo->GetItemsCount())
                {
                    index++;
                }

                if (index != combo->StartIndex)
                {
                    if (index < 0)
                    {
                        index = 0;
                    }

                    combo->StartIndex = index;
                    g_PressedObject.LeftGump->WantRedraw = true;
                }

                combo->ListingTimer = g_Ticks + 50;
            }
        }
    }
}

void CGump::DrawItems(CBaseGUI *start, int currentPage, int pageAmount)
{
    ScopedPerfMarker(__FUNCTION__);
    CGUIComboBox *combo = nullptr;
    int page = 0;
    bool canDraw = ((pageAmount == 0) || (page >= currentPage && page <= currentPage + pageAmount));
    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;
            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
        }
        else if (canDraw && item->Visible && !item->SelectOnly)
        {
            switch (item->Type)
            {
                case GOT_DATABOX:
                {
                    CGump::DrawItems((CBaseGUI *)item->m_Items, currentPage, pageAmount);
                    break;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)item;

                    float x = (float)htmlGump->GetX();
                    float y = (float)htmlGump->GetY();

                    RenderAdd_SetModelViewTranslation(
                        g_renderCmdList, SetModelViewTranslationCmd{ { x, y, 0.f } });

                    CBaseGUI *subItem = (CBaseGUI *)htmlGump->m_Items;
                    for (int j = 0; j < 5; j++)
                    {
                        if (subItem->Visible && !subItem->SelectOnly)
                        {
                            subItem->Draw(false);
                        }
                        subItem = (CBaseGUI *)subItem->m_Next;
                    }

                    float offsetX = (float)(htmlGump->DataOffset.X - htmlGump->CurrentOffset.X);
                    float offsetY = (float)(htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y);

                    RenderAdd_SetModelViewTranslation(
                        g_renderCmdList, SetModelViewTranslationCmd{ { offsetX, offsetY, 0.0f } });
                    CGump::DrawItems(subItem, currentPage, pageAmount);
                    Render_PopScissor();
                    RenderAdd_SetModelViewTranslation(
                        g_renderCmdList,
                        SetModelViewTranslationCmd{ { -(x + offsetX), -(y + offsetY), 0.0f } });

                    break;
                }
                case GOT_CHECKTRANS:
                {
                    item->Draw();
                    break;
                }
                case GOT_COMBOBOX:
                {
                    if (g_PressedObject.LeftObject == item)
                    {
                        combo = (CGUIComboBox *)item;
                        break;
                    }
                }
                // fall through
                default:
                {
                    item->Draw();
                    break;
                }
            }
        }
    }

    if (combo != nullptr)
    {
        combo->Draw();
    }

    // sanitize the shared draw state so a leaked drawMode/color from any item
    // cannot tint the draws that follow this batch
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
    //RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{ SDM_NO_COLOR });
}

CRenderObject *CGump::SelectItems(CBaseGUI *start, int currentPage, int pageAmount)
{
    CRenderObject *selected = nullptr;

    int page = 0;
    bool canDraw = ((pageAmount == 0) || (page >= currentPage && page <= currentPage + pageAmount));
    std::vector<bool> scissorList;
    bool currentScissorState = true;
    CGUIComboBox *combo = nullptr;

    CPoint2Di oldPos = g_MouseManager.Position;

    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + pageAmount)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
        }
        else if (canDraw && item->Visible)
        {
            if (item->Type == GOT_SCISSOR)
            {
                if (item->Enabled)
                {
                    currentScissorState = item->Select();
                    scissorList.push_back(currentScissorState);
                }
                else
                {
                    scissorList.pop_back();
                    if (static_cast<unsigned int>(!scissorList.empty()) != 0u)
                    {
                        currentScissorState = scissorList.back();
                    }
                    else
                    {
                        currentScissorState = true;
                    }
                }
                continue;
            }
            if (!currentScissorState || !item->Enabled || (item->DrawOnly && selected != nullptr) ||
                !item->Select())
            {
                continue;
            }

            switch (item->Type)
            {
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)item;
                    g_MouseManager.Position =
                        CPoint2Di(oldPos.X - htmlGump->GetX(), oldPos.Y - htmlGump->GetY());
                    CBaseGUI *subItem = (CBaseGUI *)htmlGump->m_Items;
                    CRenderObject *selectedHTML = nullptr;

                    for (int j = 0; j < 4; j++)
                    {
                        if (subItem->Select())
                        {
                            selectedHTML = subItem;
                        }

                        subItem = (CBaseGUI *)subItem->m_Next;
                    }

                    //Scissor
                    if (subItem->Select())
                    {
                        int offsetX = htmlGump->DataOffset.X - htmlGump->CurrentOffset.X;
                        int offsetY = htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y;
                        g_MouseManager.Position = CPoint2Di(
                            g_MouseManager.Position.X - offsetX,
                            g_MouseManager.Position.Y - offsetY);
                        selected = CGump::SelectItems(
                            (CBaseGUI *)subItem->m_Next, currentPage, pageAmount);
                    }
                    else
                    {
                        selected = nullptr;
                    }

                    if (selected == nullptr)
                    {
                        selected = selectedHTML;
                        if (selected == nullptr)
                        {
                            selected = subItem;
                        }
                    }
                    g_MouseManager.Position = oldPos;
                    break;
                }
                case GOT_DATABOX:
                {
                    CRenderObject *selectedBox =
                        CGump::SelectItems((CBaseGUI *)item->m_Items, currentPage, pageAmount);
                    if (selectedBox != nullptr)
                    {
                        selected = selectedBox;
                    }
                    break;
                }
                case GOT_COMBOBOX:
                {
                    if (g_PressedObject.LeftObject == item)
                    {
                        combo = (CGUIComboBox *)item;
                    }
                    else
                    {
                        selected = item;
                    }
                    break;
                }
                case GOT_SHOPRESULT:
                {
                    selected = ((CGUIShopResult *)item)->SelectedItem();
                    break;
                }
                case GOT_SKILLITEM:
                {
                    selected = ((CGUISkillItem *)item)->SelectedItem();
                    break;
                }
                case GOT_SKILLGROUP:
                {
                    selected = ((CGUISkillGroup *)item)->SelectedItem();
                    break;
                }
                default:
                {
                    selected = item;
                    break;
                }
            }
        }
    }

    if (combo != nullptr)
    {
        selected = combo->SelectedItem();
    }

    return selected;
}

void CGump::TestItemsLeftMouseDown(
    CGump *gump, CBaseGUI *start, int currentPage, int pageAmount, int count)
{
    int page = 0;
    bool canDraw = ((pageAmount == 0) || (page >= currentPage && page <= currentPage + pageAmount));

    static bool htmlTextBackgroundCanBeColored = false;

    if (!(start != nullptr && start->m_Next == nullptr && start->Type == GOT_HTMLTEXT))
    {
        htmlTextBackgroundCanBeColored = false;
    }

    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (count == 0)
        {
            break;
        }

        count--;
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;
            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                continue;
            }
            if (g_SelectedObject.Object != item && !item->IsHTMLGump())
            {
                if (item->Type == GOT_SHOPRESULT)
                {
                    if (g_SelectedObject.Object == ((CGUIShopResult *)item)->m_MinMaxButtons)
                    {
                        CPoint2Di oldPos = g_MouseManager.Position;
                        g_MouseManager.Position = CPoint2Di(
                            g_MouseManager.Position.X - item->GetX(),
                            g_MouseManager.Position.Y - item->GetY());

                        ((CGUIShopResult *)item)->m_MinMaxButtons->OnClick();

                        g_MouseManager.Position = oldPos;
                    }
                    continue;
                }
                if (item->Type != GOT_SKILLGROUP &&
                    item->Type != GOT_DATABOX /*&& item->Type != GOT_TEXTENTRY*/)
                {
                    continue;
                }
            }

            switch (item->Type)
            {
                case GOT_HITBOX:
                {
                    if (((CGUIPolygonal *)item)->CallOnMouseUp)
                    {
                        break;
                    }
                    CGUIHitBox *box = (CGUIHitBox *)item;
                    if (box->ToPage != -1)
                    {
                        gump->Page = box->ToPage;
                    }
                }
                // fall through
                case GOT_COLOREDPOLYGONE:
                {
                    if (((CGUIPolygonal *)item)->CallOnMouseUp)
                    {
                        break;
                    }
                }
                // fall through
                case GOT_RESIZEPIC:
                {
                    uint32_t serial = item->Serial;
                    if (serial == 0u)
                    {
                        break;
                    }

                    int tempPage = -1;
                    bool tempCanDraw = true;
                    for (CBaseGUI *testItem = (CBaseGUI *)start; testItem != nullptr; testItem = (CBaseGUI *)testItem->m_Next)
                    {
                        if (testItem->Type == GOT_PAGE)
                        {
                            tempPage = ((CGUIPage *)testItem)->Index;
                            tempCanDraw =
                                ((tempPage == -1) ||
                                 ((tempPage >= page && tempPage <= page + pageAmount) ||
                                  ((tempPage == 0) && (pageAmount == 0))));
                        }
                        else if (
                            tempCanDraw && testItem->Type == GOT_TEXTENTRY &&
                            testItem->Serial == serial && testItem->Enabled && testItem->Visible)
                        {
                            CGUITextEntry *entry = (CGUITextEntry *)testItem;
                            if (!entry->ReadOnly)
                            {
                                int x = g_MouseManager.Position.X - item->GetX();
                                int y = g_MouseManager.Position.Y - item->GetY();

                                entry->OnClick(gump, x, y);
                            }
                            break;
                        }
                    }
                    gump->OnTextEntry(serial);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_SKILLGROUP:
                {
                    CGUISkillGroup *skillGroup = (CGUISkillGroup *)item;
                    if (g_SelectedObject.Object == skillGroup->m_Name)
                    {
                        gump->OnTextEntry(g_SelectedObject.Object->Serial);
                        gump->WantRedraw = true;
                        return;
                    }
                    break;
                }
                case GOT_RESIZEBUTTON:
                {
                    gump->OnResizeStart(item->Serial);
                    gump->WantRedraw = true;
                    break;
                }
                case GOT_SHOPITEM:
                {
                    ((CGUIShopItem *)item)->OnClick();
                    gump->WantRedraw = true;
                    break;
                }
                case GOT_HTMLTEXT:
                {
                    CGUIHTMLText *htmlText = (CGUIHTMLText *)item;
                    uint16_t link =
                        htmlText->m_Sprite.WebLinkUnderMouse(item->GetX(), item->GetY());

                    if ((link != 0u) && link != 0xFFFF)
                    {
                        gump->OnDirectHTMLLink(link);
                        gump->WantRedraw = true;

                        htmlText->m_Sprite.ClearWebLink();
                        htmlText->Create(htmlTextBackgroundCanBeColored);
                    }
                    break;
                }
                case GOT_DATABOX:
                {
                    CGump::TestItemsLeftMouseDown(
                        gump, (CBaseGUI *)item->m_Items, currentPage, pageAmount);
                    break;
                }
                case GOT_BUTTON:
                case GOT_BUTTONTILEART:
                {
                    gump->WantRedraw = true;
                    break;
                }
                case GOT_TEXTENTRY:
                {
                    CGUITextEntry *entry = (CGUITextEntry *)item;
                    if (!entry->ReadOnly)
                    {
                        int x = g_MouseManager.Position.X - item->GetX();
                        int y = g_MouseManager.Position.Y - item->GetY();

                        entry->OnClick(gump, x, y);
                    }
                    gump->OnTextEntry(item->Serial);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_SLIDER:
                {
                    int x = g_MouseManager.Position.X - item->GetX();
                    int y = g_MouseManager.Position.Y - item->GetY();
                    ((CGUISlider *)item)->OnClick(x, y);
                    gump->OnSliderClick(item->Serial);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_MINMAXBUTTONS:
                {
                    ((CGUIMinMaxButtons *)item)->OnClick();
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_COMBOBOX:
                {
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)item;
                    htmlTextBackgroundCanBeColored = !htmlGump->HaveBackground;
                    CPoint2Di oldPos = g_MouseManager.Position;
                    g_MouseManager.Position =
                        CPoint2Di(oldPos.X - htmlGump->GetX(), oldPos.Y - htmlGump->GetY());
                    CBaseGUI *subItem = (CBaseGUI *)htmlGump->m_Items;

                    TestItemsLeftMouseDown(gump, subItem, currentPage, pageAmount, 5);
                    for (int j = 0; j < 5; j++)
                    {
                        subItem = (CBaseGUI *)subItem->m_Next;
                    }
                    int offsetX = htmlGump->DataOffset.X - htmlGump->CurrentOffset.X;
                    int offsetY = htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y;
                    g_MouseManager.Position = CPoint2Di(
                        g_MouseManager.Position.X - offsetX, g_MouseManager.Position.Y - offsetY);

                    TestItemsLeftMouseDown(gump, subItem, currentPage, pageAmount);
                    g_MouseManager.Position = oldPos;
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CGump::TestItemsLeftMouseUp(CGump *gump, CBaseGUI *start, int currentPage, int pageAmount)
{
    int group = 0;
    int page = 0;
    bool canDraw = ((pageAmount == 0) || (page >= currentPage && page <= currentPage + pageAmount));
    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;
            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                group = ((CGUIGroup *)item)->Index;
                continue;
            }
            if (!item->IsHTMLGump())
            {
                if (item->Type != GOT_DATABOX && item->Type != GOT_COMBOBOX &&
                    item->Type != GOT_SKILLITEM && item->Type != GOT_SKILLGROUP)
                {
                    if (g_PressedObject.LeftObject == item)
                    {
                        if (g_SelectedObject.Object != g_PressedObject.LeftObject &&
                            !item->IsPressedOuthit())
                        {
                            continue;
                        }
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            switch (item->Type)
            {
                case GOT_HITBOX:
                {
                    if (!((CGUIPolygonal *)item)->CallOnMouseUp)
                    {
                        break;
                    }

                    CGUIHitBox *box = (CGUIHitBox *)item;
                    if (box->ToPage != -1)
                    {
                        gump->Page = box->ToPage;
                    }
                    gump->OnButton(item->Serial);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_COLOREDPOLYGONE:
                {
                    if (!((CGUIPolygonal *)item)->CallOnMouseUp)
                    {
                        break;
                    }
                    gump->OnButton(item->Serial);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_RESIZEBUTTON:
                {
                    gump->OnResizeEnd(item->Serial);
                    gump->WantRedraw = true;
                    break;
                }
                case GOT_SLIDER:
                {
                    gump->WantRedraw = true;
                    break;
                }
                case GOT_DATABOX:
                {
                    CGump::TestItemsLeftMouseUp(gump, (CBaseGUI *)item->m_Items, 0);
                    break;
                }
                case GOT_BUTTON:
                case GOT_BUTTONTILEART:
                {
                    if (item->IsControlHTML())
                    {
                        break;
                    }

                    CGUIButton *button = (CGUIButton *)item;
                    if (button->ToPage != -1)
                    {
                        gump->Page = button->ToPage;
                        if (gump->GumpType == GT_GENERIC)
                        {
                            gump->WantUpdateContent = true;
                        }
                    }
                    else
                    {
                        gump->OnButton(item->Serial);
                    }
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_TILEPICHIGHTLIGHTED:
                case GOT_GUMPPICHIGHTLIGHTED:
                {
                    gump->OnButton(item->Serial);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_CHECKBOX:
                {
                    CGUICheckbox *checkbox = (CGUICheckbox *)item;
                    checkbox->Checked = !checkbox->Checked;

                    gump->OnCheckbox(item->Serial, checkbox->Checked);
                    gump->WantRedraw = true;

                    return;
                }
                case GOT_RADIO:
                {
                    CGUIRadio *radio = (CGUIRadio *)item;
                    int radioPage = 0;
                    int radioGroup = 0;
                    for (CBaseGUI *testRadio = (CBaseGUI *)start; testRadio != nullptr; testRadio = (CBaseGUI *)testRadio->m_Next)
                    {
                        if (testRadio->Type == GOT_PAGE)
                        {
                            radioPage = ((CGUIPage *)testRadio)->Index;
                        }
                        else if (testRadio->Type == GOT_GROUP)
                        {
                            radioGroup = ((CGUIGroup *)testRadio)->Index;
                        }
                        else if (testRadio->Type == GOT_RADIO)
                        {
                            if (page <= 1 && radioPage <= 1)
                            {
                                if (group == radioGroup)
                                {
                                    if (((CGUIRadio *)testRadio)->Checked && testRadio != radio)
                                    {
                                        gump->OnRadio(testRadio->Serial, false);
                                    }
                                    ((CGUIRadio *)testRadio)->Checked = false;
                                }
                            }
                            else if (page == radioPage)
                            {
                                if (group == radioGroup)
                                {
                                    if (((CGUIRadio *)testRadio)->Checked && testRadio != radio)
                                    {
                                        gump->OnRadio(testRadio->Serial, false);
                                    }
                                    ((CGUIRadio *)testRadio)->Checked = false;
                                }
                            }
                        }
                    }
                    radio->Checked = true;
                    gump->OnRadio(item->Serial, true);
                    gump->WantRedraw = true;
                    return;
                }
                case GOT_COMBOBOX:
                {
                    CGUIComboBox *combo = (CGUIComboBox *)item;
                    int selectedCombo = combo->IsSelectedItem();
                    if (selectedCombo != -1)
                    {
                        combo->SelectedIndex = selectedCombo;
                        gump->OnComboboxSelection(item->Serial + selectedCombo);
                        gump->WantRedraw = true;
                    }
                    break;
                }
                case GOT_SKILLITEM:
                {
                    CGUISkillItem *skillItem = (CGUISkillItem *)item;
                    if ((g_PressedObject.LeftObject == skillItem->m_ButtonUse &&
                         skillItem->m_ButtonUse != nullptr) ||
                        g_PressedObject.LeftObject == skillItem->m_ButtonStatus)
                    {
                        gump->OnButton(g_PressedObject.LeftSerial);
                        gump->WantRedraw = true;
                    }
                    break;
                }
                case GOT_SKILLGROUP:
                {
                    CGUISkillGroup *skillGroup = (CGUISkillGroup *)item;
                    if (g_PressedObject.LeftObject == skillGroup->m_Minimizer)
                    {
                        gump->OnButton(g_PressedObject.LeftSerial);
                        gump->WantRedraw = true;
                    }
                    else
                    {
                        TestItemsLeftMouseUp(
                            gump, (CBaseGUI *)skillGroup->m_Items, currentPage, pageAmount);
                    }
                    break;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    TestItemsLeftMouseUp(gump, (CBaseGUI *)item->m_Items, currentPage, pageAmount);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CGump::TestItemsScrolling(
    CGump *gump, CBaseGUI *start, bool up, int currentPage, int pageAmount)
{
    const int delay = SCROLL_LISTING_DELAY / 7;

    int page = 0;
    bool canDraw = ((pageAmount == 0) || (page >= currentPage && page <= currentPage + pageAmount));

    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;
            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                continue;
            }
            if (g_SelectedObject.Object != item && !item->IsHTMLGump())
            {
                continue;
            }

            switch (item->Type)
            {
                case GOT_RESIZEPIC:
                {
                    if (!item->IsControlHTML())
                    {
                        break;
                    }

                    CGUIHTMLResizepic *resizepic = (CGUIHTMLResizepic *)item;
                    resizepic->Scroll(up, delay);

                    gump->OnSliderMove(item->Serial);
                    gump->WantRedraw = true;

                    break;
                }
                case GOT_SLIDER:
                {
                    ((CGUISlider *)item)->OnScroll(up, delay);

                    gump->OnSliderMove(item->Serial);
                    gump->WantRedraw = true;

                    return;
                }
                case GOT_DATABOX:
                {
                    CGump::TestItemsScrolling(
                        gump, (CBaseGUI *)item->m_Items, up, currentPage, pageAmount);
                    break;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    if (item->Select())
                    {
                        CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)item;

                        CPoint2Di oldPos = g_MouseManager.Position;
                        g_MouseManager.Position =
                            CPoint2Di(oldPos.X - htmlGump->GetX(), oldPos.Y - htmlGump->GetY());

                        CBaseGUI *subItem = (CBaseGUI *)htmlGump->m_Items;

                        for (int j = 0; j < 5; j++)
                        {
                            if (subItem->Type == GOT_SLIDER)
                            {
                                ((CGUISlider *)subItem)->OnScroll(up, delay);

                                gump->OnSliderMove(subItem->Serial);
                            }

                            subItem = (CBaseGUI *)subItem->m_Next;
                        }

                        int offsetX = htmlGump->DataOffset.X - htmlGump->CurrentOffset.X;
                        int offsetY = htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y;

                        g_MouseManager.Position = CPoint2Di(
                            g_MouseManager.Position.X - offsetX,
                            g_MouseManager.Position.Y - offsetY);

                        TestItemsScrolling(gump, (CBaseGUI *)subItem, up, currentPage, pageAmount);

                        g_MouseManager.Position = oldPos;

                        gump->WantRedraw = true;
                    }

                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CGump::TestItemsDragging(
    CGump *gump, CBaseGUI *start, int currentPage, int pageAmount, int count)
{
    int page = 0;
    bool canDraw =
        ((page == -1) || ((page == 0) && (pageAmount == 0)) ||
         (page >= currentPage && page <= currentPage + pageAmount));

    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (count == 0)
        {
            break;
        }

        count--;
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;
            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                continue;
            }
            if (g_PressedObject.LeftObject != item && !item->IsHTMLGump())
            {
                continue;
            }

            switch (item->Type)
            {
                case GOT_SLIDER:
                {
                    int x = g_MouseManager.Position.X - item->GetX();
                    int y = g_MouseManager.Position.Y - item->GetY();

                    ((CGUISlider *)item)->OnClick(x, y);

                    gump->OnSliderMove(item->Serial);
                    gump->WantRedraw = true;

                    return;
                }
                case GOT_DATABOX:
                {
                    CGump::TestItemsDragging(
                        gump, (CBaseGUI *)item->m_Items, currentPage, pageAmount);
                    break;
                }
                case GOT_RESIZEBUTTON:
                {
                    gump->OnResize(item->Serial);
                    gump->WantRedraw = true;

                    break;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)item;

                    CPoint2Di oldPos = g_MouseManager.Position;
                    g_MouseManager.Position =
                        CPoint2Di(oldPos.X - htmlGump->GetX(), oldPos.Y - htmlGump->GetY());

                    CBaseGUI *subItem = (CBaseGUI *)htmlGump->m_Items;

                    TestItemsDragging(gump, subItem, currentPage, pageAmount, 5);

                    for (int j = 0; j < 5; j++)
                    {
                        subItem = (CBaseGUI *)subItem->m_Next;
                    }

                    int offsetX = htmlGump->DataOffset.X - htmlGump->CurrentOffset.X;
                    int offsetY = htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y;

                    g_MouseManager.Position = CPoint2Di(
                        g_MouseManager.Position.X - offsetX, g_MouseManager.Position.Y - offsetY);

                    TestItemsDragging(gump, subItem, currentPage, pageAmount);

                    g_MouseManager.Position = oldPos;

                    gump->WantRedraw = true;
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CGump::PrepareTextures()
{
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
    item->PrepareTextures();
}

bool CGump::EntryPointerHere()
{
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Visible && item->EntryPointerHere())
        {
            return true;
        }
    }

    return false;
}

void CGump::GenerateFrame()
{
    CalculateGumpState();
    PrepareTextures();

    // scissor components resolve their coordinate space against the gump being
    // drawn; some of them are never adopted by GetItemsSize (see
    // CurrentDrawGump), so publish this gump for the duration of the draw.
    // The renderer reads the same state through the origin provider, so every
    // scissor pushed from inside here is shifted without the caller knowing.
    // (the provider lives in the new GL3/GLES renderer only)
#if defined(USE_GL3) || defined(USE_GLES)
    static const bool providerInstalled = []() {
        Render_SetScissorOriginProvider(GumpScissorOriginProvider);
        return true;
    }();
    (void)providerInstalled;
#endif

    CGump *prevDrawGump = s_currentDrawGump;
    s_currentDrawGump = this;
    DrawItems((CBaseGUI *)m_Items, Page, PageAmount);
    s_currentDrawGump = prevDrawGump;

    WantRedraw = true;
    FrameCreated = true;
}

void CGump::Draw()
{
    ScopedPerfMarker(__FUNCTION__);
    CalculateGumpState();
    if (WantUpdateContent)
    {
        UpdateContent();
        RecalculateSize();
        WantUpdateContent = false;
        FrameCreated = false;
    }

#if defined(USE_GL3) || defined(USE_GLES)
    // The framebuffer path bakes the gump into a texture once and blits that
    // texture on every later frame. There is no such cached artifact here, so
    // the item commands are re-emitted every frame while the (more expensive)
    // content preparation is still gated on the invalidate flags.
    if (s_gumpDirectDraw)
    {
        // The framebuffer path cancels GumpRect.Position on both sides: items
        // are baked at (raw - GumpRect.Position) and the texture is blitted at
        // g_GumpTranslate + GumpRect.Position, so on screen an item lands at
        // g_GumpTranslate + raw. Direct drawing must reproduce exactly that,
        // so the model translation is the gump position alone - adding
        // GumpRect.Position here too would displace every item by it (visible
        // on server gumps whose content does not start at (0,0)).
        const float gx = g_GumpTranslate.X;
        const float gy = g_GumpTranslate.Y;

        RenderAdd_SetModelViewTranslation(
            g_renderCmdList, SetModelViewTranslationCmd{ { gx, gy, 0.0f } });

        // the clip still needs the content's on-screen extent, which starts at
        // GumpRect.Position within the item space (RecalculateSize sets it to
        // the item bounding-box minimum)
        Render_PushScissor(
            (int)(gx + GumpRect.Position.X),
            (int)(gy + GumpRect.Position.Y),
            (uint32_t)GumpRect.Size.Width,
            (uint32_t)GumpRect.Size.Height);

        GenerateFrame();

        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });

        DrawLocker();

        // restore the state the item stream left behind: without this, the
        // locker's blend stays on and bleeds into the next gump's items (the
        // framebuffer path disabled blend right after its composite blit)
        RenderAdd_DisableBlend(g_renderCmdList);
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });

        Render_PopScissor();

        RenderAdd_SetModelViewTranslation(
            g_renderCmdList, SetModelViewTranslationCmd{ { -gx, -gy, 0.0f } });

        FrameCreated = true;
        return;
    }
#endif

    if (!FrameCreated)
    {
    loc_create_frame:

        if (!m_FrameBuffer.Ready(GumpRect.Size))
        {
            m_FrameBuffer.Init(GumpRect.Size);
        }

        if (m_FrameBuffer.Use())
        {
            RenderAdd_SetClearColor(g_renderCmdList, SetClearColorCmd{ { 0.f, 0.f, 0.f, 0.f } });
            RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{ ClearRT::ClearRT_Color });
            RenderAdd_SetClearColor(g_renderCmdList, SetClearColorCmd{ g_ColorBlack });
            RenderAdd_SetModelViewTranslation(
                g_renderCmdList,
                SetModelViewTranslationCmd{
                    { -(float)GumpRect.Position.X, -(float)GumpRect.Position.Y, 0.0f } });

            GenerateFrame();

            if (g_DeveloperMode == DM_DEBUGGING)
            {
                static float4 s_colorThisGump = { 0.0f, 1.0f, 0.0f, 0.2f };
                static float4 s_colorOtherGump = { 1.0f, 1.0f, 1.0f, 0.2f };
                if (g_SelectedObject.Gump != this)
                {
                    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ s_colorOtherGump });
                }
                else
                {
                    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ s_colorThisGump });
                }

                RenderAdd_DrawLine(
                    g_renderCmdList,
                    DrawLineCmd{ GumpRect.Position.X + 1,
                                 GumpRect.Position.Y + 1,
                                 GumpRect.Position.X + GumpRect.Size.Width,
                                 GumpRect.Position.Y + 1 });
                RenderAdd_DrawLine(
                    g_renderCmdList,
                    DrawLineCmd{ GumpRect.Position.X + GumpRect.Size.Width,
                                 GumpRect.Position.Y + 1,
                                 GumpRect.Position.X + GumpRect.Size.Width,
                                 GumpRect.Position.Y + GumpRect.Size.Height });
                RenderAdd_DrawLine(
                    g_renderCmdList,
                    DrawLineCmd{ GumpRect.Position.X + GumpRect.Size.Width,
                                 GumpRect.Position.Y + GumpRect.Size.Height,
                                 GumpRect.Position.X + 1,
                                 GumpRect.Position.Y + GumpRect.Size.Height });
                RenderAdd_DrawLine(
                    g_renderCmdList,
                    DrawLineCmd{ GumpRect.Position.X + 1,
                                 GumpRect.Position.Y + GumpRect.Size.Height,
                                 GumpRect.Position.X + 1,
                                 GumpRect.Position.Y + 1 });

                RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
            }

            RenderAdd_SetModelViewTranslation(
                g_renderCmdList,
                SetModelViewTranslationCmd{
                    { (float)GumpRect.Position.X, (float)GumpRect.Position.Y, 0.0f } });

            m_FrameBuffer.Release();
        }
    }
    else if (WantRedraw)
    {
        WantRedraw = false;
        goto loc_create_frame;
    }

    float posX = g_GumpTranslate.X;
    float posY = g_GumpTranslate.Y;

    posX += (float)GumpRect.Position.X;
    posY += (float)GumpRect.Position.Y;

    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { posX, posY, 0.0f } });

    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                       BlendFactor::BlendFactor_OneMinusSrcAlpha });
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
    //RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{ SDM_NO_COLOR });

    m_FrameBuffer.Draw(0, 0);

    RenderAdd_DisableBlend(g_renderCmdList);

    DrawLocker();

    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { -posX, -posY, 0.0f } });
}

CRenderObject *CGump::Select()
{
    g_CurrentCheckGump = this;
    CalculateGumpState();

    if (WantUpdateContent)
    {
        UpdateContent();
        RecalculateSize();
        WantUpdateContent = false;
        FrameCreated = false;
    }

    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position =
        CPoint2Di(oldPos.X - (int)g_GumpTranslate.X, oldPos.Y - (int)g_GumpTranslate.Y);

    CRenderObject *selected = nullptr;

    if (SelectLocker())
    {
        selected = &m_Locker;
    }
    else if (
        g_MouseManager.Position.X >= GumpRect.Position.X &&
        g_MouseManager.Position.X < GumpRect.Position.X + GumpRect.Size.Width &&
        g_MouseManager.Position.Y >= GumpRect.Position.Y &&
        g_MouseManager.Position.Y < GumpRect.Position.Y + GumpRect.Size.Height)
    {
        selected = SelectItems((CBaseGUI *)m_Items, Page, PageAmount);
    }

    if (selected != nullptr)
    {
        g_SelectedObject.Init(selected, this);
    }

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;

    return selected;
}

void CGump::RecalculateSize()
{
    CPoint2Di minPosition(999, 999);
    CPoint2Di maxPosition;
    CPoint2Di offset;

    GetItemsSize(this, (CBaseGUI *)m_Items, minPosition, maxPosition, offset, -1, Page, PageAmount);
    auto w = maxPosition.X - minPosition.X;
    auto h = maxPosition.Y - minPosition.Y;
    CSize size(std::max(0, w), std::max(0, h));
    GumpRect = CRect(minPosition, size);
}

void CGump::GetItemsSize(
    CGump *gump,
    CBaseGUI *start,
    CPoint2Di &minPosition,
    CPoint2Di &maxPosition,
    CPoint2Di &offset,
    int count,
    int currentPage,
    int pageAmount)
{
    int page = 0;
    bool canDraw = ((pageAmount == 0) || (page >= currentPage && page <= currentPage + pageAmount));

    for (CBaseGUI *item = (CBaseGUI *)start; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (count == 0)
        {
            break;
        }

        count--;
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;
            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + pageAmount) ||
                                  ((page == 0) && (pageAmount == 0))));
            continue;
        }

        if (!canDraw || !item->Visible)
        {
            continue;
        }

        switch (item->Type)
        {
            case GOT_PAGE:
            case GOT_GROUP:
            case GOT_NONE:
            case GOT_MASTERGUMP:
            case GOT_CHECKTRANS:
            case GOT_SHADER:
            case GOT_BLENDING:
            case GOT_GLOBAL_COLOR:
            case GOT_TOOLTIP:
                break;
            case GOT_DATABOX:
            {
                CGump::GetItemsSize(
                    gump,
                    (CBaseGUI *)item->m_Items,
                    minPosition,
                    maxPosition,
                    offset,
                    count,
                    currentPage,
                    pageAmount);
                break;
            }
            case GOT_HTMLGUMP:
            case GOT_XFMHTMLGUMP:
            case GOT_XFMHTMLTOKEN:
            {
                CPoint2Di htmlOffset(offset.X + item->GetX(), offset.X + item->GetY());
                CGump::GetItemsSize(
                    gump,
                    (CBaseGUI *)item->m_Items,
                    minPosition,
                    maxPosition,
                    htmlOffset,
                    5,
                    currentPage,
                    pageAmount);
                break;
            }
            case GOT_SCISSOR:
                ((CGUIScissor *)item)->GumpParent = gump;
                // fall through
            default:
            {
                int x = item->GetX() + offset.X;
                int y = item->GetY() + offset.Y;

                if (x < minPosition.X)
                {
                    minPosition.X = x;
                }

                if (y < minPosition.Y)
                {
                    minPosition.Y = y;
                }

                CSize itemSize = item->GetSize();

                x += itemSize.Width;
                y += itemSize.Height;

                if (x > maxPosition.X)
                {
                    maxPosition.X = x;
                }

                if (y > maxPosition.Y)
                {
                    maxPosition.Y = y;
                }

                break;
            }
        }
    }
}

void CGump::OnLeftMouseButtonDown()
{
    g_CurrentCheckGump = this;
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position = CPoint2Di(oldPos.X - m_X, oldPos.Y - m_Y);

    TestItemsLeftMouseDown(this, (CBaseGUI *)m_Items, Page, PageAmount);

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;
}

void CGump::OnLeftMouseButtonUp()
{
    g_CurrentCheckGump = this;
    TestItemsLeftMouseUp(this, (CBaseGUI *)m_Items, Page, PageAmount);
    TestLockerClick();
    g_CurrentCheckGump = nullptr;
}

void CGump::OnMidMouseButtonScroll(bool up)
{
    g_CurrentCheckGump = this;
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position = CPoint2Di(oldPos.X - m_X, oldPos.Y - m_Y);

    TestItemsScrolling(this, (CBaseGUI *)m_Items, up, Page, PageAmount);

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;
}

void CGump::OnDragging()
{
    g_CurrentCheckGump = this;
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position = CPoint2Di(oldPos.X - m_X, oldPos.Y - m_Y);

    TestItemsDragging(this, (CBaseGUI *)m_Items, Page, PageAmount);

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;
}

void CGump::PasteClipboardData(wstr_t &data)
{
}
