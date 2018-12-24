// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpPaperdoll.h"

enum
{
    ID_GP_NONE,

    ID_GP_BUTTON_HELP,
    ID_GP_BUTTON_OPTIONS,
    ID_GP_BUTTON_LOGOUT,
    ID_GP_BUTTON_JOURNAL_OR_QUESTS,
    ID_GP_BUTTON_SKILLS,
    ID_GP_BUTTON_CHAT_OR_GUILD,
    ID_GP_BUTTON_WARMODE,
    ID_GP_BUTTON_STATUS,
    ID_GP_BUTTON_MINIMIZE,
    ID_GP_PROFILE_SCROLL,
    ID_GP_PARTY_MANIFEST_SCROLL,
    ID_GP_BUTTON_VIRTURE,
    ID_GP_COMBAT_BOOK,
    ID_GP_RACIAL_ABILITIES_BOOK,
    ID_GP_LOCK_MOVING,

    ID_GP_COUNT,
};

int CGumpPaperdoll::UsedLayers[m_LayerCount] = {
    OL_CLOAK,    OL_RING,  OL_SHIRT,  OL_ARMS,  OL_PANTS,  OL_SHOES,    OL_LEGS,    OL_TORSO,
    OL_BRACELET, OL_15,    OL_GLOVES, OL_TUNIC, OL_SKIRT,  OL_NECKLACE, OL_HAIR,    OL_ROBE,
    OL_EARRINGS, OL_BEARD, OL_HELMET, OL_WAIST, OL_1_HAND, OL_2_HAND,   OL_TALISMAN
};

CGumpPaperdoll::CGumpPaperdoll(uint32_t serial, short x, short y, bool minimized)
    : CGump(GT_PAPERDOLL, serial, x, y)
{
    DEBUG_TRACE_FUNCTION;
    m_Locker.Serial = ID_GP_LOCK_MOVING;

    if (minimized)
    {
        Minimized = true;
        MinimizedX = x;
        MinimizedY = y;
        Page = 1;
    }
    else
    {
        Page = 2;
    }

    int partyManifestX = 37;
    int profileX = 23;
    static const int SCROLLS_STEP = 12;

    Add(new CGUIPage(1));
    Add(new CGUIGumppic(0x07EE, 0, 0));

    Add(new CGUIPage(2));

    if (Serial == g_PlayerSerial)
    {
        Add(new CGUIGumppic(0x07D0, 0, 0));

        Add(new CGUIButton(ID_GP_BUTTON_HELP, 0x07EF, 0x07F1, 0x07F0, 185, 44));
        Add(new CGUIButton(ID_GP_BUTTON_OPTIONS, 0x07D6, 0x07D8, 0x07D7, 185, 71));
        Add(new CGUIButton(ID_GP_BUTTON_LOGOUT, 0x07D9, 0x07DB, 0x07DA, 185, 98));

        if (g_PacketManager.GetClientVersion() >= CV_500A)
        {
            Add(new CGUIButton(ID_GP_BUTTON_JOURNAL_OR_QUESTS, 0x57B5, 0x57B6, 0x57B7, 185, 125));
        }
        else
        {
            Add(new CGUIButton(ID_GP_BUTTON_JOURNAL_OR_QUESTS, 0x07DC, 0x07DE, 0x07DD, 185, 125));
        }

        Add(new CGUIButton(ID_GP_BUTTON_SKILLS, 0x07DF, 0x07E1, 0x07E0, 185, 152));

        if (g_PacketManager.GetClientVersion() >= CV_500A)
        {
            Add(new CGUIButton(ID_GP_BUTTON_CHAT_OR_GUILD, 0x57B2, 0x57B3, 0x57B4, 185, 179));
        }
        else
        {
            if (!g_ChatEnabled)
            {
                Add(new CGUIShader(&g_DeathShader, true));
                Add(new CGUIButton(ID_GP_BUTTON_CHAT_OR_GUILD, 0x07E2, 0x07E2, 0x07E2, 185, 179));
                Add(new CGUIShader(&g_DeathShader, false));
            }
            else
            {
                Add(new CGUIButton(ID_GP_BUTTON_CHAT_OR_GUILD, 0x07E2, 0x07E4, 0x07E3, 185, 179));
            }
        }

        if (g_Player->Warmode)
        {
            m_ButtonWarmode = (CGUIButton *)Add(
                new CGUIButton(ID_GP_BUTTON_WARMODE, 0x07E8, 0x07EA, 0x07E9, 185, 206));
        }
        else
        {
            m_ButtonWarmode = (CGUIButton *)Add(
                new CGUIButton(ID_GP_BUTTON_WARMODE, 0x07E5, 0x07E7, 0x07E6, 185, 206));
        }

        //UO->DrawGump(0x0FA1, 0, posX + 80, posY + 4); //Paperdoll mail bag
        Add(new CGUIButton(ID_GP_BUTTON_VIRTURE, 0x0071, 0x0071, 0x0071, 80, 4));

        if (g_PaperdollBooks)
        {
            Add(new CGUIButton(ID_GP_COMBAT_BOOK, 0x2B34, 0x2B34, 0x2B34, 156, 200));

            if (g_PacketManager.GetClientVersion() >= CV_7000)
            {
                Add(new CGUIButton(ID_GP_RACIAL_ABILITIES_BOOK, 0x2B28, 0x2B28, 0x2B28, 23, 200));
                profileX += SCROLLS_STEP;
                partyManifestX += SCROLLS_STEP;
            }
        }

        Add(new CGUIButton(
            ID_GP_PARTY_MANIFEST_SCROLL, 0x07D2, 0x07D2, 0x07D2, partyManifestX, 196));

        CGUIHitBox *minimizeButton =
            (CGUIHitBox *)Add(new CGUIHitBox(ID_GP_BUTTON_MINIMIZE, 226, 258, 16, 16, true));
        minimizeButton->ToPage = 1;
    }
    else
    {
        m_ButtonWarmode = nullptr;
        Add(new CGUIGumppic(0x07D1, 0, 0));
    }

    Add(new CGUIButton(ID_GP_BUTTON_STATUS, 0x07EB, 0x07ED, 0x07EC, 185, 233));

    Add(new CGUIButton(ID_GP_PROFILE_SCROLL, 0x07D2, 0x07D2, 0x07D2, profileX, 196));

    //if (ConnectionManager.ClientVersion >= CV_60142)
    //	UO->DrawGump(0x2B34, 0, 156, 200);

    m_DataBox = (CGUIDataBox *)Add(new CGUIDataBox());
}

