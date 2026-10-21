// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "GumpManager.h"
#include "ConfigManager.h"
#include "SkillsManager.h"
#include "MacroManager.h"
#include "OptionsMacroManager.h"
#include "MouseManager.h"
#include <xuocore/mappedfile.h>
#include <xuocore/text_parser.h>
#include "../Point.h"
#include "../CrossUO.h"
#include "../Party.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../GameWindow.h"
#include "../UseItemsList.h"
#include "../ContainerStack.h"
#include "../Container.h"
#include "../TextEngine/GameConsole.h"
#include "../GameObjects/ObjectOnCursor.h"
#include "../GameObjects/GamePlayer.h"
#include "../Gumps/Gump.h"
#include "../Gumps/GumpMenubar.h"
#include "../Gumps/GumpMinimap.h"
#include "../Gumps/GumpSkills.h"
#include "../Gumps/GumpSkill.h"
#include "../Gumps/GumpJournal.h"
#include "../Gumps/GumpBook.h"
#include "../Gumps/GumpSpell.h"
#include "../Gumps/GumpConsoleType.h"
#include "../Gumps/GumpBuff.h"
#include "../Gumps/GumpStatusbar.h"
#include "../Gumps/GumpPaperdoll.h"
#include "../Gumps/GumpWorldMap.h"
#include "../Gumps/GumpAbility.h"
#include "../Gumps/GumpRacialAbility.h"
#include "../Gumps/GumpRacialAbilitiesBook.h"
#include "../Gumps/GumpResourceTracker.h"
#include "../Gumps/GumpPropertyIcon.h"
#include "../Gumps/GumpCombatBook.h"
#include "../Gumps/GumpSecureTrading.h"
#include "../Gumps/GumpCustomHouse.h"
#include "../Gumps/GumpPopupMenu.h"
#include "../Gumps/GumpOptions.h"
#include "../Gumps/GumpMenu.h"
#include "../Gumps/GumpGeneric.h"
#include "../Gumps/GumpSpellbook.h"
#include "../Network/Packets.h"
#include "../Utility/PerfMarker.h"
#if defined(XUO_ASSISTANT)
#	include <managers/menumanager.h>
#endif // xuoassist::g_MenuManager (the menu-replay tap)

CGumpManager g_GumpManager;

int CGumpManager::GetNonpartyStatusbarsCount()
{
    int count = 0;

    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (gump->GumpType == GT_STATUSBAR && gump->Serial != g_PlayerSerial &&
            !g_Party.Contains(gump->Serial))
        {
            count++;
        }
    }

    return count;
}

void CGumpManager::AddGump(CGump *obj)
{
    if (m_Items == nullptr)
    {
        m_Items = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = nullptr;
    }
    else
    {
        bool canCheck =
            (obj->GumpType != GT_GENERIC && obj->GumpType != GT_MENU && obj->GumpType != GT_TRADE &&
             obj->GumpType != GT_BULLETIN_BOARD_ITEM);

        for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
        {
            GUMP_TYPE gumpType = gump->GumpType;
            if (gumpType == obj->GumpType && canCheck)
            {
                if (gump->Serial == obj->Serial)
                {
                    if (gump->CanBeMoved())
                    {
                        if (gumpType == GT_DRAG || gumpType == GT_ABILITY ||
                            gumpType == GT_RACIAL_ABILITY)
                        {
                            gump->SetX(obj->GetX());
                            gump->SetY(obj->GetY());
                        }
                        else if (gumpType == GT_STATUSBAR)
                        {
                            CGumpStatusbar *sb = (CGumpStatusbar *)gump;
                            int gx = obj->GetX();
                            int gy = obj->GetY();
                            if (obj->Minimized)
                            {
                                gx = obj->MinimizedX;
                                gy = obj->MinimizedY;
                            }

                            if (sb->InGroup())
                            {
                                sb->UpdateGroup(-(gump->MinimizedX - gx), -(gump->MinimizedY - gy));
                            }

                            if (gump->Minimized)
                            {
                                gump->MinimizedX = gx;
                                gump->MinimizedY = gy;
                            }
                            else
                            {
                                gump->SetX(gx);
                                gump->SetY(gy);
                            }
                        }
                        else if (gumpType == GT_SPELL)
                        {
                            CGumpSpell *spell = (CGumpSpell *)gump;
                            int gx = obj->GetX();
                            int gy = obj->GetY();
                            if (spell->InGroup())
                            {
                                spell->UpdateGroup(-(gump->GetX() - gx), -(gump->GetY() - gy));
                            }

                            gump->SetX(gx);
                            gump->SetY(gy);
                        }
                    }

                    obj->m_Next = nullptr;
                    delete obj;
                    obj = nullptr;

                    if (gumpType == GT_WORLD_MAP && !((CGumpWorldMap *)gump)->Called)
                    {
                        ((CGumpWorldMap *)gump)->Called = true;
                    }
                    else if (gump->GumpType == GT_POPUP_MENU)
                    {
                        g_PopupMenu = (CGumpPopupMenu *)gump;
                    }
                    else if (
                        gumpType == GT_CONTAINER || gumpType == GT_JOURNAL || gumpType == GT_SKILLS)
                    {
                        gump->Minimized = false;
                        gump->Page = 2;
                    }
                    else if (gumpType == GT_MINIMAP)
                    {
                        gump->Minimized = !gump->Minimized;
                        ((CGumpMinimap *)gump)->LastX = 0;
                    }
                    else if (gumpType == GT_SPELLBOOK && gump->Minimized)
                    {
                        gump->Minimized = false;
                        gump->WantUpdateContent = true;
                    }

                    MoveToBack(gump);
                    gump->FrameCreated = false;
                    return;
                }
            }

            if (gump->m_Next == nullptr)
            {
                gump->m_Next = obj;
                obj->m_Prev = gump;
                obj->m_Next = nullptr;
                break;
            }
        }
    }

    if (obj->GumpType == GT_STATUSBAR && !g_Party.Contains(obj->Serial) &&
        GetNonpartyStatusbarsCount() > 10)
    {
        //RemoveGump(obj);
        for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
        {
            if (gump->GumpType == GT_STATUSBAR && gump->Serial != g_PlayerSerial &&
                !g_Party.Contains(gump->Serial))
            {
                RemoveGump(gump);
                g_Game.CreateTextMessage(
                    TT_SYSTEM, 0xFFFFFFFF, 3, 0, "You have too many non-party status bars");

                break;
            }
        }
        obj->PrepareTextures();
    }
    else
    {
        switch (obj->GumpType)
        {
            case GT_CONTAINER:
            {
                uint16_t sound = g_Container[obj->Graphic].OpenSound;
                if (sound != 0u)
                {
                    g_Game.PlaySoundEffect(sound);
                }
                break;
            }
            case GT_SPELLBOOK:
            {
                //UO->PlaySoundEffect(0x????);
                break;
            }
            case GT_SKILLS:
            {
                ((CGumpSkills *)obj)->Init();
                break;
            }
            case GT_OPTIONS:
            {
                ((CGumpOptions *)obj)->Init();
                break;
            }
            case GT_WORLD_MAP:
            {
                if (g_ConfigManager.GetUseGlobalMapLayer())
                {
                    MoveToFront(obj);
                }
                break;
            }
            default:
                break;
        }
        obj->PrepareTextures();
    }
}

