// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenSelectTown.h"
#include "Gump1998ScrollChoice.h"
#include "../Config.h"
#include "../CityList.h"
#include "../Misc.h" // CalculatePercents
#include "../ServerList.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/CreateCharacterManager.h"
#include "../GUI/GUIButton.h"
#include "../GUI/GUIShader.h"
#include "../Renderer/RenderShader.h" // g_ColorizerShader
#include "../Managers/FontsManager.h"
#include "../ScreenStages/SelectTownScreen.h"
#include "../Globals.h"     // g_Language, g_MapSize, UOFONT_*
#include <xuocore/uodata.h> // g_MapSize
#include <common/str.h>

enum
{
    ID_STS_NONE,

    ID_STS_QUIT,
    ID_STS_ARROW_PREV,
    ID_STS_ARROW_NEXT,
    ID_STS_HTML_GUMP,

    ID_STS_TOWN = 10,

    //1.25.32 (1998) CharCreation / CityChoice controls: disjoint id blocks
    //per role (panel buttons / hue sliders / one popup block per popup -
    //row ids are bounded by the row count in the handlers, never an
    //open-ended >=)
    ID_STS_1998_SELECT_SERVER = 300,
    ID_STS_1998_LOCATION,
    ID_STS_1998_BACK,
    ID_STS_1998_OK,

    ID_STS_1998_HAIR_POPUP_UP = 330,
    ID_STS_1998_HAIR_POPUP_DOWN,
    ID_STS_1998_HAIR_POPUP_THUMB,
    ID_STS_1998_BEARD_POPUP_UP = 335,
    ID_STS_1998_BEARD_POPUP_DOWN,
    ID_STS_1998_BEARD_POPUP_THUMB,
    ID_STS_1998_LOCATION_POPUP_UP = 340,
    ID_STS_1998_LOCATION_POPUP_DOWN,
    ID_STS_1998_LOCATION_POPUP_THUMB,
    ID_STS_1998_SERVER_POPUP_UP = 345,
    ID_STS_1998_SERVER_POPUP_DOWN,
    ID_STS_1998_SERVER_POPUP_THUMB,

    ID_STS_1998_HAIR_POPUP_ROW = 400,     //+0..+rowCount-1
    ID_STS_1998_BEARD_POPUP_ROW = 500,    //+0..+rowCount-1
    ID_STS_1998_LOCATION_POPUP_ROW = 600, //+0..+rowCount-1
    ID_STS_1998_SERVER_POPUP_ROW = 700,   //+0..+rowCount-1
};

CGumpScreenSelectTown::CGumpScreenSelectTown()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;

    //!Список точек для отображения кнопок городов
    m_TownButtonText.push_back(CPoint2Di(105, 130));
    m_TownButtonText.push_back(CPoint2Di(245, 90));
    m_TownButtonText.push_back(CPoint2Di(165, 200));
    m_TownButtonText.push_back(CPoint2Di(395, 160));
    m_TownButtonText.push_back(CPoint2Di(200, 305));
    m_TownButtonText.push_back(CPoint2Di(335, 250));
    m_TownButtonText.push_back(CPoint2Di(160, 395));
    m_TownButtonText.push_back(CPoint2Di(100, 250));
    m_TownButtonText.push_back(CPoint2Di(270, 130));
}

CGumpScreenSelectTown::~CGumpScreenSelectTown()
{
}