CGumpPaperdoll::~CGumpPaperdoll()
{
}

void CGumpPaperdoll::CalculateGumpState()
{
    DEBUG_TRACE_FUNCTION;
    CGump::CalculateGumpState();

    if (g_GumpPressed && g_PressedObject.LeftObject != nullptr &&
        g_PressedObject.LeftObject->IsText())
    {
        g_GumpMovingOffset.Reset();

        if (Minimized)
        {
            g_GumpTranslate.X = (float)MinimizedX;
            g_GumpTranslate.Y = (float)MinimizedY;
        }
        else
        {
            g_GumpTranslate.X = (float)m_X;
            g_GumpTranslate.Y = (float)m_Y;
        }
    }
}

void CGumpPaperdoll::InitToolTip()
{
    DEBUG_TRACE_FUNCTION;
    uint32_t id = g_SelectedObject.Serial;

    if (!Minimized)
    {
        switch (id)
        {
            case ID_GP_BUTTON_HELP:
            {
                g_ToolTip.Set(L"Open server's help menu gump");
                break;
            }
            case ID_GP_BUTTON_OPTIONS:
            {
                g_ToolTip.Set(L"Open the configuration gump");
                break;
            }
            case ID_GP_BUTTON_LOGOUT:
            {
                g_ToolTip.Set(L"Open the logout gump");
                break;
            }
            case ID_GP_BUTTON_JOURNAL_OR_QUESTS:
            {
                if (g_PacketManager.GetClientVersion() >= CV_500A)
                {
                    g_ToolTip.Set(L"Open the quests gump");
                }
                else
                {
                    g_ToolTip.Set(L"Open the journal gump");
                }

                break;
            }
            case ID_GP_BUTTON_SKILLS:
            {
                g_ToolTip.Set(L"Open the skills gump");
                break;
            }
            case ID_GP_BUTTON_CHAT_OR_GUILD:
            {
                if (g_PacketManager.GetClientVersion() >= CV_500A)
                {
                    g_ToolTip.Set(L"Open the guild gump");
                }
                else
                {
                    g_ToolTip.Set(L"Open the chat gump");
                }

                break;
            }
            case ID_GP_BUTTON_WARMODE:
            {
                g_ToolTip.Set(L"Change player's warmode");
                break;
            }
            case ID_GP_BUTTON_STATUS:
            {
                g_ToolTip.Set(L"Open player's statusbar gump");
                break;
            }
            case ID_GP_BUTTON_MINIMIZE:
            {
                g_ToolTip.Set(L"Minimize the paperdoll gump");
                break;
            }
            case ID_GP_PROFILE_SCROLL:
            {
                g_ToolTip.Set(L"Double click for open profile gump");
                break;
            }
            case ID_GP_PARTY_MANIFEST_SCROLL:
            {
                g_ToolTip.Set(L"Double click for open party manifest gump");
                break;
            }
            case ID_GP_BUTTON_VIRTURE:
            {
                g_ToolTip.Set(L"Open server's virture gump");
                break;
            }
            case ID_GP_COMBAT_BOOK:
            {
                g_ToolTip.Set(L"Open combat book");
                break;
            }
            case ID_GP_RACIAL_ABILITIES_BOOK:
            {
                g_ToolTip.Set(L"Open racial abilities book");
                break;
            }
            case ID_GP_LOCK_MOVING:
            {
                g_ToolTip.Set(L"Lock moving/closing the paperdoll gump");
                break;
            }
            default:
                break;
        }
    }
    else
    {
        g_ToolTip.Set(L"Double click to maximize paperdoll gump");
    }
}