CGump *CGumpManager::GetTextEntryOwner()
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (gump->EntryPointerHere())
        {
            return gump;
        }
    }

    return nullptr;
}

CGump *CGumpManager::GumpExists(uintptr_t gumpID)
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (gumpID == (uintptr_t)gump)
        {
            return gump;
        }
    }

    return nullptr;
}

CGump *CGumpManager::UpdateContent(uint32_t serial, int id, const GUMP_TYPE &type)
{
    CGump *gump = GetGump(serial, id, type);
    if (gump != nullptr)
    {
        gump->WantUpdateContent = true;
    }
    return gump;
}

CGump *CGumpManager::UpdateGump(uint32_t serial, int id, const GUMP_TYPE &type)
{
    CGump *gump = GetGump(serial, id, type);
    if (gump != nullptr)
    {
        gump->WantRedraw = true;
    }
    return gump;
}

CGump *CGumpManager::GetGump(uint32_t serial, int id, const GUMP_TYPE &type)
{
    auto *gump = (CGump *)m_Items;
    while (gump != nullptr)
    {
        if (gump->GumpType == type)
        {
            if (type == GT_RESOURCETRACKER || type == GT_SHOP || type == GT_TARGET_SYSTEM ||
                type == GT_CUSTOM_HOUSE)
            {
                break;
            }
            if (type == GT_TRADE)
            {
                if (gump->Serial == serial)
                {
                    break;
                }
                if ((serial == 0) && (gump->ID == id || ((CGumpSecureTrading *)gump)->ID2 == id))
                {
                    break;
                }
            }
            else if (gump->Serial == serial)
            {
                if (type == GT_CONTAINER || type == GT_SPELLBOOK || type == GT_BULLETIN_BOARD_ITEM)
                {
                    break;
                }
                if (gump->ID == id)
                {
                    break;
                }
            }
        }
        gump = (CGump *)gump->m_Next;
    }
    return gump;
}

void CGumpManager::CloseGump(uint32_t serial, uint32_t id, GUMP_TYPE type)
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr;)
    {
        CGump *next = (CGump *)gump->m_Next;

        if (gump->GumpType == type)
        {
            if (type == GT_TARGET_SYSTEM)
            {
                RemoveGump(gump);
            }
            else if (type == GT_GENERIC && gump->Serial == serial && gump->ID == id)
            {
                RemoveGump(gump);
            }
            else if (type == GT_TRADE && gump->ID == serial)
            {
                RemoveGump(gump);
            }
            else if (type == GT_BULLETIN_BOARD_ITEM && gump->ID == id)
            {
                if (serial == 0xFFFFFFFF || gump->Serial == serial)
                {
                    RemoveGump(gump);
                }
            }
            else if (type == GT_SHOP || type == GT_SPELLBOOK || type == GT_DRAG)
            {
                if (serial != 0u)
                {
                    if (serial == gump->Serial)
                    {
                        RemoveGump(gump);
                    }
                }
                else
                {
                    RemoveGump(gump);
                }
            }
            else if (gump->Serial == serial)
            {
                RemoveGump(gump);
            }
        }

        gump = next;
    }
}

void CGumpManager::RemoveGump(CGump *obj)
{
    Unlink(obj);

    if (g_World != nullptr)
    {
        CGameItem *selobj = g_World->FindWorldItem(obj->Serial);

        if (selobj != nullptr)
        {
            selobj->Dragged = false;
            selobj->Opened = false;
        }
    }

    if (obj->GumpType == GT_CONTAINER && obj->Graphic > 0 && obj->Graphic < g_Container.size())
    {
        uint16_t sound = g_Container[obj->Graphic].CloseSound;
        if (sound != 0u)
        {
            g_Game.PlaySoundEffect(sound);
        }
    }

    obj->m_Next = nullptr;
    obj->m_Prev = nullptr;
    delete obj;
}

void CGumpManager::CloseGenericGumps(uint32_t serial, uint32_t id, bool all)
{
    // close without answering the server: the gumps are only marked for
    // removal (RemoveMarked processes them at the next frame step)
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (gump->GumpType == GT_GENERIC && gump->Serial == serial && gump->ID == id)
        {
            gump->RemoveMark = true;
            if (!all)
            {
                break;
            }
        }
    }
}

void CGumpManager::SelectMenu(uint32_t serial, uint32_t id, int code)
{
    if ((serial == 0u) && (id == 0u))
    {
        for (CGump *gump = (CGump *)m_Items; gump != nullptr;)
        {
            CGump *next = (CGump *)gump->m_Next;
            if (gump->GumpType == GT_MENU || gump->GumpType == GT_GRAY_MENU)
            {
                CPacketMenuResponse(gump, code).Send();
#if defined(XUO_ASSISTANT)
                // the send tap of the (menu round): the assistant replays its
                // menu record before the gump goes away
                xuoassist::g_MenuManager.OnMenuChoiceSent(gump->Serial, gump->ID, code);
#endif
                RemoveGump(gump);
            }
            gump = next;
        }

        return;
    }

    CGump *gump = GetGump(serial, id, GT_MENU);
    if (gump == nullptr)
    {
        gump = GetGump(serial, id, GT_GRAY_MENU);
        if (gump != nullptr)
        {
            CPacketGrayMenuResponse(gump, code).Send();
#if defined(XUO_ASSISTANT)
            xuoassist::g_MenuManager.OnMenuChoiceSent(gump->Serial, gump->ID, code);
#endif
            RemoveGump(gump);
        }
    }
    else
    {
        CPacketMenuResponse(gump, code).Send();
#if defined(XUO_ASSISTANT)
        xuoassist::g_MenuManager.OnMenuChoiceSent(gump->Serial, gump->ID, code);
#endif
        RemoveGump(gump);
    }
}

void CGumpManager::ShowStatusbar(uint32_t serial, int x, int y, bool minimized)
{
    if (serial != g_PlayerSerial)
    {
        minimized = true;
    }

    CGump *gump = UpdateContent(serial, 0, GT_STATUSBAR);
    if (gump != nullptr)
    {
        gump->Minimized = minimized;

        if (gump->Minimized)
        {
            gump->MinimizedX = x;
            gump->MinimizedY = y;
        }
        else
        {
            gump->SetX(x);
            gump->SetY(y);
        }
    }
    else
    {
        CPacketStatusRequest(serial).Send();
        AddGump(new CGumpStatusbar(serial, x, y, minimized));
    }
}

