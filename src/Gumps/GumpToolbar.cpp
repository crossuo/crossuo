//
//  GumpToolbar.cpp
//  crossuo
//
//  Created by Jean-Martin Miljours on 19-12-21.
//

#include "GumpToolbar.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GamePlayer.h"
#include "../Target.h"
#include "../PressedObject.h"
#include "../CrossUO.h"

enum
{
    ID_GMB_NONE,
    
    ID_GTB_TARGET,
};


CGumpToolbar::CGumpToolbar(short x, short y)
: CGump(GT_GENERIC, 0, x, y)
{
    //Set All default item graphic
    Items[0] = 0x0E21;//bandage
    Items[1] = 0x0F7A;//black pearl
    Items[2] = 0x0F8C;//SULFOR ASH
    Items[3] = 0x0F85;//Ginsens
    Items[4] = 0x0F86;//mandrake
    Items[5] = 0x0F8D;//spider s'ilk
    Items[6] = 0x0F84;//garlic
    Items[7] = 0x0F88;//nightsade
    Items[8] = 0x0F7B;//bloodmosd
    Items[9] = 0x0F0B;//Heal potion
    Items[10] = 0x0F0C;//Refrech potion
    Items[11] = 0x0F09;//Mana potion
    
    //CGameObject *obj = g_World->FindWorldItem(g_LastTargetObject);
    DrawContent();
    
}

CGumpToolbar::~CGumpToolbar()
{
}

void CGumpToolbar::DrawContent()
{
    //Body
    Add(new CGUIResizepic(0, 0xA3C, 0, 0, Width, Height));
    Add(new CGUIChecktrans(5, 5,505,65));
    
    //Content
    int x_p = 7; //X axsis
    for (int i = 0; i < 12; i++)
    {
        Add(new CGUITilepic(Items[i],  0xFFFFFF, x_p, 15));
        CGUIText *txt = (CGUIText *) Add(new CGUIText(0x44, x_p + 15, 40));
        int count = CountItemBackPack(Items[i]);
        if (count <= 20 && count > 10)
        {
            txt->Color = 0x99;//yellow
        }
        else if (count <= 10)
        {
            txt->Color = 0x26;//red
        }
        else
        {
            txt->Color = 0x44;
        }
        txt->CreateTextureW(1,std::to_wstring(count));
        x_p += 42;
    }
}

void CGumpToolbar::PrepareContent()
{
    if (m_Count == 3)
    {
        WantUpdateContent = true;
        m_Count = 0;
    }
    if (m_Clear > 200)
    {
        Clear();
        m_Clear = 0;
    }
    static uint32_t ticks = 0;
    if (ticks < g_Ticks)
    {
        m_Clear += 1;
        m_Count += 1;
        ticks = g_Ticks + 100;
    }
}

void CGumpToolbar::UpdateContent()
{
    DrawContent();
}

void CGumpToolbar::GenerateFrame(bool stop)
{
    CGump::GenerateFrame(stop);
}

bool CGumpToolbar::OnLeftMouseButtonDoubleClick()
{
    bool result = false;
    
    if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI())
    {
        if (((CBaseGUI *)g_PressedObject.LeftObject)->Type == GOT_TILEPIC)
        {
            CGameItem *item = (CGameItem *) g_Player->FindLayer(OL_BACKPACK);
            CGameItem *find = nullptr;
            if (item != nullptr)
            {
                find = item->FindItem(g_PressedObject.LeftObject->Graphic);
            }
            if (find != nullptr)
            {
                g_Game.DoubleClick(find->Serial);
                FrameCreated = false;
                result = true;
            }
        }
    }
    
    return result;
}

void CGumpToolbar::GUMP_BUTTON_EVENT_C
{
    switch (serial)
    {
        case ID_GTB_TARGET:
        {
            //g_Target.GetTarget();
            break;
        }
    }
}

int CGumpToolbar::CountItemBackPack(uint32_t graph)
{
    int result = 0;
    CGameItem *bk = (CGameItem *) g_Player->FindLayer(OL_BACKPACK);
    if (bk != nullptr)
    {
        for (int i = 0; i < bk->GetItemsCount();i ++)
        {
            CGameItem *item = (CGameItem *)bk->Get(i);
            if (item->IsContainer())
            {
                for (int j = 0; j < item->GetItemsCount(); j++) {
                    CGameItem *subitem = (CGameItem *)item->Get(j);
                    if (subitem->Graphic == graph)
                    {
                        result += subitem->Count;
                    }
                }
            }
            else if (item->Graphic == graph)
            {
                result += item->Count;
            }
        }
    }
    return result;
}