void CGumpPaperdoll::DelayedClick(CRenderObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    if (obj != nullptr)
    {
        CTextData *td = new CTextData();
        td->Unicode = false;
        td->Font = 3;
        td->Serial = 0;
        td->Color = 0x038F;
        td->Timer = g_Ticks;
        td->Type = TT_CLIENT;
        td->SetX(g_MouseManager.Position.X - m_X);
        td->SetY(g_MouseManager.Position.Y - m_Y);

        string text = "Party Manifest";

        if (obj->Serial == ID_GP_PROFILE_SCROLL)
        {
            text = "Character Profile";
        }

        td->Text = text;

        int width = g_FontManager.GetWidthA(3, text);

        if (width > TEXT_MESSAGE_MAX_WIDTH)
        {
            td->GenerateTexture(TEXT_MESSAGE_MAX_WIDTH, 0, TS_CENTER);
        }
        else
        {
            td->GenerateTexture(0, 0, TS_CENTER);
        }

        m_TextContainer.Add(td);
        m_TextRenderer.AddText(td);

        g_Orion.AddJournalMessage(td, "");
    }
}

void CGumpPaperdoll::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    CGameCharacter *obj = g_World->FindWorldCharacter(Serial);

    if (obj == nullptr)
    {
        return;
    }

    if (!g_Player->Dead() && (CanLift || Serial == g_PlayerSerial) &&
        g_PressedObject.LeftGump == this && !g_ObjectInHand.Enabled &&
        g_PressedObject.LeftSerial != 0xFFFFFFFF &&
        g_MouseManager.LastLeftButtonClickTimer < g_Ticks)
    {
        Wisp::CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

        if (CanBeDraggedByOffset(offset) ||
            (g_MouseManager.LastLeftButtonClickTimer + g_MouseManager.DoubleClickDelay < g_Ticks))
        {
            int layer = g_PressedObject.LeftSerial - ID_GP_ITEMS;

            if (layer != OL_HAIR && layer != OL_BEARD && layer != OL_BACKPACK)
            {
                CGameItem *equipment = obj->FindLayer(layer);

                if (equipment != nullptr)
                {
                    //if (g_Target.IsTargeting())
                    //	g_Target.SendCancelTarget();

                    g_Orion.PickupItem(equipment);
                    //g_LastGumpLeftMouseDown = 0;
                    g_PressedObject.ClearLeft();
                    g_MouseManager.LeftDropPosition = g_MouseManager.Position;

                    if (layer == OL_1_HAND || layer == OL_2_HAND)
                    {
                        g_Player->UpdateAbilities();
                    }
                }
            }
        }
    }

    bool wantTransparent = false;

    if (g_SelectedObject.Gump == this && g_ObjectInHand.Enabled &&
        (g_ObjectInHand.TiledataPtr->AnimID != 0u))
    {
        if (obj->FindLayer(g_ObjectInHand.TiledataPtr->Layer) == nullptr)
        {
            if (!m_WantTransparentContent)
            {
                WantUpdateContent = true;
                m_WantTransparentContent = true;
            }

            wantTransparent = true;
        }
    }

    if (m_WantTransparentContent && !wantTransparent)
    {
        WantUpdateContent = true;
        m_WantTransparentContent = false;
    }

    if (!Minimized && m_TextRenderer.CalculatePositions(false))
    {
        WantRedraw = true;
    }
}