void CGumpManager::MovePaperdoll(uint32_t serial, int x, int y)
{
    CGump *gump = UpdateContent(serial, 0, GT_PAPERDOLL);
    if (gump != nullptr)
    {
        if (gump->Minimized)
        {
            gump->MinimizedX = x;
            gump->MinimizedY = y;
        }
        else
        {
            gump->SetX(x);
            gump->SetY(y);
        }
    }
}

void CGumpManager::SecureTradeCheck(uint32_t id, bool state)
{
    // the send (and the trade-record tap) lives in
    // CGumpSecureTrading::SendTradingResponse
    auto *gump = (CGumpSecureTrading *)UpdateGump(id, 0, GT_TRADE);
    if (gump != nullptr)
    {
        gump->StateMine = state;
        gump->SendTradingResponse(2);
    }
}

void CGumpManager::SecureTradeClose(uint32_t id)
{
    // SendTradingResponse also marks the gump for removal on the close code
    auto *gump = (CGumpSecureTrading *)GetGump(id, 0, GT_TRADE);
    if (gump != nullptr)
    {
        gump->SendTradingResponse(1);
    }
}

void CGumpManager::OnDelete()
{
    CGump *gump = (CGump *)m_Items;

    while (gump != nullptr)
    {
        CGump *tmp = (CGump *)gump->m_Next;

        if (gump->GumpType == GT_STATUSBAR)
        {
            RemoveGump(gump);
        }

        gump = tmp;
    }
}

void CGumpManager::RemoveRangedGumps()
{
    if (g_World != nullptr)
    {
        CGump *gump = (CGump *)m_Items;

        while (gump != nullptr)
        {
            CGump *tmp = (CGump *)gump->m_Next;

            switch (gump->GumpType)
            {
                //case GT_STATUSBAR:
                case GT_PAPERDOLL:
                case GT_MAP:
                case GT_SPELLBOOK:
                case GT_DRAG:
                {
                    if (g_World->FindWorldObject(gump->Serial) == nullptr)
                    {
                        RemoveGump(gump); // Or CloseGump()?
                    }
                    break;
                }
                case GT_TRADE:
                case GT_SHOP:
                {
                    if (GetTopObjDistance(g_Player, g_World->FindWorldObject(gump->Serial)) >
                        MAX_VIEW_RANGE_OLD)
                    {
                        RemoveGump(gump);
                    }
                    break;
                }
                case GT_CONTAINER:
                {
                    if (GetTopObjDistance(g_Player, g_World->FindWorldObject(gump->Serial)) >
                        REMOVE_CONTAINER_GUMP_RANGE)
                    {
                        RemoveGump(gump);
                    }
                    break;
                }
                default:
                    break;
            }

            gump = tmp;
        }
    }
}

void CGumpManager::PrepareContent()
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr;)
    {
        CGump *next = (CGump *)gump->m_Next;

        if (!gump->RemoveMark)
        {
            gump->PrepareContent();
        }

        if (gump->RemoveMark)
        {
            RemoveGump(gump);
        }

        gump = next;
    }
}

void CGumpManager::RemoveMarked()
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr;)
    {
        CGump *next = (CGump *)gump->m_Next;

        if (gump->RemoveMark)
        {
            RemoveGump(gump);
        }

        gump = next;
    }
}

void CGumpManager::PrepareTextures()
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        g_CurrentCheckGump = gump;
        gump->PrepareTextures();
    }

    g_CurrentCheckGump = nullptr;
}

void CGumpManager::Draw(bool blocked)
{
    ScopedPerfMarker(__FUNCTION__);

    CGump *gump = (CGump *)m_Items;
    CGump *menuBarGump = nullptr;

    while (gump != nullptr)
    {
        CGump *next = (CGump *)gump->m_Next;

        if (blocked == gump->Blocked)
        {
            if (gump->CanBeDisplayed())
            {
                if (g_CustomHouseGump == nullptr || gump == g_CustomHouseGump ||
                    gump->GumpType == GT_GENERIC)
                {
                    gump->Draw();
                }
            }

            if (gump->GumpType == GT_MENUBAR)
            {
                menuBarGump = gump;
            }
        }

        gump = next;
    }

    if (menuBarGump != nullptr &&
        (g_CustomHouseGump == nullptr || menuBarGump == g_CustomHouseGump))
    {
        menuBarGump->Draw();
    }
}

void CGumpManager::Select(bool blocked)
{
    CGump *gump = (CGump *)m_Items;
    CGump *menuBarGump = nullptr;

    while (gump != nullptr)
    {
        CGump *next = (CGump *)gump->m_Next;

        if (blocked == gump->Blocked)
        {
            if (gump->CanBeDisplayed())
            {
                if (g_CustomHouseGump == nullptr || gump == g_CustomHouseGump ||
                    gump->GumpType == GT_GENERIC)
                {
                    gump->Select();
                }
            }

            if (gump->GumpType == GT_MENUBAR)
            {
                menuBarGump = gump;
            }
        }

        gump = next;
    }

    if (menuBarGump != nullptr && blocked == menuBarGump->Blocked &&
        (g_CustomHouseGump == nullptr || menuBarGump == g_CustomHouseGump))
    {
        menuBarGump->Select();
    }
}

void CGumpManager::InitToolTip()
{
    CGump *gump = g_SelectedObject.Gump;

    if (gump != nullptr)
    {
        if (!g_ConfigManager.UseToolTips &&
            (!g_TooltipsEnabled || (gump->GumpType != GT_CONTAINER &&
                                    gump->GumpType != GT_PAPERDOLL && gump->GumpType != GT_TRADE)))
        {
            return;
        }

        gump->InitToolTip();
    }
}

void CGumpManager::RedrawAll()
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    gump->WantRedraw = true;
}

void CGumpManager::OnLeftMouseButtonDown(bool blocked)
{
    if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsText())
    {
        return;
    }

    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_SelectedObject.Gump == gump && !gump->NoProcess)
        {
            if (gump->GumpType == GT_STATUSBAR && ((CGumpStatusbar *)gump)->InGroup())
            {
                ((CGumpStatusbar *)gump)->UpdateGroup(0, 0);
            }
            else if (gump->GumpType == GT_SPELL && ((CGumpSpell *)gump)->InGroup())
            {
                ((CGumpSpell *)gump)->UpdateGroup(0, 0);
            }
            else if (gump->GumpType == GT_SKILL && ((CGumpSkill *)gump)->InGroup())
            {
                ((CGumpSkill *)gump)->UpdateGroup(0, 0);
            }
            else
            {
                MoveToBack(gump);
            }

            gump->OnLeftMouseButtonDown();

            break;
        }
    }

    RemoveMarked();
}