void CGumpScreenSelectTown::UpdateContent()
{
    Clear();

    CCityItem *city = g_SelectTownScreen.m_City;

    //1.25.32 (1998) wizard steps 3+4: CharCreation (page 1) and CityChoice
    //(page 2), plus the hair/beard/location/server scrollchoice popups on
    //pages 3-6
    if (g_Config.ClientVersion < CV_200)
    {
        UpdateContent1998();
        return;
    }

    if (city == nullptr)
    {
        return;
    }

    wstr_t description = city->m_City.Description;
    int map = 0;

    if (city->IsNewCity())
    {
        //!Получаем строку клилока с описанием города
        description = g_ClilocManager.Cliloc(g_Language)->GetW(((CCityItemNew *)city)->Cliloc);
        map = ((CCityItemNew *)city)->MapIndex;
    }

    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));
    Add(new CGUIGumppic(0x157C, 0, 0));
    Add(new CGUIGumppic(0x15A0, 0, 4));

    if (g_Config.ClientVersion >= CV_70130)
    {
        Add(new CGUIGumppic(0x15D9 + map, 62, 54));
        Add(new CGUIGumppic(0x15DF, 57, 49));

        static const wstr_t townNames[6] = { L"Felucca", L"Trammel", L"Ilshenar",
                                             L"Malas",   L"Tokuno",  L"Ter Mur" };

        CGUIText *mapName = (CGUIText *)Add(new CGUIText(0x0481, 240, 440));
        mapName->CreateTextureW(0, townNames[map], 24, 0, TS_LEFT, UOFONT_BLACK_BORDER);
    }
    else
    {
        Add(new CGUIGumppic(0x1598, 57, 49));
    }

    Add(new CGUIButton(ID_STS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));
    Add(new CGUIButton(ID_STS_ARROW_PREV, 0x15A1, 0x15A2, 0x15A3, 586, 445));
    Add(new CGUIButton(ID_STS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));

    m_HTMLGump = (CGUIHTMLGump *)Add(
        new CGUIHTMLGump(ID_STS_HTML_GUMP, 0x0BB8, 452, 60, 175, 367, true, true));

    //!Используем обработку HTML-тэгов при создании текстуры текста
    g_FontManager.SetUseHTML(true);

    m_Description = (CGUIText *)m_HTMLGump->Add(new CGUIText(0, 3, 3));
    m_Description->CreateTextureW(1, description, 30, 150);

    //!Выключаем обработку HTML-тэгов
    g_FontManager.SetUseHTML(false);

    m_HTMLGump->CalculateDataSize();

    for (int i = 0; i < (int)g_CityList.CityCount(); i++)
    {
        if (g_Config.ClientVersion >= CV_70130)
        {
            city = g_CityList.GetCity((uint32_t)i);
        }
        else
        {
            city = g_CityList.GetCity((uint32_t)i + 1);
        }

        if (city == nullptr)
        {
            continue;
        }

        int x = 0;
        int y = 0;

        if (city->IsNewCity())
        {
            CCityItemNew *newCity = (CCityItemNew *)city;

            uint32_t mapIdx = newCity->MapIndex;

            if (mapIdx > 5)
            {
                mapIdx = 5;
            }

            x = 62 + CalculatePercents(g_MapSize[mapIdx].Width - 2048, newCity->X, 383);
            y = 54 + CalculatePercents(g_MapSize[mapIdx].Height, newCity->Y, 384);
        }
        else if (i < (int)m_TownButtonText.size())
        {
            x = m_TownButtonText[i].X;
            y = m_TownButtonText[i].Y;
        }

        Add(new CGUIButton(ID_STS_TOWN + (int)i, 0x04B9, 0x04BA, 0x04BA, x, y));

        y -= 20;

        if (i == 3)
        { //Moonglow
            x -= 60;
        }

        CGUITextEntry *entry = (CGUITextEntry *)Add(
            new CGUITextEntry(ID_STS_TOWN + (int)i, 0x0058, 0x0099, 0x0481, x, y, 0, false, 3));
        entry->m_Entry.SetTextA(city->Name);
        entry->CheckOnSerial = true;
        entry->ReadOnly = true;
        entry->Focused = (g_SelectTownScreen.m_City->Name == city->Name);
    }
}

//1.25.32 (1998) creation wizard steps 3+4 on one stage: CharCreation (sex
//toggle, hair/beard pickers, hue sliders - page 1) then CityChoice (server,
//location, OK sends the create packet - page 2). The preview is composed
//per the original's 0x419CB0. Hue palette: skin 1001+, hair/beard
//1101..1148 via the coarse+fine slider pairs (0x4198A0).

//1.25.32 (1998) CityChoice: server + start-location selection; the OK
//button sends the create-character packet. The side scrolls are the
//collapsed lists: the left one is replaced by the location popup, the
//right one by the server popup