void CGumpPaperdoll::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    //Clear();

    CGameCharacter *obj = g_World->FindWorldCharacter(Serial);

    if (obj == nullptr)
    {
        return;
    }

    m_DataBox->Clear();

    m_Description = (CGUIText *)m_DataBox->Add(new CGUIText(0x0386, 39, 262));
    m_Description->CreateTextureA(1, obj->Title, 185);

    m_DataBox->Add(new CGUIShader(&g_ColorizerShader, true));

    CGUIGumppic *bodyGumppic = nullptr;

    uint16_t color = obj->Color & 0x3FFF;

    if (obj->Graphic == 0x0191 || obj->Graphic == 0x0193)
    {
        bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x000D, 8, 19)); //Female gump
        bodyGumppic->PartialHue = true;
        bodyGumppic->Color = color;
    }
    else if (obj->Graphic == 0x025D)
    {
        bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x000E, 8, 19)); //Elf Male gump
        bodyGumppic->PartialHue = true;
        bodyGumppic->Color = color;
    }
    else if (obj->Graphic == 0x025E)
    {
        bodyGumppic =
            (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x000F, 8, 19)); //Elf Female gump
        bodyGumppic->PartialHue = true;
        bodyGumppic->Color = color;
    }
    else if (obj->Graphic == 0x029A)
    {
        bodyGumppic =
            (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x29A, 8, 19)); //Gargoly Male gump
        bodyGumppic->PartialHue = true;
        bodyGumppic->Color = color;
    }
    else if (obj->Graphic == 0x029B)
    {
        bodyGumppic =
            (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x299, 8, 19)); //Gargoly Female gump
        bodyGumppic->PartialHue = true;
        bodyGumppic->Color = color;
    }
    else if (obj->Graphic == 0x03DB)
    {
        bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x000C, 8, 19)); //Male gump
        bodyGumppic->Color = 0x03EA;
        bodyGumppic->PartialHue = true;

        bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0xC72B, 8, 19)); //GM robe gump
        bodyGumppic->Color = color;
    }
    else //if (obj->Graphic == 0x0190 || obj->Graphic == 0x0192)
    {
        bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x000C, 8, 19)); //Male gump
        bodyGumppic->PartialHue = true;
        bodyGumppic->Color = color;
    }

    int gumpOffset = (obj->Female ? FEMALE_GUMP_OFFSET : MALE_GUMP_OFFSET);

    //Draw equipment & backpack
    CGameItem *equipment = nullptr;

    EQUIP_CONV_BODY_MAP &equipConv = g_AnimationManager.GetEquipConv();

    EQUIP_CONV_BODY_MAP::iterator bodyIter = equipConv.find(obj->Graphic);

    g_ColorizerShader.Use();

    //if (obj->IsHuman())
    {
        bool useSlots = g_ConfigManager.GetPaperdollSlots();

        static const int maxPaperdollSlots = 6;
        CGameItem *slotObjects[maxPaperdollSlots] = { 0 };

        for (int i = 0; i < m_LayerCount; i++)
        {
            equipment = obj->FindLayer(UsedLayers[i]);

            if (equipment != nullptr)
            {
                if (useSlots)
                {
                    switch (UsedLayers[i])
                    {
                        case OL_HELMET:
                        {
                            slotObjects[0] = equipment;
                            break;
                        }
                        case OL_EARRINGS:
                        {
                            slotObjects[1] = equipment;
                            break;
                        }
                        case OL_NECKLACE:
                        {
                            slotObjects[2] = equipment;
                            break;
                        }
                        case OL_RING:
                        {
                            slotObjects[3] = equipment;
                            break;
                        }
                        case OL_BRACELET:
                        {
                            slotObjects[4] = equipment;
                            break;
                        }
                        case OL_TUNIC:
                        {
                            slotObjects[5] = equipment;
                            break;
                        }
                        default:
                            break;
                    }
                }

                uint16_t id = equipment->AnimID;

                if (bodyIter != equipConv.end())
                {
                    EQUIP_CONV_DATA_MAP::iterator dataIter = bodyIter->second.find(id);

                    if (dataIter != bodyIter->second.end())
                    {
                        id = dataIter->second.Gump;
                    }
                }

                if (id != 0u)
                {
                    int cOfs = gumpOffset;

                    if (obj->Female && g_Orion.ExecuteGump(id + cOfs) == nullptr)
                    {
                        cOfs = MALE_GUMP_OFFSET;
                    }

                    bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(id + cOfs, 8, 19));
                    bodyGumppic->Color = equipment->Color & 0x3FFF;
                    bodyGumppic->PartialHue = equipment->IsPartialHue();
                    bodyGumppic->Serial = ID_GP_ITEMS + UsedLayers[i];
                }
            }
            else if (
                m_WantTransparentContent && g_ObjectInHand.Enabled &&
                UsedLayers[i] == g_ObjectInHand.TiledataPtr->Layer &&
                (g_ObjectInHand.TiledataPtr->AnimID != 0u))
            {
                equipment = obj->FindLayer(g_ObjectInHand.TiledataPtr->Layer);

                if (equipment == nullptr)
                {
                    uint16_t id = g_ObjectInHand.TiledataPtr->AnimID;

                    if (bodyIter != equipConv.end())
                    {
                        EQUIP_CONV_DATA_MAP::iterator dataIter = bodyIter->second.find(id);

                        if (dataIter != bodyIter->second.end())
                        {
                            id = dataIter->second.Gump;
                        }
                    }

                    int cOfs = gumpOffset;

                    if (obj->Female && (g_Orion.ExecuteGump(id + cOfs) == nullptr))
                    {
                        cOfs = MALE_GUMP_OFFSET;
                    }

                    m_DataBox->Add(new CGUIAlphaBlending(true, 0.7f));

                    bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(id + cOfs, 8, 19));
                    bodyGumppic->Color = g_ObjectInHand.Color & 0x3FFF;
                    bodyGumppic->PartialHue = ((g_ObjectInHand.Color & 0x8000) != 0);

                    m_DataBox->Add(new CGUIAlphaBlending(false, 0.0f));
                }
            }
        }

        if (useSlots)
        {
            int yPtr = 75;

            bool scaleImages = g_ConfigManager.GetScaleImagesInPaperdollSlots();

            for (int i = 0; i < maxPaperdollSlots; i++)
            {
                CGUIGumppicTiled *backgroundSlot = (CGUIGumppicTiled *)m_DataBox->Add(
                    new CGUIGumppicTiled(0x243A, 1, yPtr, 19, 20));
                CGUIGumppic *scopeSlot =
                    (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x2344, 1, yPtr));

                CGameItem *equipment = slotObjects[i];

                if (equipment != nullptr)
                {
                    uint32_t slotSerial = ID_GP_ITEMS + equipment->Layer;

                    CIndexObjectStatic &sio = g_Orion.m_StaticDataIndex[equipment->Graphic];
                    CGLTexture *texture = sio.Texture;

                    if (texture == nullptr)
                    {
                        texture = g_Orion.ExecuteStaticArt(equipment->Graphic);
                    }

                    if (texture == nullptr)
                    {
                        yPtr += 21;
                        continue;
                    }

                    int tileOffsetX = (13 - texture->ImageWidth) / 2;
                    int tileOffsetY = (14 - texture->ImageHeight) / 2;

                    int tileX = 4;
                    int tileY = 3 + yPtr;

                    if (scaleImages && (tileOffsetX < 0 || tileOffsetY < 0))
                    {
                        short imageWidth = 0;
                        short imageHeight = 0;
                        vector<uint16_t> pixels = g_UOFileReader.GetArtPixels(
                            equipment->Graphic, sio, true, imageWidth, imageHeight);

                        int wantImageWidth = texture->ImageWidth;
                        int wantImageHeight = texture->ImageHeight;

                        if (pixels.empty() || (wantImageWidth == 0) || (wantImageHeight == 0))
                        {
                            yPtr += 21;
                            continue;
                        }

                        vector<uint16_t> wantPixels(wantImageWidth * wantImageHeight, 0);

                        int imageOffsetX = texture->ImageOffsetX;
                        int imageOffsetY = texture->ImageOffsetY;

                        for (int y = 0; y < wantImageHeight; y++)
                        {
                            int srcPos = (y + imageOffsetY) * imageWidth + imageOffsetX;
                            int destPos = y * wantImageWidth;

                            for (int x = 0; x < wantImageWidth; x++)
                            {
                                wantPixels[destPos + x] = pixels[srcPos + x];
                            }
                        }

                        texture = new CGLTexture();
                        g_GL_BindTexture16(
                            *texture, wantImageWidth, wantImageHeight, &wantPixels[0]);

                        if (wantImageWidth < 14)
                        {
                            tileX += 7 - (wantImageWidth / 2);
                        }
                        else
                        {
                            tileX -= 2;

                            if (wantImageWidth > 18)
                            {
                                wantImageWidth = 18;
                            }
                        }

                        if (wantImageHeight < 14)
                        {
                            tileY += 7 - (wantImageHeight / 2);
                        }
                        else
                        {
                            tileY -= 2;

                            if (wantImageHeight > 18)
                            {
                                wantImageHeight = 18;
                            }
                        }

                        CGUIExternalTexture *ext =
                            (CGUIExternalTexture *)m_DataBox->Add(new CGUIExternalTexture(
                                texture, true, tileX, tileY, wantImageWidth, wantImageHeight));
                        ext->DrawOnly = true;
                        ext->Color = equipment->Color & 0x3FFF;
                    }
                    else
                    {
                        tileX -= texture->ImageOffsetX - tileOffsetX;
                        tileY -= texture->ImageOffsetY - tileOffsetY;
                        CGUITilepic *pic = new CGUITilepic(
                            equipment->Graphic, equipment->Color & 0x3FFF, tileX, tileY);
                        pic->PartialHue = equipment->IsPartialHue();
                        m_DataBox->Add(pic);
                        pic->Serial = slotSerial;
                    }

                    backgroundSlot->Serial = slotSerial;
                    scopeSlot->Serial = slotSerial;
                }

                yPtr += 21;
            }
        }
    }

    equipment = obj->FindLayer(OL_BACKPACK);

    if (equipment != nullptr && equipment->AnimID != 0)
    {
        uint16_t backpackGraphic = equipment->AnimID + 50000;

        if (obj->IsPlayer())
        {
            switch (g_ConfigManager.GetCharacterBackpackStyle())
            {
                case CBS_SUEDE:
                    backpackGraphic = 0x777B;
                    break;
                case CBS_POLAR_BEAR:
                    backpackGraphic = 0x777C;
                    break;
                case CBS_GHOUL_SKIN:
                    backpackGraphic = 0x777D;
                    break;
                default:
                    break;
            }

            if (g_Orion.ExecuteGump(backpackGraphic) == nullptr)
            {
                backpackGraphic = equipment->AnimID + 50000;
            }
        }

        int bpX = 8;

        if (g_PaperdollBooks)
        {
            bpX = 2;
        }

        bodyGumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(backpackGraphic, bpX, 19));
        bodyGumppic->Color = equipment->Color & 0x3FFF;
        bodyGumppic->PartialHue = equipment->IsPartialHue();
        bodyGumppic->Serial = ID_GP_ITEMS + OL_BACKPACK;
        bodyGumppic->MoveOnDrag = true;
    }

    m_DataBox->Add(new CGUIShader(&g_ColorizerShader, false));
}