bool CGumpManager::OnLeftMouseButtonUp(bool blocked)
{
    if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsText())
    {
        return false;
    }

    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_PressedObject.LeftGump == gump && !gump->NoProcess)
        {
            bool canMove = false;

            if (g_PressedObject.LeftObject != nullptr)
            {
                if (!g_PressedObject.LeftObject->IsText())
                {
                    if (g_PressedObject.LeftObject->Serial == 0u)
                    {
                        canMove = true;
                    }
                    else if (
                        g_PressedObject.LeftObject->IsGUI() &&
                        ((CBaseGUI *)g_PressedObject.LeftObject)->MoveOnDrag)
                    {
                        canMove = true;
                    }
                }
            }
            else
            {
                canMove = true;
            }

            if (canMove && gump->CanBeMoved() && !gump->NoMove && !g_ObjectInHand.Enabled)
            {
                CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

                if (gump->GumpType == GT_STATUSBAR)
                {
                    CGumpStatusbar *sb = (CGumpStatusbar *)gump;

                    if (!gump->Minimized)
                    {
                        sb->RemoveFromGroup();
                        gump->SetX(gump->GetX() + offset.X);
                        gump->SetY(gump->GetY() + offset.Y);

                        gump->FixCoordinates();
                    }
                    else
                    {
                        gump->MinimizedX = gump->MinimizedX + offset.X;
                        gump->MinimizedY = gump->MinimizedY + offset.Y;

                        gump->FixCoordinates();

                        if (sb->InGroup())
                        {
                            sb->UpdateGroup(offset.X, offset.Y);
                        }
                        else
                        {
                            int testX = g_MouseManager.Position.X;
                            int testY = g_MouseManager.Position.Y;

                            CGumpStatusbar *nearBar = sb->GetNearStatusbar(testX, testY);

                            if (nearBar != nullptr)
                            {
                                gump->MinimizedX = testX;
                                gump->MinimizedY = testY;

                                nearBar->AddStatusbar(sb);
                            }
                        }
                    }
                }
                else if (gump->GumpType == GT_SPELL)
                {
                    CGumpSpell *spell = (CGumpSpell *)gump;

                    gump->SetX(gump->GetX() + offset.X);
                    gump->SetY(gump->GetY() + offset.Y);

                    gump->FixCoordinates();

                    if (spell->InGroup())
                    {
                        spell->UpdateGroup(offset.X, offset.Y);
                    }
                    else
                    {
                        int testX = g_MouseManager.Position.X;
                        int testY = g_MouseManager.Position.Y;

                        CGumpSpell *nearSpell = spell->GetNearSpell(testX, testY);

                        if (nearSpell != nullptr)
                        {
                            gump->SetX(testX);
                            gump->SetY(testY);

                            nearSpell->AddSpell(spell);
                        }
                    }
                }
                else if (gump->GumpType == GT_SKILL)
                {
                    CGumpSkill *skillGump = (CGumpSkill *)gump;

                    gump->SetX(gump->GetX() + offset.X);
                    gump->SetY(gump->GetY() + offset.Y);

                    gump->FixCoordinates();

                    if (skillGump->InGroup())
                    {
                        skillGump->UpdateGroup(offset.X, offset.Y);
                    }
                    else
                    {
                        int testX = g_MouseManager.Position.X;
                        int testY = g_MouseManager.Position.Y;

                        CGumpSkill *nearSkill = skillGump->GetNearSkill(testX, testY);

                        if (nearSkill != nullptr)
                        {
                            gump->SetX(testX);
                            gump->SetY(testY);

                            nearSkill->AddSkill(skillGump);
                        }
                    }
                }
                else if (gump->Minimized && gump->GumpType != GT_MINIMAP)
                {
                    gump->MinimizedX = gump->MinimizedX + offset.X;
                    gump->MinimizedY = gump->MinimizedY + offset.Y;

                    gump->FixCoordinates();
                }
                else
                {
                    gump->SetX(gump->GetX() + offset.X);
                    gump->SetY(gump->GetY() + offset.Y);

                    gump->FixCoordinates();
                }

                g_MouseManager.CancelDoubleClick = true;

                //gump->FrameCreated = false;
            }

            if (g_ObjectInHand.Enabled)
            {
                if (g_SelectedObject.Gump == gump)
                {
                    gump->OnLeftMouseButtonUp();
                }
            }
            else
            {
                gump->OnLeftMouseButtonUp();
            }

            if (gump->GumpType == GT_WORLD_MAP && g_ConfigManager.GetUseGlobalMapLayer())
            {
                MoveToFront(gump);
            }

            RemoveMarked();
            //MoveGumpToTop(gump);
            return true;
        }
        if (g_SelectedObject.Gump == gump && g_ObjectInHand.Enabled &&
            (gump->GumpType == GT_PAPERDOLL || gump->GumpType == GT_CONTAINER ||
             gump->GumpType == GT_TRADE || gump->GumpType == GT_RESOURCETRACKER))
        {
            gump->OnLeftMouseButtonUp();
            RemoveMarked();
            return true;
        }
    }

    return false;
}

bool CGumpManager::OnLeftMouseButtonDoubleClick(bool blocked)
{
    bool result = false;

    if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsText())
    {
        return result;
    }

    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_SelectedObject.Gump == gump && !gump->NoProcess)
        {
            if (gump->OnLeftMouseButtonDoubleClick())
            {
                result = true;
            }

            //MoveGumpToTop(gump);
            break;
        }
    }

    RemoveMarked();

    return result;
}

void CGumpManager::OnRightMouseButtonDown(bool blocked)
{
    if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsText())
    {
        return;
    }

    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_SelectedObject.Gump == gump && !gump->NoProcess)
        {
            if (gump->GumpType == GT_STATUSBAR && ((CGumpStatusbar *)gump)->InGroup())
            {
                ((CGumpStatusbar *)gump)->UpdateGroup(0, 0);
            }
            else if (gump->GumpType == GT_SPELL && ((CGumpSpell *)gump)->InGroup())
            {
                ((CGumpSpell *)gump)->UpdateGroup(0, 0);
            }
            else if (gump->GumpType == GT_SKILL && ((CGumpSkill *)gump)->InGroup())
            {
                ((CGumpSkill *)gump)->UpdateGroup(0, 0);
            }
            else
            {
                MoveToBack(gump);
            }

            g_MouseManager.CancelDoubleClick = true;

            break;
        }
    }

    RemoveMarked();
}

