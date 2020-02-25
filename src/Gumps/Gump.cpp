// MIT License
// Copyright (C) August 2016 Hotride

#include "Gump.h"
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

bool CGump::CanBeMoved()
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

void CGump::DrawItems(CBaseGUI *start, int currentPage, int draw2Page)
{
    ScopedPerfMarker(__FUNCTION__);

    CGUIComboBox *combo = nullptr;
    int page = 0;
    bool canDraw = ((draw2Page == 0) || (page >= currentPage && page <= currentPage + draw2Page));

    QFOR(item, start, CBaseGUI *)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));
        }
        else if (canDraw && item->Visible && !item->SelectOnly)
        {
            switch (item->Type)
            {
                case GOT_DATABOX:
                {
                    CGump::DrawItems((CBaseGUI *)item->m_Items, currentPage, draw2Page);
                    break;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)item;

                    float x = (float)htmlGump->GetX();
                    float y = (float)htmlGump->GetY();

#ifndef NEW_RENDERER_ENABLED
                    glTranslatef(x, y, 0.0f);
#else
                    RenderAdd_SetModelViewTranslation(
                        g_renderCmdList, SetModelViewTranslationCmd{ { x, y, 0.f } });
#endif

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

#ifndef NEW_RENDERER_ENABLED
                    glTranslatef(offsetX, offsetY, 0.0f);
                    CGump::DrawItems(subItem, currentPage, draw2Page);
                    g_GL.PopScissor();
                    glTranslatef(-(x + offsetX), -(y + offsetY), 0.0f);
#else
                    RenderAdd_SetModelViewTranslation(
                        g_renderCmdList, SetModelViewTranslationCmd{ { offsetX, offsetY, 0.0f } });
                    CGump::DrawItems(subItem, currentPage, draw2Page);
                    Render_PopScissor();
                    RenderAdd_SetModelViewTranslation(
                        g_renderCmdList,
                        SetModelViewTranslationCmd{ { -(x + offsetX), -(y + offsetY), 0.0f } });
#endif

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

#ifndef NEW_RENDERER_ENABLED
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
#endif
}

CRenderObject *CGump::SelectItems(CBaseGUI *start, int currentPage, int draw2Page)
{
    CRenderObject *selected = nullptr;

    int page = 0;
    bool canDraw = ((draw2Page == 0) || (page >= currentPage && page <= currentPage + draw2Page));
    std::vector<bool> scissorList;
    bool currentScissorState = true;
    CGUIComboBox *combo = nullptr;

    CPoint2Di oldPos = g_MouseManager.Position;

    QFOR(item, start, CBaseGUI *)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));
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

                        selected =
                            CGump::SelectItems((CBaseGUI *)subItem->m_Next, currentPage, draw2Page);
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
                        CGump::SelectItems((CBaseGUI *)item->m_Items, currentPage, draw2Page);

                    if (selectedBox != nullptr)
                    {
                        selected = selectedBox;
                    }

                    break;
                }
                case GOT_COMBOBOX:
                {
                    //selected = ((CGUIComboBox*)item)->SelectedItem();

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
    CGump *gump, CBaseGUI *start, int currentPage, int draw2Page, int count)
{
    int group = 0;
    int page = 0;
    bool canDraw = ((draw2Page == 0) || (page >= currentPage && page <= currentPage + draw2Page));

    static bool htmlTextBackgroundCanBeColored = false;

    if (!(start != nullptr && start->m_Next == nullptr && start->Type == GOT_HTMLTEXT))
    {
        htmlTextBackgroundCanBeColored = false;
    }

    QFOR(item, start, CBaseGUI *)
    {
        if (count == 0)
        {
            break;
        }

        count--;

        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                group = ((CGUIGroup *)item)->Index;
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

                        //if (gump->Page < 1)
                        //	gump->Page = 1;
                    }
                }
                case GOT_COLOREDPOLYGONE:
                {
                    if (((CGUIPolygonal *)item)->CallOnMouseUp)
                    {
                        break;
                    }
                }
                case GOT_RESIZEPIC:
                {
                    uint32_t serial = item->Serial;

                    if (serial == 0u)
                    {
                        break;
                    }

                    int tempPage = -1;
                    bool tempCanDraw = true;

                    QFOR(testItem, start, CBaseGUI *)
                    {
                        if (testItem->Type == GOT_PAGE)
                        {
                            tempPage = ((CGUIPage *)testItem)->Index;

                            tempCanDraw =
                                ((tempPage == -1) ||
                                 ((tempPage >= page && tempPage <= page + draw2Page) ||
                                  ((tempPage == 0) && (draw2Page == 0))));
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
                        gump, (CBaseGUI *)item->m_Items, currentPage, draw2Page);
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

                    TestItemsLeftMouseDown(gump, subItem, currentPage, draw2Page, 5);

                    for (int j = 0; j < 5; j++)
                    {
                        subItem = (CBaseGUI *)subItem->m_Next;
                    }

                    int offsetX = htmlGump->DataOffset.X - htmlGump->CurrentOffset.X;
                    int offsetY = htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y;

                    g_MouseManager.Position = CPoint2Di(
                        g_MouseManager.Position.X - offsetX, g_MouseManager.Position.Y - offsetY);

                    TestItemsLeftMouseDown(gump, subItem, currentPage, draw2Page);

                    g_MouseManager.Position = oldPos;

                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CGump::TestItemsLeftMouseUp(CGump *gump, CBaseGUI *start, int currentPage, int draw2Page)
{
    int group = 0;
    int page = 0;
    bool canDraw = ((draw2Page == 0) || (page >= currentPage && page <= currentPage + draw2Page));

    QFOR(item, start, CBaseGUI *)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));
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

                        //if (gump->Page < 1)
                        //	gump->Page = 1;
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

                        //if (gump->Page < 1)
                        //	gump->Page = 1;
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

                    QFOR(testRadio, start, CBaseGUI *)
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
                            gump, (CBaseGUI *)skillGroup->m_Items, currentPage, draw2Page);
                    }

                    break;
                }
                case GOT_HTMLGUMP:
                case GOT_XFMHTMLGUMP:
                case GOT_XFMHTMLTOKEN:
                {
                    TestItemsLeftMouseUp(gump, (CBaseGUI *)item->m_Items, currentPage, draw2Page);

                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CGump::TestItemsScrolling(
    CGump *gump, CBaseGUI *start, bool up, int currentPage, int draw2Page)
{
    const int delay = SCROLL_LISTING_DELAY / 7;

    int group = 0;
    int page = 0;
    bool canDraw = ((draw2Page == 0) || (page >= currentPage && page <= currentPage + draw2Page));

    QFOR(item, start, CBaseGUI *)
    {
        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                group = ((CGUIGroup *)item)->Index;
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
                        gump, (CBaseGUI *)item->m_Items, up, currentPage, draw2Page);
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

                        TestItemsScrolling(gump, (CBaseGUI *)subItem, up, currentPage, draw2Page);

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
    CGump *gump, CBaseGUI *start, int currentPage, int draw2Page, int count)
{
    int group = 0;
    int page = 0;
    bool canDraw =
        ((page == -1) || ((page == 0) && (draw2Page == 0)) ||
         (page >= currentPage && page <= currentPage + draw2Page));

    QFOR(item, start, CBaseGUI *)
    {
        if (count == 0)
        {
            break;
        }

        count--;

        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));
        }
        else if (canDraw && item->Enabled && item->Visible)
        {
            if (item->Type == GOT_GROUP)
            {
                group = ((CGUIGroup *)item)->Index;
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
                        gump, (CBaseGUI *)item->m_Items, currentPage, draw2Page);
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

                    TestItemsDragging(gump, subItem, currentPage, draw2Page, 5);

                    for (int j = 0; j < 5; j++)
                    {
                        subItem = (CBaseGUI *)subItem->m_Next;
                    }

                    int offsetX = htmlGump->DataOffset.X - htmlGump->CurrentOffset.X;
                    int offsetY = htmlGump->DataOffset.Y - htmlGump->CurrentOffset.Y;

                    g_MouseManager.Position = CPoint2Di(
                        g_MouseManager.Position.X - offsetX, g_MouseManager.Position.Y - offsetY);

                    TestItemsDragging(gump, subItem, currentPage, draw2Page);

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
    QFOR(item, m_Items, CBaseGUI *)
    item->PrepareTextures();
}