void CGumpPaperdoll::UpdateDescription(const string &text)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Description != nullptr)
    {
        m_Description->CreateTextureA(1, text, 185);

        WantRedraw = true;
    }
}

void CGumpPaperdoll::Draw()
{
    DEBUG_TRACE_FUNCTION;
    CGameCharacter *obj = g_World->FindWorldCharacter(Serial);

    if (obj == nullptr)
    {
        return;
    }

    //if (g_LastObjectType == SOT_TEXT_OBJECT)
    //	g_GumpPressedElement = false;

    CGump::Draw();

    if (!Minimized)
    {
        glTranslatef(g_GumpTranslate.X, g_GumpTranslate.Y, 0.0f);

        g_FontColorizerShader.Use();

        m_TextRenderer.Draw();

        UnuseShader();

        glTranslatef(-g_GumpTranslate.X, -g_GumpTranslate.Y, 0.0f);
    }
}

CRenderObject *CGumpPaperdoll::Select()
{
    DEBUG_TRACE_FUNCTION;
    CGameCharacter *obj = g_World->FindWorldCharacter(Serial);

    if (obj == nullptr)
    {
        return nullptr;
    }

    CRenderObject *selected = CGump::Select();

    if (!Minimized)
    {
        Wisp::CPoint2Di oldPos = g_MouseManager.Position;
        g_MouseManager.Position =
            Wisp::CPoint2Di(oldPos.X - (int)g_GumpTranslate.X, oldPos.Y - (int)g_GumpTranslate.Y);

        m_TextRenderer.Select(this);

        g_MouseManager.Position = oldPos;
    }

    return selected;
}