void CGumpScreenSelectTown::UpdateContent1998()
{
    //hardening: the scrollchoices cache child pointers that the Clear()
    //below frees - drop them before any conditional re-Create
    m_1998LocationPopup.Reset();
    m_1998ServerPopup.Reset();

    //the wood backdrop (640x480) stays behind every 1998 login-flow screen
    Add(new CGUIGumppic(0x03E8, 0, 0));

    //no text entries on this step; drop any earlier entry focus
    g_EntryPointer = nullptr;

    //CityChoice backdrop (388x247) at the fixed origin
    Add(new CGUIGumppic(0x0514, 119, 113));

    //side paper scrolls (CityChoice ctor 0x423830: left gumppic 0x4ED at
    //(85,121), right 0x4EE at (503,116) - the left one tucks under the
    //backdrop edge at x 119, so it must sit fully left of it). Collapsed
    //location/server lists: hidden while their popup is open
    if (m_1998OpenPopup != 1)
    {
        Add(new CGUIGumppic(0x04ED, 85, 121));
    }
    if (m_1998OpenPopup != 2)
    {
        Add(new CGUIGumppic(0x04EE, 503, 116));
    }

    //server label + SELECT. The original's server name carries a read-only
    //InputControl (0x41DE20) 180px wide at x 224 whose text is centred
    //((width-textWidth)/2 in 0x41E0F0); the SELECT button is DISABLED: the
    //CityChoice ctor stores +0xB0=1, which makes 0x413340 skip it and
    //0x413500 draw it hued 0x3C0 (dark grey)
    CServer *server = g_ServerList.GetSelectedServer();
    CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, 224, 173));
    text->CreateTextureA(1, server != nullptr ? server->Name : astr_t(""), 180, TS_CENTER);
    CGUIButton *serverButton =
        (CGUIButton *)Add(new CGUIButton(ID_STS_1998_SELECT_SERVER, 0x051B, 0x051C, 0x051C, 253, 134));
    serverButton->Enabled = false;
    serverButton->Color = 0x03C0;

    //location label + LOCATION picker (same 180px centred InputControl)
    text = (CGUIText *)Add(new CGUIText(0x0386, 224, 249));
    if (g_SelectTownScreen.m_City != nullptr)
    {
        text->CreateTextureA(1, g_SelectTownScreen.m_City->Name, 180, TS_CENTER);
    }
    Add(new CGUIButton(ID_STS_1998_LOCATION, 0x051E, 0x051F, 0x0520, 274, 208));

    //BACK -> the character customization step; OK -> send the
    //create-character packet
    Add(new CGUIButton(ID_STS_1998_BACK, 0x0515, 0x0516, 0x0517, 180, 281));
    Add(new CGUIButton(ID_STS_1998_OK, 0x0518, 0x0519, 0x051A, 391, 281));

    //the location/server scrollchoices draw over the CityChoice panel
    if (m_1998OpenPopup == 1)
    {
        //start locations: 1.25.32 reads regions.mul at startup (the 0xA9
        //list is not the source - see CCityList::LoadRegionsOrDefault).
        //Each row is the original's sprintf("%s: %s", city, building)
        //(0x460FD5, format at 0x4AD4C8)
        g_CityList.LoadRegionsOrDefault();
        std::vector<astr_t> rows;
        for (size_t i = 0; i < g_CityList.CityCount(); i++)
        {
            CCityItem *city = g_CityList.GetCity((int)i);
            if (city != nullptr)
            {
                rows.push_back(city->Name + ": " + city->Area);
            }
        }
        int selected = -1;
        for (int i = 0; i < (int)g_CityList.CityCount(); i++)
        {
            if (g_CityList.GetCity(i) == g_SelectTownScreen.m_City)
            {
                selected = i;
                break;
            }
        }
        m_1998LocationPopup.Create(this, ID_STS_1998_LOCATION_POPUP_ROW,
            ID_STS_1998_LOCATION_POPUP_UP, ID_STS_1998_LOCATION_POPUP_DOWN,
            ID_STS_1998_LOCATION_POPUP_THUMB, true, rows, 0, selected);
    }
    if (m_1998OpenPopup == 2)
    {
        std::vector<astr_t> rows;
        for (int i = 0; i < g_ServerList.ServersCount(); i++)
        {
            CServer *entry = g_ServerList.GetServer(i);
            if (entry != nullptr)
            {
                rows.push_back(entry->Name);
            }
        }
        int selected = -1;
        CServer *current = g_ServerList.GetSelectedServer();
        for (int i = 0; i < g_ServerList.ServersCount(); i++)
        {
            if (g_ServerList.GetServer(i) == current)
            {
                selected = i;
                break;
            }
        }
        m_1998ServerPopup.Create(this, ID_STS_1998_SERVER_POPUP_ROW,
            ID_STS_1998_SERVER_POPUP_UP, ID_STS_1998_SERVER_POPUP_DOWN,
            ID_STS_1998_SERVER_POPUP_THUMB, true, rows, 0, selected);
    }
}