void CGumpManager::OnRightMouseButtonUp(bool blocked)
{
    if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsText())
    {
        return;
    }

    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_PressedObject.RightGump == gump && !gump->NoProcess && !gump->NoClose &&
            (gump->CanBeMoved() || gump->GumpType == GT_GENERIC ||
             gump->GumpType == GT_RACE_CHANGE))
        {
            //gump->OnClose();
            switch (gump->GumpType)
            {
                case GT_CONTAINER:
                {
                    CGameItem *obj = g_World->FindWorldItem(gump->Serial);

                    if (obj != nullptr)
                    {
                        if (obj->IsCorpse())
                        {
                            for (CGameItem *gi = (CGameItem *)obj->m_Items; gi != nullptr;)
                            {
                                CGameItem *next = (CGameItem *)gi->m_Next;

                                if (gi->Layer == OL_NONE)
                                {
                                    g_World->RemoveObject(gi);
                                }

                                gi = next;
                            }
                        }
                        else
                        {
                            obj->Clear();
                        }

                        obj->Opened = false;
                        CloseGump(gump->Serial, 0, GT_CONTAINER);
                    }

                    break;
                }
                case GT_MENUBAR:
                {
                    gump->SetX(0);
                    gump->SetY(0);
                    gump->FrameCreated = false;

                    break;
                }
                case GT_BUFF:
                {
                    //gump->SetX(g_GameWindowPosX);
                    //gump->SetY(g_GameWindowPosY + g_GameWindowHeight);
                    gump->FrameCreated = false;
                    g_ConfigManager.ToggleBufficonWindow = false;

                    break;
                }
                case GT_MENU:
                {
                    ((CGumpMenu *)gump)->SendMenuResponse(0);

                    break;
                }
                case GT_PROPERTY_ICON:
                {
                    if (g_ConfigManager.GetItemPropertiesMode() == OPM_ALWAYS_UP)
                    {
                        g_ConfigManager.SetItemPropertiesMode(OPM_FOLLOW_MOUSE);
                    }

                    break;
                }
                case GT_TRADE:
                {
                    ((CGumpSecureTrading *)gump)->SendTradingResponse(1);

                    break;
                }
                case GT_RACE_CHANGE:
                {
                    //non-movable dialog that must still close on right click;
                    //CloseGump deletes the gump, its destructor restores the
                    //borrowed character creation state
                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
                case GT_GENERIC:
                {
                    ((CGumpGeneric *)gump)->SendGumpResponse(0);

                    break;
                }
                case GT_OPTIONS:
                {
                    g_OptionsMacroManager.Clear();

                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
                case GT_STATUSBAR:
                {
                    CGumpStatusbar *sb = ((CGumpStatusbar *)gump)->GetTopStatusbar();

                    if (sb != nullptr)
                    {
                        while (sb != nullptr)
                        {
                            CGumpStatusbar *next = sb->m_GroupNext;

                            sb->RemoveFromGroup();
                            RemoveGump(sb);

                            sb = next;
                        }
                    }
                    else
                    {
                        RemoveGump(gump);
                    }

                    break;
                }
                /*case GT_SPELL:
				{
					CGumpSpell *spell = ((CGumpSpell*)gump)->GetTopSpell();

					if (spell != nullptr)
					{
						while (spell != nullptr)
						{
							CGumpSpell *next = spell->m_GroupNext;

							spell->RemoveFromGroup();
							RemoveGump(spell);

							spell = next;
						}
					}
					else
						RemoveGump(gump);

					break;
				}*/
                case GT_SPELLBOOK:
                {
                    g_Game.PlaySoundEffect(0x0055);
                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
                case GT_BOOK:
                {
                    ((CGumpBook *)gump)->ChangePage(0);
                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
                case GT_CHAT:
                {
                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
                case GT_RESOURCETRACKER:
                {
                    // right click over a tracked counter erases it; over an
                    // empty cell it closes the tracker
                    if (((CGumpResourceTracker *)gump)->RightClickErase())
                    {
                        break;
                    }

                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
                default:
                {
                    CloseGump(gump->Serial, gump->ID, gump->GumpType);

                    break;
                }
            }

            break;
        }
    }

    RemoveMarked();
}

void CGumpManager::OnMidMouseButtonScroll(bool up, bool blocked)
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_SelectedObject.Gump == gump && !gump->NoProcess)
        {
            gump->OnMidMouseButtonScroll(up);

            break;
        }
    }
}

void CGumpManager::OnDragging(bool blocked)
{
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        if (g_PressedObject.LeftGump == gump && !gump->NoProcess)
        {
            gump->OnDragging();

            break;
        }
    }
}

bool CGumpManager::OnTextInput(const TextEvent &ev, bool blocked)
{
    CGump *gump = GetTextEntryOwner();
    bool result = false;
    if (gump != nullptr && !gump->NoProcess)
    {
        if (g_GameState == GS_GAME_BLOCKED)
        {
            if (gump->GumpType == GT_TEXT_ENTRY_DIALOG)
            {
                gump->OnTextInput(ev);
                result = true;
            }
        }
        else
        {
            gump->OnTextInput(ev);
            result = true;
        }
    }

    RemoveMarked();
    return result;
}

bool CGumpManager::OnKeyDown(const KeyEvent &ev, bool blocked)
{
    bool result = false;
    const auto key = EvKey(ev);
    if (g_EntryPointer != nullptr && g_EntryPointer != &g_GameConsole)
    {
        CGump *gump = GetTextEntryOwner();
        if (gump != nullptr && !gump->NoProcess)
        {
            switch (gump->GumpType)
            {
                case GT_STATUSBAR:
                case GT_DRAG:
                case GT_OPTIONS:
                case GT_SKILLS:
                case GT_PROFILE:
                case GT_BULLETIN_BOARD_ITEM:
                case GT_GENERIC:
                case GT_BOOK:
                case GT_CHAT:
                {
                    gump->OnKeyDown(ev);
                    result = true;
                    break;
                }
                default:
                    break;
            }
        }
        else if (key == KEY_DELETE)
        {
            gump = GetGump(0, 0, GT_SKILLS);
            if (gump != nullptr && !gump->NoProcess)
            {
                gump->OnKeyDown(ev);
            }
        }
    }
    else if (key == KEY_DELETE)
    {
        CGump *gump = GetGump(0, 0, GT_SKILLS);
        if (gump != nullptr && !gump->NoProcess)
        {
            gump->OnKeyDown(ev);
        }
    }

    RemoveMarked();
    return result;
}