void CGumpPaperdoll::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    switch (serial)
    {
        case ID_GP_BUTTON_HELP: //Paperdoll button Help
        {
            CPacketHelpRequest().Send();
            break;
        }
        case ID_GP_BUTTON_OPTIONS: //Paperdoll button Options
        {
            g_Orion.OpenConfiguration();
            break;
        }
        case ID_GP_BUTTON_LOGOUT: //Paperdoll button Log Out
        {
            g_Orion.OpenLogOut();
            break;
        }
        case ID_GP_BUTTON_JOURNAL_OR_QUESTS: //Paperdoll button Journal
        {
            if (g_PacketManager.GetClientVersion() >= CV_500A)
            {
                CPacketQuestMenuRequest().Send();
            }
            else
            {
                g_Orion.OpenJournal();
            }
            break;
        }
        case ID_GP_BUTTON_SKILLS: //Paperdoll button Skills
        {
            g_Orion.OpenSkills();
            break;
        }
        case ID_GP_BUTTON_CHAT_OR_GUILD: //Paperdoll button Chat
        {
            if (g_PacketManager.GetClientVersion() >= CV_500A)
            {
                CPacketGuildMenuRequest().Send();
            }
            else
            {
                g_Orion.OpenChat();
            }
            break;
        }
        case ID_GP_BUTTON_WARMODE: //Paperdoll button Peace/War
        {
            g_Orion.ChangeWarmode();
            break;
        }
        case ID_GP_BUTTON_STATUS: //Paperdoll button Status
        {
            g_Orion.OpenStatus(Serial);
            g_MouseManager.LastLeftButtonClickTimer = 0;
            break;
        }
        case ID_GP_BUTTON_MINIMIZE: //Paperdoll button Minimize
        {
            Minimized = true;
            break;
        }
        case ID_GP_LOCK_MOVING:
        {
            LockMoving = !LockMoving;
            g_MouseManager.CancelDoubleClick = true;
            break;
        }
        case ID_GP_PROFILE_SCROLL:
        case ID_GP_PARTY_MANIFEST_SCROLL:
        {
            if (!g_ClickObject.Enabled)
            {
                g_ClickObject.Init(g_PressedObject.LeftObject, this);
                g_ClickObject.Timer = g_Ticks + g_MouseManager.DoubleClickDelay;
            }

            break;
        }
        case ID_GP_COMBAT_BOOK:
        case ID_GP_RACIAL_ABILITIES_BOOK:
        {
            break;
        }
        default:
            break;
    }
}

