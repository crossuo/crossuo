// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpBulletinBoardItem.h"
#include "../Config.h"

enum
{
    ID_GBBI_NONE,

    ID_GBBI_SUBJECT_TEXT_FIELD,
    ID_GBBI_TEXT_FIELD,
    ID_GBBI_POST,
    ID_GBBI_REPLY,
    ID_GBBI_REMOVE,

    ID_GBBI_COUNT,
};

CGumpBulletinBoardItem::CGumpBulletinBoardItem(
    int serial,
    int x,
    int y,
    uint8_t variant,
    int id,
    const wstring &poster,
    const wstring &subject,
    const wstring &dataTime,
    const wstring &data)
    : CGumpBaseScroll(GT_BULLETIN_BOARD_ITEM, serial, 0x0820, 250, x, y, false, 70)
    , m_Variant(variant)
{
    DEBUG_TRACE_FUNCTION;
    ID = id;
    m_MinHeight = 200;

    bool useUnicode = (g_Config.ClientVersion >= CV_305D);
    int unicodeFontIndex = 1;
    int unicodeHeightOffset = 0;
    uint16_t textColor = 0x0386;

    if (useUnicode)
    {
        unicodeHeightOffset = -6;
        textColor = 0;
    }

    CGUIText *text = (CGUIText *)Add(new CGUIText(0, 30, 40));

    if (useUnicode)
    {
        text->CreateTextureW(unicodeFontIndex, L"Author:");
    }
    else
    {
        text->CreateTextureA(6, "Author:");
    }

    CGUIText *text2 = (CGUIText *)Add(
        new CGUIText(textColor, 30 + text->m_Texture.Width, 46 + unicodeHeightOffset));

    if (useUnicode)
    {
        text2->CreateTextureW(unicodeFontIndex, poster);
    }
    else
    {
        text2->CreateTextureA(9, ToString(poster));
    }

    text = (CGUIText *)Add(new CGUIText(0, 30, 56));

    if (useUnicode)
    {
        text->CreateTextureW(unicodeFontIndex, L"Time:");
    }
    else
    {
        text->CreateTextureA(6, "Time:");
    }

    text2 = (CGUIText *)Add(
        new CGUIText(textColor, 30 + text->m_Texture.Width, 62 + unicodeHeightOffset));

    if (useUnicode)
    {
        text2->CreateTextureW(unicodeFontIndex, dataTime);
    }
    else
    {
        text2->CreateTextureA(9, ToString(dataTime));
    }

    text = (CGUIText *)Add(new CGUIText(0, 30, 72));

    if (useUnicode)
    {
        text->CreateTextureW(unicodeFontIndex, L"Subject:");
    }
    else
    {
        text->CreateTextureA(6, "Subject:");
    }

    uint16_t subjectColor = textColor;

    if (m_Variant == 0u)
    {
        subjectColor = 0x0008;
    }

    Add(new CGUIHitBox(ID_GBBI_SUBJECT_TEXT_FIELD, 30 + text->m_Texture.Width, 78, 160, 16));

    m_EntrySubject = (CGUITextEntry *)Add(new CGUITextEntry(
        ID_GBBI_SUBJECT_TEXT_FIELD,
        subjectColor,
        subjectColor,
        subjectColor,
        30 + text->m_Texture.Width,
        78 + unicodeHeightOffset,
        150,
        useUnicode,
        (useUnicode ? unicodeFontIndex : 9)));
    m_EntrySubject->m_Entry.SetTextW(subject);

    if (m_Variant == 0u)
    {
        CGump *gumpEntry = g_GumpManager.GetTextEntryOwner();

        if (gumpEntry != nullptr)
        {
            gumpEntry->WantRedraw = true;
        }

        g_EntryPointer = &m_EntrySubject->m_Entry;
    }

    Add(new CGUIGumppicTiled(0x0835, 30, 100, 204, 0));

    m_Entry = (CGUITextEntry *)m_HTMLGump->Add(new CGUITextEntry(
        ID_GBBI_TEXT_FIELD,
        textColor,
        textColor,
        textColor,
        3,
        3,
        220,
        useUnicode,
        (useUnicode ? unicodeFontIndex : 9)));
    m_Entry->m_Entry.MaxWidth = 0;
    m_Entry->m_Entry.SetTextW(data);
    m_Entry->m_Entry.CreateTextureA(9, m_Entry->m_Entry.c_str(), textColor, 220, TS_LEFT, 0);
    m_HitBox = (CGUIHitBox *)m_HTMLGump->Add(
        new CGUIHitBox(ID_GBBI_TEXT_FIELD, 3, 3, 220, m_Entry->m_Entry.m_Texture.Height));

    if (m_HitBox->Height < 14)
    {
        m_HitBox->Height = 14;
    }

    m_HTMLGump->CalculateDataSize();

    m_ButtonPost = nullptr;
    m_ButtonRemove = nullptr;
    m_ButtonReply = nullptr;

    switch (m_Variant)
    {
        case 0:
        {
            Add(new CGUIGumppic(0x0883, 97, 12)); //NEW MESSAGE
            m_ButtonPost = (CGUIButton *)Add(
                new CGUIButton(ID_GBBI_POST, 0x0886, 0x0886, 0x0886, 37, Height - 22)); //Post
            m_ButtonPost->CheckPolygone = true;

            break;
        }
        case 2:
            m_ButtonRemove = (CGUIButton *)Add(
                new CGUIButton(ID_GBBI_REMOVE, 0x0885, 0x0885, 0x0885, 235, Height - 22)); //Remove
            m_ButtonRemove->CheckPolygone = true;
        case 1:
        {
            m_ButtonReply = (CGUIButton *)Add(
                new CGUIButton(ID_GBBI_REPLY, 0x0884, 0x0884, 0x0884, 37, Height - 22)); //Reply
            m_ButtonReply->CheckPolygone = true;

            m_EntrySubject->ReadOnly = true;
            m_Entry->ReadOnly = true;

            break;
        }
        default:
            break;
    }
}