void CGumpScreenSelectTown::GUMP_BUTTON_EVENT_C
{
    //1.25.32 CityChoice controls
    if (g_Config.ClientVersion < CV_200)
    {
        if (serial == ID_STS_1998_SELECT_SERVER)
        {
            //the picker toggles the server list; it replaces the RIGHT
            //scroll art
            m_1998OpenPopup = m_1998OpenPopup == 2 ? 0 : 2;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (serial == ID_STS_1998_LOCATION)
        {
            //the picker toggles the location list; it replaces the LEFT
            //scroll art
            m_1998OpenPopup = m_1998OpenPopup == 1 ? 0 : 1;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (m_1998LocationPopup.IsRow(serial))
        {
            //start location row (rows are indexed from 0, matching the
            //regions.mul table the popup was built from)
            CCityItem *city = g_CityList.GetCity(serial - ID_STS_1998_LOCATION_POPUP_ROW);
            if (city != nullptr)
            {
                g_SelectTownScreen.m_City = city;
            }

            //a pick closes the list
            m_1998OpenPopup = 0;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (m_1998ServerPopup.IsRow(serial))
        {
            //server row: updates the local selection shown on CityChoice.
            //The original re-sent the server-select packet from here (which
            //relays to another game server); crossuo keeps the relay made
            //at the server screen, so this only updates the label/packet
            //server index.
            g_ServerList.Select(serial - ID_STS_1998_SERVER_POPUP_ROW);

            m_1998OpenPopup = 0;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (
            serial == ID_STS_1998_LOCATION_POPUP_UP || serial == ID_STS_1998_LOCATION_POPUP_DOWN)
        {
            m_1998LocationPopup.SetTop(
                m_1998LocationPopup.Top() +
                (serial == ID_STS_1998_LOCATION_POPUP_UP ? -1 : 1));
            WantRedraw = true;
        }
        else if (
            serial == ID_STS_1998_SERVER_POPUP_UP || serial == ID_STS_1998_SERVER_POPUP_DOWN)
        {
            m_1998ServerPopup.SetTop(
                m_1998ServerPopup.Top() + (serial == ID_STS_1998_SERVER_POPUP_UP ? -1 : 1));
            WantRedraw = true;
        }
        else if (serial == ID_STS_1998_BACK)
        {
            //BACK -> the character customization step
            g_SelectTownScreen.CreateSmoothAction(
                CSelectTownScreen::ID_SMOOTH_STS_GO_SCREEN_CREATE);
        }
        else if (serial == ID_STS_1998_OK)
        {
            //send the 0x00 create-character packet (the stage action builds
            //it from the wizard state)
            g_SelectTownScreen.CreateSmoothAction(
                CSelectTownScreen::ID_SMOOTH_STS_GO_SCREEN_GAME_CONNECT);
        }

        return;
    }

    if (serial == ID_STS_QUIT)
    { //x button
        g_SelectTownScreen.CreateSmoothAction(CSelectTownScreen::ID_SMOOTH_STS_QUIT);
    }
    else if (serial == ID_STS_ARROW_PREV)
    { //< button
        g_SelectTownScreen.CreateSmoothAction(CSelectTownScreen::ID_SMOOTH_STS_GO_SCREEN_CHARACTER);
    }
    else if (serial == ID_STS_ARROW_NEXT || serial >= ID_STS_TOWN)
    { //> button
        g_SelectTownScreen.CreateSmoothAction(
            CSelectTownScreen::ID_SMOOTH_STS_GO_SCREEN_GAME_CONNECT);
    }
}

void CGumpScreenSelectTown::GUMP_SLIDER_CLICK_EVENT_C
{
    OnSliderMove(serial);
}

void CGumpScreenSelectTown::GUMP_SLIDER_MOVE_EVENT_C
{
    //1.25.32 CityChoice popup thumbs: read the slider value back into the
    //matching row window (the row textures refresh in place)
    if (g_Config.ClientVersion < CV_200)
    {
        if (m_1998LocationPopup.IsThumb(serial))
        {
            m_1998LocationPopup.SyncThumb();
            WantRedraw = true;
            return;
        }
        if (m_1998ServerPopup.IsThumb(serial))
        {
            m_1998ServerPopup.SyncThumb();
            WantRedraw = true;
            return;
        }
    }
}

void CGumpScreenSelectTown::GUMP_TEXT_ENTRY_EVENT_C
{
    if (g_Config.ClientVersion < CV_200)
    {
        return; //the CityChoice step has no text entries
    }

    // the modern layout's town rows are read-only text entries: the click
    // selects the start city and refreshes the description panel
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_TEXTENTRY)
        {
            if (item->Serial == serial)
            {
                CCityItem *city = g_CityList.GetCity(serial - ID_STS_TOWN + 1);

                if (city == nullptr || g_SelectTownScreen.m_City->Name == city->Name)
                {
                    break;
                }

                g_SelectTownScreen.m_City = city;

                auto description = city->m_City.Description;

                if (city->IsNewCity())
                {
                    description =
                        g_ClilocManager.Cliloc(g_Language)->GetW(((CCityItemNew *)city)->Cliloc);
                }

                g_FontManager.SetUseHTML(true);

                if (m_Description != nullptr)
                {
                    m_Description->CreateTextureW(1, description, 30, 150);
                }

                g_FontManager.SetUseHTML(false);

                if (m_HTMLGump != nullptr)
                {
                    m_HTMLGump->ResetDataOffset();
                    m_HTMLGump->CalculateDataSize();
                }

                ((CGUITextEntry *)item)->Focused = true;
            }
            else
            {
                ((CGUITextEntry *)item)->Focused = false;
            }
        }
    }
}