void CGumpPaperdoll::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    CGump::OnLeftMouseButtonUp();

    uint32_t serial = g_SelectedObject.Serial;

    CGameCharacter *container = g_World->FindWorldCharacter(Serial);

    if (container == nullptr && serial >= ID_GP_ITEMS)
    {
        return;
    }

    //Что-то в руке
    if (((serial == 0u) || serial >= ID_GP_ITEMS) && g_ObjectInHand.Enabled)
    {
        int layer = serial - ID_GP_ITEMS;
        bool canWear = true;

        if (layer != OL_BACKPACK && Serial != g_PlayerSerial &&
            GetDistance(g_Player, container) >= DRAG_ITEMS_DISTANCE)
        {
            canWear = false;
        }

        if (canWear && container != nullptr)
        {
            if (layer == OL_BACKPACK) //Ткнули на пак
            {
                CGameItem *equipment = container->FindLayer(layer);

                if (equipment != nullptr)
                {
                    if (Serial != g_PlayerSerial)
                    {
                        g_Orion.DropItem(container->Serial, 0xFFFF, 0xFFFF, 0);
                    }
                    else
                    {
                        g_Orion.DropItem(equipment->Serial, 0xFFFF, 0xFFFF, 0);
                    }

                    g_MouseManager.LeftDropPosition = g_MouseManager.Position;
                    g_MouseManager.CancelDoubleClick = true;

                    FrameCreated = false;

                    return;
                }
            }
            else if (IsWearable(g_ObjectInHand.TiledataPtr->Flags)) //Можно одевать
            {
                CGameItem *equipment = container->FindLayer(g_ObjectInHand.TiledataPtr->Layer);

                if (equipment == nullptr) //На этом слое ничего нет
                {
                    if (Serial != g_PlayerSerial)
                    {
                        g_Orion.EquipItem(container->Serial);
                    }
                    else
                    {
                        g_Orion.EquipItem();
                    }

                    g_MouseManager.LeftDropPosition = g_MouseManager.Position;
                    g_MouseManager.CancelDoubleClick = true;

                    FrameCreated = false;

                    return;
                }
            }
        }
        else
        {
            g_Orion.PlaySoundEffect(0x0051);
        }
    }

    if (g_PressedObject.LeftSerial == serial && serial >= ID_GP_ITEMS && !g_ObjectInHand.Enabled)
    {
        int layer = serial - ID_GP_ITEMS;
        CGameItem *equipment = container->FindLayer(layer);

        if (equipment != nullptr)
        {
            if (g_Target.IsTargeting())
            {
                g_Target.SendTargetObject(equipment->Serial);
                g_MouseManager.CancelDoubleClick = true;
            }
            else //Click on object
            {
                if (!g_ClickObject.Enabled)
                {
                    g_ClickObject.Init(equipment);
                    g_ClickObject.Timer = g_Ticks + g_MouseManager.DoubleClickDelay;
                    g_ClickObject.X = g_MouseManager.Position.X - m_X;
                    g_ClickObject.Y = g_MouseManager.Position.Y - m_Y;
                }
            }
        }
    }
}