bool CGump::EntryPointerHere()
{
    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Visible && item->EntryPointerHere())
        {
            return true;
        }
    }

    return false;
}

void CGump::GenerateFrame(bool stop)
{
#ifndef NEW_RENDERER_ENABLED
    if (!g_GL.Drawing)
    {
        FrameCreated = false;
        WantRedraw = true;
        return;
    }
#endif
    CalculateGumpState();
    PrepareTextures();
    DrawItems((CBaseGUI *)m_Items, Page, Draw2Page);
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

    if (!FrameCreated)
    {
    loc_create_frame:

        if (!m_FrameBuffer.Ready(GumpRect.Size))
        {
            m_FrameBuffer.Init(GumpRect.Size);
        }

        if (m_FrameBuffer.Use())
        {
#ifndef NEW_RENDERER_ENABLED
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glTranslatef(-(float)GumpRect.Position.X, -(float)GumpRect.Position.Y, 0.0f);
#else
            RenderAdd_SetClearColor(g_renderCmdList, SetClearColorCmd{ { 0.f, 0.f, 0.f, 0.f } });
            RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{ ClearRT::ClearRT_Color });
            RenderAdd_SetClearColor(g_renderCmdList, SetClearColorCmd{ g_ColorBlack });
            RenderAdd_SetModelViewTranslation(
                g_renderCmdList,
                SetModelViewTranslationCmd{
                    { -(float)GumpRect.Position.X, -(float)GumpRect.Position.Y, 0.0f } });
#endif

            GenerateFrame(true);

            if (g_DeveloperMode == DM_DEBUGGING)
            {
#ifndef NEW_RENDERER_ENABLED
                if (g_SelectedObject.Gump == this)
                {
                    glColor4f(0.0f, 1.0f, 0.0f, 0.2f);
                }
                else
                {
                    glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
                }

                g_GL.DrawLine(
                    GumpRect.Position.X + 1,
                    GumpRect.Position.Y + 1,
                    GumpRect.Position.X + GumpRect.Size.Width,
                    GumpRect.Position.Y + 1);
                g_GL.DrawLine(
                    GumpRect.Position.X + GumpRect.Size.Width,
                    GumpRect.Position.Y + 1,
                    GumpRect.Position.X + GumpRect.Size.Width,
                    GumpRect.Position.Y + GumpRect.Size.Height);
                g_GL.DrawLine(
                    GumpRect.Position.X + GumpRect.Size.Width,
                    GumpRect.Position.Y + GumpRect.Size.Height,
                    GumpRect.Position.X + 1,
                    GumpRect.Position.Y + GumpRect.Size.Height);
                g_GL.DrawLine(
                    GumpRect.Position.X + 1,
                    GumpRect.Position.Y + GumpRect.Size.Height,
                    GumpRect.Position.X + 1,
                    GumpRect.Position.Y + 1);

                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
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
#endif
            }

#ifndef NEW_RENDERER_ENABLED
            glTranslatef((float)GumpRect.Position.X, (float)GumpRect.Position.Y, 0.0f);
#else
            RenderAdd_SetModelViewTranslation(
                g_renderCmdList,
                SetModelViewTranslationCmd{
                    { (float)GumpRect.Position.X, (float)GumpRect.Position.Y, 0.0f } });
#endif

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

#ifndef NEW_RENDERER_ENABLED
    glTranslatef(posX, posY, 0.0f);

    glEnable(GL_BLEND);
    //glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { posX, posY, 0.0f } });

    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                       BlendFactor::BlendFactor_OneMinusSrcAlpha });
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
#endif

    m_FrameBuffer.Draw(0, 0);