CGumpBulletinBoardItem::~CGumpBulletinBoardItem()
{
}

void CGumpBulletinBoardItem::UpdateHeight()
{
    DEBUG_TRACE_FUNCTION;
    CGumpBaseScroll::UpdateHeight();

    if (m_ButtonPost != nullptr)
    {
        m_ButtonPost->SetY(Height - 22); //Post
    }

    if (m_ButtonRemove != nullptr)
    {
        m_ButtonRemove->SetY(Height - 22); //Remove
    }

    if (m_ButtonReply != nullptr)
    {
        m_ButtonReply->SetY(Height - 22); //Reply
    }
}

void CGumpBulletinBoardItem::RecalculateHeight()
{
    DEBUG_TRACE_FUNCTION;
    if (g_EntryPointer == &m_Entry->m_Entry)
    {
        m_Entry->m_Entry.CreateTextureA(9, m_Entry->m_Entry.c_str(), 0x0386, 220, TS_LEFT, 0);
        m_HitBox->Height = m_Entry->m_Entry.m_Texture.Height;

        if (m_HitBox->Height < 14)
        {
            m_HitBox->Height = 14;
        }

        m_HTMLGump->CalculateDataSize();
    }
}

void CGumpBulletinBoardItem::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (m_EntrySubject != nullptr)
    {
        if (serial == ID_GBBI_POST)
        {
            CPacketBulletinBoardPostMessage(
                ID, 0, m_EntrySubject->m_Entry.c_str(), m_Entry->m_Entry.c_str())
                .Send();

            RemoveMark = true;
        }
        else if (serial == ID_GBBI_REPLY)
        {
            wstring subj(L"RE: ");
            subj += m_EntrySubject->m_Entry.Data();

            CGumpBulletinBoardItem *gump = new CGumpBulletinBoardItem(
                0, 0, 0, 0, ID, ToWString(g_Player->GetName()), subj, L"Date/Time", {});

            g_GumpManager.AddGump(gump);
        }
        else if (serial == ID_GBBI_REMOVE)
        {
            CPacketBulletinBoardRemoveMessage(ID, Serial).Send();

            //GumpManager->CloseGump(Serial, ID, GT_BULLETIN_BOARD_ITEM);
        }
    }
}

void CGumpBulletinBoardItem::OnTextInput(const TextEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    g_EntryPointer->Insert(EvChar(ev));
    RecalculateHeight();
    WantRedraw = true;
}

void CGumpBulletinBoardItem::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;
    auto key = EvKey(ev);
    if ((key == KEY_RETURN || key == KEY_RETURN2) && m_Entry != nullptr &&
        g_EntryPointer == &m_Entry->m_Entry)
    {
        g_EntryPointer->Insert(L'\n');
        RecalculateHeight();
        WantRedraw = true;
    }
    else
    {
        g_EntryPointer->OnKey(this, key);
        if (WantRedraw)
        {
            RecalculateHeight();
        }
    }
}