bool CGumpPaperdoll::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    if (Minimized)
    {
        Minimized = false;
        Page = 2;
        WantRedraw = true;

        return true;
    }

    bool result = false;
    uint32_t serial = g_PressedObject.LeftSerial;

    if (serial < ID_GP_ITEMS)
    {
        if (serial == ID_GP_PROFILE_SCROLL)
        {
            g_Orion.OpenProfile(Serial);

            result = true;
        }
        else if (serial == ID_GP_PARTY_MANIFEST_SCROLL)
        {
            g_Orion.OpenPartyManifest();

            result = true;
        }
        else if (serial == ID_GP_BUTTON_VIRTURE)
        {
            CPacketVirtureRequest(1).Send();

            result = true;
        }
        else if (serial == ID_GP_COMBAT_BOOK)
        {
            g_Orion.OpenCombatBookGump();

            result = true;
        }
        else if (serial == ID_GP_RACIAL_ABILITIES_BOOK)
        {
            g_Orion.OpenRacialAbilitiesBookGump();

            result = true;
        }
    }
    else
    {
        CGameCharacter *container = g_World->FindWorldCharacter(Serial);

        if (container != nullptr)
        {
            int layer = serial - ID_GP_ITEMS;

            CGameItem *equipment = container->FindLayer(layer);

            if (equipment != nullptr)
            {
                g_Orion.DoubleClick(equipment->Serial);

                result = true;
            }
        }
    }

    return result;
}
