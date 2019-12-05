// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpGeneric.h"
#include "../ToolTip.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../TextEngine/GameConsole.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ConfigManager.h"
#include "../Network/Packets.h"

CGumpGeneric::CGumpGeneric(uint32_t serial, short x, short y, uint32_t id)
    : CGump(GT_GENERIC, serial, x, y)
{
    Page = 1;
    ID = id;
}

CGumpGeneric::~CGumpGeneric()
{
}

void CGumpGeneric::InitToolTip()
{
    if (g_SelectedObject.Object != nullptr && g_SelectedObject.Object->IsGUI())
    {
        CBaseGUI *obj = (CBaseGUI *)g_SelectedObject.Object;

        if (obj->Type == GOT_VIRTUE_GUMP)
        {
            int offset = 0;

            switch (g_SelectedObject.Serial)
            {
                case 0x69:
                    offset = 2;
                    break;
                case 0x6A:
                    offset = 7;
                    break;
                case 0x6B:
                    offset = 5;
                    break;
                case 0x6C:
                    offset = 0;
                    break;
                case 0x6D:
                    offset = 6;
                    break;
                case 0x6E:
                    offset = 1;
                    break;
                case 0x6F:
                    offset = 3;
                    break;
                case 0x70:
                    offset = 4;
                    break;
                default:
                    break;
            }

            g_ToolTip.Set(1051000 + offset, "Some virtue gump item", 100);
        }
        else if (obj->ClilocID != 0u)
        {
            g_ToolTip.Set(obj->ClilocID, "");
        }
    }
}

void CGumpGeneric::AddText(
    int index, const std::wstring &text, CBaseGUI *start, bool backbroundCanBeColored)
{
    if (start == nullptr)
    {
        start = (CBaseGUI *)m_Items;
    }

    QFOR(item, start, CBaseGUI *)
    {
        switch (item->Type)
        {
            case GOT_TEXT:
            {
                CGUIGenericText *gt = (CGUIGenericText *)item;

                if (gt->TextID == index)
                {
                    gt->Create(text);
                }

                break;
            }
            case GOT_TEXTENTRY:
            {
                CGUIGenericTextEntry *gte = (CGUIGenericTextEntry *)item;

                if (gte->TextID == index)
                {
                    gte->m_Entry.SetTextW(text);
                }

                break;
            }
            case GOT_HTMLTEXT:
            {
                CGUIHTMLText *gt = (CGUIHTMLText *)item;

                if (gt->TextID == index)
                {
                    gt->Text = text;
                    gt->Create(backbroundCanBeColored);
                }

                break;
            }
            case GOT_HTMLGUMP:
            {
                CGUIHTMLGump *ghtml = (CGUIHTMLGump *)item;

                if (ghtml->Serial == index + 1)
                {
                    AddText(index, text, (CBaseGUI *)item->m_Items, !ghtml->HaveBackground);
                    ghtml->CalculateDataSize();
                }

                break;
            }
            default:
                break;
        }
    }
}

void CGumpGeneric::SendGumpResponse(int index)
{
    //Ответ на гамп
    CPacketGumpResponse(this, index).Send();

    //Удаляем использованный гамп
    RemoveMark = true;
}

void CGumpGeneric::GUMP_BUTTON_EVENT_C
{
    SendGumpResponse(serial);
}

void CGumpGeneric::GUMP_DIRECT_HTML_LINK_EVENT_C
{
    g_FontManager.GoToWebLink(link);
}

bool CGumpGeneric::OnLeftMouseButtonDoubleClick()
{
    if (g_GeneratedMouseDown)
    {
        return false;
    }

    if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI() &&
        ((CBaseGUI *)g_PressedObject.LeftObject)->Type == GOT_VIRTUE_GUMP)
    {
        //Ответ на гамп
        CPacketVirtueGumpResponse(this, g_PressedObject.LeftSerial).Send();

        //Удаляем использованный гамп
        RemoveMark = true;

        return true;
    }

    return false;
}

void CGumpGeneric::OnTextInput(const TextEvent &ev)
{
    const auto ch = EvChar(ev);
    g_EntryPointer->Insert(ch);
    WantRedraw = true;
}

void CGumpGeneric::OnKeyDown(const KeyEvent &ev)
{
    auto key = EvKey(ev);
    if (key == KEY_RETURN || key == KEY_RETURN2)
    {
        if (g_ConfigManager.GetConsoleNeedEnter())
        {
            g_EntryPointer = nullptr;
        }
        else
        {
            g_EntryPointer = &g_GameConsole;
        }

        WantRedraw = true;
    }
    else
    {
        g_EntryPointer->OnKey(this, key);
    }
}