void CGumpManager::Load(const fs_path &path)
{
    bool paperdollRequested = false;
    bool menubarFound = false;
    bool bufficonWindowFound = false;
    bool minimizedConsoleType = false;
    bool showFullTextConsoleType = false;

    if (fs_path_exists(path))
    {
        TextFileParser file(path, " =", "#;", "\"");

        int spellGroupMembers = 0;
        CGumpSpell *topSpell = nullptr;
        int skillGroupMembers = 0;
        CGumpSkill *topSkill = nullptr;
        bool menubarLoaded = false;

        while (!file.IsEOF())
        {
            auto v = file.ReadTokens();
            if (v.empty())
            {
                continue;
            }

            const astr_t &key = v[0];

            // fields may be comma packed or space separated
            if (key == "Gump" && v.size() >= 2)
            {
                const auto fields = TextFields(v);
                if (fields.size() < 7)
                {
                    continue;
                }

                CGump *gump = nullptr;
                const auto gumpType = (GUMP_TYPE)str_to_int(fields[0]);
                const auto gumpX = (uint16_t)str_to_int(fields[1]);
                const auto gumpY = (uint16_t)str_to_int(fields[2]);
                const auto gumpMinimized = str_to_int(fields[3]);
                const auto gumpMinimizedX = (uint16_t)str_to_int(fields[4]);
                const auto gumpMinimizedY = (uint16_t)str_to_int(fields[5]);
                const auto gumpLockMoving = str_to_int(fields[6]);
                auto extra = [&fields](size_t idx) -> astr_t
                {
                    return idx < fields.size() ? fields[idx] : astr_t("0");
                };

                switch (gumpType)
                {
                    case GT_PAPERDOLL:
                    {
                        gump = new CGumpPaperdoll(g_PlayerSerial, gumpX, gumpY, gumpMinimized != 0);
                        g_Game.PaperdollReq(g_PlayerSerial);
                        paperdollRequested = true;
                        break;
                    }
                    case GT_STATUSBAR:
                    {
                        gump = new CGumpStatusbar(g_PlayerSerial, gumpX, gumpY, gumpMinimized != 0);
                        break;
                    }
                    case GT_MINIMAP:
                    {
                        gump = new CGumpMinimap(gumpX, gumpY, gumpMinimized != 0);
                        break;
                    }
                    case GT_SKILLS:
                    {
                        gump = new CGumpSkills(
                            gumpX, gumpY, gumpMinimized != 0, str_to_int(extra(7)));
                        gump->Visible = false;
                        g_SkillsManager.SkillsRequested = true;
                        CPacketSkillsRequest(g_PlayerSerial).Send();
                        break;
                    }
                    case GT_JOURNAL:
                    {
                        gump = new CGumpJournal(
                            gumpX, gumpY, gumpMinimized != 0, str_to_int(extra(7)));
                        break;
                    }
                    case GT_RESOURCETRACKER:
                    {
                        gump = new CGumpResourceTracker(gumpX, gumpY);
                        break;
                    }
                    case GT_MENUBAR:
                    {
                        if (!g_ConfigManager.DisableMenubar && g_Config.ClientVersion >= CV_500A)
                        {
                            menubarFound = true;
                            gump = new CGumpMenubar(gumpX, gumpY);
                            ((CGumpMenubar *)gump)->SetOpened(gumpMinimized != 0);
                        }

                        break;
                    }
                    case GT_BUFF:
                    {
                        bufficonWindowFound = true;
                        gump = new CGumpBuff(gumpX, gumpY);
                        gump->Graphic = (uint16_t)str_to_int(extra(7));
                        break;
                    }
                    case GT_ABILITY:
                    {
                        gump = new CGumpAbility(TextToUInt(extra(7)), gumpX, gumpY);
                        break;
                    }
                    case GT_PROPERTY_ICON:
                    {
                        gump = new CGumpPropertyIcon(gumpX, gumpY);
                        break;
                    }
                    case GT_COMBAT_BOOK:
                    {
                        gump = new CGumpCombatBook(gumpX, gumpY);
                        gump->Minimized = (gumpMinimized != 0);
                        break;
                    }
                    case GT_CONTAINER:
                    case GT_SPELLBOOK:
                    {
                        const uint32_t serial = TextToUInt(extra(7));
                        g_ContainerStack.push_back(CContainerStackItem(
                            serial,
                            gumpX,
                            gumpY,
                            gumpMinimizedX,
                            gumpMinimizedY,
                            gumpMinimized != 0,
                            gumpLockMoving != 0));

                        g_UseItemActions.Add(serial);
                        break;
                    }
                    case GT_SPELL:
                    {
                        const uint32_t serial = TextToUInt(extra(7));
                        const uint16_t graphic = (uint16_t)str_to_int(extra(8));
                        const SPELLBOOK_TYPE spellType = (SPELLBOOK_TYPE)str_to_int(extra(9));
                        gump = new CGumpSpell(serial, spellType, graphic, gumpX, gumpY);
                        break;
                    }
                    case GT_CONSOLE_TYPE:
                    {
                        minimizedConsoleType = (gumpMinimized != 0);
                        showFullTextConsoleType = (str_to_int(extra(7)) != 0);
                        break;
                    }
                    case GT_SKILL:
                    {
                        gump = new CGumpSkill(TextToUInt(extra(7)), gumpX, gumpY);
                        break;
                    }
                    case GT_RACIAL_ABILITIES_BOOK:
                    {
                        gump = new CGumpRacialAbilitiesBook(gumpX, gumpY);
                        gump->Minimized = (gumpMinimized != 0);
                        break;
                    }
                    case GT_RACIAL_ABILITY:
                    {
                        gump = new CGumpRacialAbility(TextToUInt(extra(7)), gumpX, gumpY);
                        break;
                    }
                    default:
                        break;
                }

                if (gump != nullptr)
                {
                    if (gumpType == GT_MENUBAR)
                    {
                        if (menubarLoaded)
                        {
                            delete gump;
                            continue;
                        }

                        menubarLoaded = true;
                    }

                    gump->MinimizedX = gumpMinimizedX;
                    gump->MinimizedY = gumpMinimizedY;
                    gump->LockMoving = (gumpLockMoving != 0);
                    // nudge fully negative positions back into view; no upper
                    // bound - off-screen right/bottom is left where it was
                    if (gump->GetX() < 0)
                    {
                        gump->SetX(0);
                    }
                    if (gump->GetY() < 0)
                    {
                        gump->SetY(0);
                    }
                    if (gump->MinimizedX < 0)
                    {
                        gump->MinimizedX = 0;
                    }
                    if (gump->MinimizedY < 0)
                    {
                        gump->MinimizedY = 0;
                    }
                    AddGump(gump);
                    gump->WantUpdateContent = true;
                }
            }
            else if (key == "SpellGroup" && v.size() >= 2)
            {
                spellGroupMembers = str_to_int(v[1]);
                topSpell = nullptr;
            }
            else if (key == "Spell" && spellGroupMembers > 0 && v.size() >= 2)
            {
                spellGroupMembers--;
                const auto fields = TextFields(v);
                if (fields.size() < 10)
                {
                    continue;
                }

                const auto spellX = (uint16_t)str_to_int(fields[1]);
                const auto spellY = (uint16_t)str_to_int(fields[2]);
                const uint8_t gumpLockMoving = (uint8_t)str_to_int(fields[6]);
                const uint32_t serial = TextToUInt(fields[7]);
                const uint16_t graphic = (uint16_t)str_to_int(fields[8]);
                const SPELLBOOK_TYPE spellType = (SPELLBOOK_TYPE)str_to_int(fields[9]);

                CGumpSpell *spell = new CGumpSpell(serial, spellType, graphic, spellX, spellY);
                spell->LockMoving = (gumpLockMoving != 0);

                AddGump(spell);
                if (topSpell == nullptr)
                {
                    topSpell = spell;
                }
                else
                {
                    topSpell->AddSpell(spell);
                }

                spell->WantUpdateContent = true;
            }
            else if (key == "SkillGroup" && v.size() >= 2)
            {
                skillGroupMembers = str_to_int(v[1]);
                topSkill = nullptr;
            }
            else if (key == "Skill" && skillGroupMembers > 0 && v.size() >= 2)
            {
                // layout mirrors the Spell member line: x,y at 1/2, lock at 6,
                // serial at 7
                skillGroupMembers--;
                const auto fields = TextFields(v);
                if (fields.size() < 8)
                {
                    continue;
                }

                const auto skillX = (uint16_t)str_to_int(fields[1]);
                const auto skillY = (uint16_t)str_to_int(fields[2]);
                const uint8_t skillLockMoving = (uint8_t)str_to_int(fields[6]);
                const uint32_t serial = TextToUInt(fields[7]);

                CGumpSkill *skill = new CGumpSkill(serial, skillX, skillY);
                skill->LockMoving = (skillLockMoving != 0);

                AddGump(skill);
                if (topSkill == nullptr)
                {
                    topSkill = skill;
                }
                else
                {
                    topSkill->AddSkill(skill);
                }
            }
            else if (key == "Size" && v.size() >= 2)
            {
                auto size = TextSplit(v[1], ',');
                if (size.size() >= 2)
                {
                    ResourceTracker_SetStaticSize(str_to_int(size[0]), str_to_int(size[1]));
                }
            }
            else if (key == "TrackerItem" && v.size() >= 2)
            {
                auto f = TextSplit(v[1], ',');
                if (f.size() >= 2)
                {
                    ResourceTracker_AddStaticItem(
                        (uint16_t)str_to_int(f[0]), (uint16_t)str_to_int(f[1]));
                }
            }
        }

    }
    else
    {
        const bool menuBarEnabled = g_Config.ClientVersion >= CV_500A && !g_ConfigManager.DisableMenubar;
        if (menuBarEnabled && (g_ConfigManager.GameWindowX == 0) && (g_ConfigManager.GameWindowY == 0))
        {
            g_ConfigManager.GameWindowY = 40;
        }

        CSize windowSize = g_GameWindow.GetSize();

        int x = g_ConfigManager.GameWindowX + g_ConfigManager.GameWindowWidth;
        int y = g_ConfigManager.GameWindowY;

        if (x + 260 >= windowSize.Width)
        {
            x = windowSize.Width - 260;
        }

        if (y + 320 >= windowSize.Height)
        {
            y = windowSize.Height - 320;
        }

        AddGump(new CGumpPaperdoll(g_PlayerSerial, x, y, false));
        x = g_ConfigManager.GameWindowX + g_ConfigManager.GameWindowWidth;
        y = g_ConfigManager.GameWindowY + g_ConfigManager.GameWindowHeight - 50;

        if (x + 150 >= windowSize.Width)
        {
            x = windowSize.Width - 150;
        }

        if (y + 60 >= windowSize.Height)
        {
            y = windowSize.Height - 60;
        }

        AddGump(new CGumpStatusbar(g_PlayerSerial, x, y, false));
        AddGump(new CGumpMinimap(g_ConfigManager.GameWindowX, g_ConfigManager.GameWindowY, true));
        if (g_Player != nullptr)
        {
            CGameItem *backpack = g_Player->FindLayer(OL_BACKPACK);
            if (backpack != nullptr)
            {
                g_ContainerStack.push_back(CContainerStackItem(
                    backpack->Serial,
                    g_ConfigManager.GameWindowX,
                    g_ConfigManager.GameWindowY,
                    g_ConfigManager.GameWindowX,
                    g_ConfigManager.GameWindowY,
                    false,
                    false));

                g_UseItemActions.Add(backpack->Serial);
            }
        }
    }

    if (g_Config.ClientVersion >= CV_500A && !g_ConfigManager.DisableMenubar && !menubarFound)
    {
        CGumpMenubar *mbg = new CGumpMenubar(0, 0);
        mbg->SetOpened(true);
        AddGump(mbg);
    }

    if (!bufficonWindowFound)
    {
        CSize windowSize = g_GameWindow.GetSize();

        int x = g_ConfigManager.GameWindowX + (int)(g_ConfigManager.GameWindowWidth * 0.7f);
        int y = g_ConfigManager.GameWindowY + g_ConfigManager.GameWindowHeight;
        if (x + 100 >= windowSize.Width)
        {
            x = windowSize.Width - 100;
        }

        if (y + 60 >= windowSize.Height)
        {
            y = windowSize.Height - 60;
        }

        AddGump(new CGumpBuff(x, y));
    }

    if (!paperdollRequested)
    {
        g_Game.PaperdollReq(g_PlayerSerial);
    }

    AddGump(new CGumpConsoleType(minimizedConsoleType, showFullTextConsoleType));
}