#ifndef NEW_RENDERER_ENABLED
    glDisable(GL_BLEND);
#else
    RenderAdd_DisableBlend(g_renderCmdList);
#endif

    DrawLocker();

#ifndef NEW_RENDERER_ENABLED
    glTranslatef(-posX, -posY, 0.0f);
#else
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { -posX, -posY, 0.0f } });
#endif
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
        selected = SelectItems((CBaseGUI *)m_Items, Page, Draw2Page);
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

    GetItemsSize(this, (CBaseGUI *)m_Items, minPosition, maxPosition, offset, -1, Page, Draw2Page);

    CSize size(maxPosition.X - minPosition.X, maxPosition.Y - minPosition.Y);

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
    int draw2Page)
{
    int page = 0;
    bool canDraw = ((draw2Page == 0) || (page >= currentPage && page <= currentPage + draw2Page));

    QFOR(item, start, CBaseGUI *)
    {
        if (count == 0)
        {
            break;
        }

        count--;

        if (item->Type == GOT_PAGE)
        {
            page = ((CGUIPage *)item)->Index;

            //if (page >= 2 && page > currentPage + draw2Page)
            //	break;

            canDraw =
                ((page == -1) || ((page >= currentPage && page <= currentPage + draw2Page) ||
                                  ((page == 0) && (draw2Page == 0))));

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
                    draw2Page);
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
                    draw2Page);
                break;
            }
            case GOT_SCISSOR:
                ((CGUIScissor *)item)->GumpParent = gump;
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

    TestItemsLeftMouseDown(this, (CBaseGUI *)m_Items, Page, Draw2Page);

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;
}

void CGump::OnLeftMouseButtonUp()
{
    g_CurrentCheckGump = this;
    TestItemsLeftMouseUp(this, (CBaseGUI *)m_Items, Page, Draw2Page);
    TestLockerClick();
    g_CurrentCheckGump = nullptr;
}

void CGump::OnMidMouseButtonScroll(bool up)
{
    g_CurrentCheckGump = this;
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position = CPoint2Di(oldPos.X - m_X, oldPos.Y - m_Y);

    TestItemsScrolling(this, (CBaseGUI *)m_Items, up, Page, Draw2Page);

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;
}

void CGump::OnDragging()
{
    g_CurrentCheckGump = this;
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position = CPoint2Di(oldPos.X - m_X, oldPos.Y - m_Y);

    TestItemsDragging(this, (CBaseGUI *)m_Items, Page, Draw2Page);

    g_MouseManager.Position = oldPos;
    g_CurrentCheckGump = nullptr;
}

void CGump::PasteClipboardData(wstr_t &data)
{
}
