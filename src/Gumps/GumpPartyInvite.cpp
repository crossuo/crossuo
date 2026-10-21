// SPDX-License-Identifier: MIT

#include "GumpPartyInvite.h"
#include "../GameWindow.h"
#include "../GameObjects/GameWorld.h"
#include "../Party.h"
#include "../Managers/GumpManager.h"
#include "../Network/Packets.h"
#include "../GUI/GUIAlphaBlending.h"
#include "../GUI/GUIHitBox.h"
#include "../GUI/GUIText.h"
#include "../Managers/FontsManager.h"
#include <cstdint>

CGumpPartyInvite::CGumpPartyInvite(uint32_t inviter)
    : CGump(GT_PARTY_INVITE, inviter, 0, 0)
{
    m_InviterNameSerial = inviter;

    CGameObject *mobile = g_World->FindWorldObject(inviter);
    astr_t name = (mobile != nullptr) ? mobile->GetName() : "";
    const int nameWidthAdjustment = (name.length() < 10) ? 0 : (int)name.length() * 5;

    const int x = g_GameWindow.GetSize().Width / 2 - 125;
    const int y = 150;

    Add(new CGUIAlphaBlending(true, 0.8f));
    Add(new CGUIColoredPolygone(0, 0, x, y, 270 + nameWidthAdjustment, 80, 0x80000000));

    astr_t text = name.empty() ? "No name" : name;
    astr_t line = text + " has invited you to party";
    CGUIText *label = (CGUIText *)Add(new CGUIText(0x0F, x + 10, y + 15));
    label->CreateTextureA(3, line, 250 + nameWidthAdjustment);

    // accept / decline text buttons at the bottom right
    const int buttonY = y + 55;
    const int acceptX = x + 99 + nameWidthAdjustment;
    const int declineX = x + 39 + nameWidthAdjustment;

    Add(new CGUIHitBox(ID_PI_ACCEPT, acceptX, buttonY, 45, 25));
    CGUIText *accept = (CGUIText *)Add(new CGUIText(0x0386, acceptX, buttonY + 5));
    accept->CreateTextureA(3, "Accept");
    accept->SetX(acceptX + (45 - g_FontManager.GetWidthA(3, "Accept")) / 2);

    Add(new CGUIHitBox(ID_PI_DECLINE, declineX, buttonY, 45, 25));
    CGUIText *decline = (CGUIText *)Add(new CGUIText(0x0386, declineX, buttonY + 5));
    decline->CreateTextureA(3, "Decline");
    decline->SetX(declineX + (45 - g_FontManager.GetWidthA(3, "Decline")) / 2);
}

void CGumpPartyInvite::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_PI_ACCEPT)
    {
        if (g_Party.Inviter != 0u && g_Party.Leader == 0u)
        {
            CPacketPartyAccept(g_Party.Inviter).Send();
            g_Party.Leader = g_Party.Inviter;
            g_Party.Inviter = 0;
        }
    }
    else if (serial == ID_PI_DECLINE)
    {
        if (g_Party.Inviter != 0u && g_Party.Leader == 0u)
        {
            CPacketPartyDecline(g_Party.Inviter).Send();
            g_Party.Inviter = 0;
        }
    }

    RemoveMark = true;
}