static astr_t GumpBaseLine(GUMP_TYPE type, int x, int y, bool minimized, int minX, int minY, bool lockMoving)
{
    return "Gump=" + str_from((int)type) + "," + str_from(x) + "," + str_from(y) + "," +
           str_from((int)minimized) + "," + str_from(minX) + "," + str_from(minY) + "," +
           str_from((int)lockMoving);
}

void CGumpManager::Save(TextFileWriter &writer)
{
    writer.WriteRaw("[gumps]");

    std::vector<CGump *> containerList;
    std::vector<CGump *> spellInGroupList;
    std::vector<CGump *> skillInGroupList;
    for (CGump *gump = (CGump *)m_Items; gump != nullptr; gump = (CGump *)gump->m_Next)
    {
        // grouped skill gumps persist as SkillGroup blocks below, not as
        // individual generic lines
        if (gump->GumpType == GT_SKILL && ((CGumpSkill *)gump)->InGroup())
        {
            skillInGroupList.push_back(gump);
        }
        switch (gump->GumpType)
        {
            case GT_PAPERDOLL:
            case GT_STATUSBAR:
                if (gump->Serial != g_PlayerSerial)
                {
                    break;
                }
                // fall through
            case GT_MINIMAP:
            case GT_SKILLS:
            case GT_JOURNAL:
            case GT_RESOURCETRACKER:
            case GT_PROPERTY_ICON:
            case GT_MENUBAR:
            case GT_BUFF:
            case GT_ABILITY:
            case GT_COMBAT_BOOK:
            case GT_CONSOLE_TYPE:
            case GT_SKILL:
            case GT_RACIAL_ABILITIES_BOOK:
            case GT_RACIAL_ABILITY:
            {
                if (gump->GumpType == GT_SKILL && ((CGumpSkill *)gump)->InGroup())
                {
                    break; // already collected into skillInGroupList
                }
                // the menubar stores its Opened state (an unopened menubar is
                // a collapsed icon, not a hidden gump); everything else stores
                // Minimized
                const bool stateFlag = gump->GumpType == GT_MENUBAR
                    ? (bool)((CGumpMenubar *)gump)->GetOpened()
                    : gump->Minimized;

                astr_t line = GumpBaseLine(
                    gump->GumpType,
                    gump->GetX(),
                    gump->GetY(),
                    stateFlag,
                    gump->MinimizedX,
                    gump->MinimizedY,
                    gump->LockMoving);

                switch (gump->GumpType)
                {
                    case GT_JOURNAL:
                        line += "," + str_from(((CGumpJournal *)gump)->Height);
                        break;
                    case GT_SKILLS:
                        line += "," + str_from(((CGumpSkills *)gump)->Height);
                        break;
                    case GT_BUFF:
                        line += "," + str_from((int)gump->Graphic);
                        break;
                    case GT_ABILITY:
                    case GT_RACIAL_ABILITY:
                    {
                        char serial[16] = { 0 };
                        snprintf(serial, sizeof(serial), "%u", gump->Serial);
                        line += ",";
                        line += serial;
                        break;
                    }
                    case GT_CONSOLE_TYPE:
                        line += "," +
                                str_from((int)((CGumpConsoleType *)gump)->GetShowFullText());
                        break;
                    default:
                        break;
                }

                writer.WriteRaw(line);
                break;
            }
            case GT_CONTAINER:
            case GT_SPELLBOOK:
            case GT_SPELL:
            {
                if (gump->GumpType != GT_SPELL)
                {
                    CGameObject *topobj = g_World->FindWorldObject(gump->Serial);
                    if (topobj == nullptr || ((CGameItem *)topobj)->Layer == OL_BANK)
                    {
                        break;
                    }

                    topobj = topobj->GetTopObject();
                    if (topobj->Serial != g_PlayerSerial)
                    {
                        break;
                    }

                    containerList.push_back(gump);
                    break;
                }
                if (((CGumpSpell *)gump)->InGroup())
                {
                    spellInGroupList.push_back(gump);
                    break;
                }

                writer.WriteRaw(
                    GumpBaseLine(
                        gump->GumpType,
                        gump->GetX(),
                        gump->GetY(),
                        gump->Minimized,
                        gump->MinimizedX,
                        gump->MinimizedY,
                        gump->LockMoving) +
                    "," + std::to_string(gump->Serial) + "," + str_from((int)gump->Graphic) +
                    "," + str_from((int)((CGumpSpell *)gump)->SpellType));
                break;
            }
            default:
                break;
        }
    }

    std::vector<uint32_t> playerContainers;
    playerContainers.push_back(g_PlayerSerial);

    while (!playerContainers.empty() && !containerList.empty())
    {
        uint32_t containerSerial = playerContainers.front();
        playerContainers.erase(playerContainers.begin());
        for (auto it = containerList.begin(); it != containerList.end();)
        {
            CGump *gump = *it;
            if (gump->Serial == containerSerial)
            {
                writer.WriteRaw(
                    GumpBaseLine(
                        gump->GumpType,
                        gump->GetX(),
                        gump->GetY(),
                        gump->Minimized,
                        gump->MinimizedX,
                        gump->MinimizedY,
                        gump->LockMoving) +
                    "," + std::to_string(gump->Serial));
                it = containerList.erase(it);
                break;
            }
            ++it;
        }

        CGameObject *owner = g_World->FindWorldObject(containerSerial);
        if (owner != nullptr)
        {
            for (CGameItem *item = (CGameItem *)owner->m_Items; item != nullptr;
                 item = (CGameItem *)item->m_Next)
            {
                if (item->Opened)
                {
                    playerContainers.push_back(item->Serial);
                }
            }
        }
    }

    // spell groups: one "SpellGroup=<memberCount>" line followed by the
    // member "Spell=..." lines, in group order
    while (!spellInGroupList.empty())
    {
        CGumpSpell *topSpell = ((CGumpSpell *)spellInGroupList[0])->GetTopSpell();

        int spellsCount = 0;
        for (auto *spell = topSpell; spell != nullptr; spell = spell->m_GroupNext)
        {
            spellsCount++;
        }

        writer.WriteRaw("SpellGroup=" + str_from(spellsCount));
        for (auto *spell = topSpell; spell != nullptr; spell = spell->m_GroupNext)
        {
            // member lines carry an explicit "Spell" key: the loader only
            // regroups lines keyed "Spell" (plain "Gump=" lines load as
            // ungrouped icons). TextFields drops the key token, so the
            // payload repeats the gump type first - the reader reads x,y at
            // 1/2, lock at 6, serial/graphic/type at 7/8/9
            writer.WriteRaw(
                "Spell=" + str_from((int)GT_SPELL) + "," + str_from(spell->GetX()) + "," +
                str_from(spell->GetY()) + ",0,0,0," + str_from((int)spell->LockMoving) + "," +
                std::to_string(spell->Serial) + "," + str_from((int)spell->Graphic) + "," +
                str_from((int)spell->SpellType));

            for (auto it = spellInGroupList.begin(); it != spellInGroupList.end(); ++it)
            {
                if (*it == spell)
                {
                    spellInGroupList.erase(it);
                    break;
                }
            }
        }
    }

    // skill groups: one "SkillGroup=<memberCount>" line followed by the member
    // "Skill=..." lines, in group order (same field layout the loader reads)
    while (!skillInGroupList.empty())
    {
        CGumpSkill *topSkill = ((CGumpSkill *)skillInGroupList[0])->GetTopSkill();

        int skillsCount = 0;
        for (auto *skill = topSkill; skill != nullptr; skill = skill->m_GroupNext)
        {
            skillsCount++;
        }

        writer.WriteRaw("SkillGroup=" + str_from(skillsCount));
        for (auto *skill = topSkill; skill != nullptr; skill = skill->m_GroupNext)
        {
            writer.WriteRaw(
                "Skill=" + str_from((int)GT_SKILL) + "," + str_from(skill->GetX()) + "," +
                str_from(skill->GetY()) + ",0,0,0," + str_from((int)skill->LockMoving) + "," +
                std::to_string(skill->Serial));

            for (auto it = skillInGroupList.begin(); it != skillInGroupList.end(); ++it)
            {
                if (*it == skill)
                {
                    skillInGroupList.erase(it);
                    break;
                }
            }
        }
    }

    ResourceTracker_SaveStaticContents(writer);
}
