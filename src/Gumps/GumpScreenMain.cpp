// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpScreenMain.h"
#include "GitRevision.h"
#include "../Config.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/MainScreen.h"

enum
{
    ID_MS_NONE,

    ID_MS_QUIT,
    ID_MS_ARROW_NEXT,
    ID_MS_ACCOUNT,
    ID_MS_PASSWORD,
    ID_MS_SAVEPASSWORD,
    ID_MS_AUTOLOGIN,
    ID_MS_UOACCOUNT,
    ID_MS_UOCOM,
    ID_MS_HELP,
    ID_MS_MOVIE,
    ID_MS_CREDITS,

    ID_MS_COUNT,
};

CGumpScreenMain::CGumpScreenMain()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;
}

CGumpScreenMain::~CGumpScreenMain()
{
}

void CGumpScreenMain::PrepareContent()
{
    static uint32_t lastArrowTick = 0;
    static bool arrowLighted = false;

    if (lastArrowTick < g_Ticks && m_Arrow != nullptr)
    {
        arrowLighted = !arrowLighted;
        if (g_Config.ClientVersion < CV_70611)
        {
            if (arrowLighted)
            {
                m_Arrow->Graphic = 0x15A5;
            }
            else
            {
                m_Arrow->Graphic = 0x15A4;
            }
        }
        else
        {
            if (arrowLighted)
            {
                m_Arrow->Graphic = 0x5CB;
            }
            else
            {
                m_Arrow->Graphic = 0x5CD;
            }
        }

        lastArrowTick = g_Ticks + 500;

        WantRedraw = true;
    }
}

void CGumpScreenMain::UpdateContent()
{
    if (m_Items != nullptr)
    {
        return;
    }

    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));

    Add(new CGUIGumppic(0x157C, 0, 0));

    if (g_Config.ClientVersion >= CV_500A && g_Config.ClientVersion < CV_70611)
    {
        Add(new CGUIGumppic(0x2329, 0, 0));
    }

    if (g_Config.ClientVersion < CV_500A)
    {
        Add(new CGUIGumppic(0x058A, 286, 45));
    }

    CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, 253, 305));

    if (g_Config.ClientVersion < CV_70611)
    {
        Add(new CGUIGumppic(0x15A0, 0, 4));
        Add(new CGUIResizepic(0, 0x13BE, 128, 288, 451, 157));

        //quit button
        Add(new CGUIButton(ID_MS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));

        text->CreateTextureA(2, "Log in to Ultima Online"); // cliloc 3000038

        text = (CGUIText *)Add(new CGUIText(0x0386, 183, 345));
        text->CreateTextureA(2, "Account Name"); // cliloc 3000039

        text = (CGUIText *)Add(new CGUIText(0x0386, 183, 385));
        text->CreateTextureA(2, "Password"); // cliloc 3000040

        Add(new CGUIResizepic(ID_MS_ACCOUNT, 0x0BB8, 328, 343, 210, 30));
        Add(new CGUIResizepic(ID_MS_PASSWORD, 0x0BB8, 328, 383, 210, 30));
        CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_ACCOUNT, 0x034F, 0x03E3, 0x0021, 335, 343, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;

        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_PASSWORD, 0x034F, 0x03E3, 0x0021, 335, 385, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;

        m_Arrow =
            (CGUIButton *)Add(new CGUIButton(ID_MS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x00D2, 0x00D3, 0x00D2, 328, 417));
        g_MainScreen.m_SavePassword->SetTextParameters(
            9, "Save Password", 0x0386, STP_RIGHT_CENTER);

        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 183, 417));
        g_MainScreen.m_AutoLogin->SetTextParameters(9, "Auto Login", 0x0386, STP_RIGHT_CENTER);

        text = (CGUIText *)Add(new CGUIText(0x034E, 286, 455));
        text->CreateTextureA(9, std::string("UO Version " + g_Config.ClientVersionString + "."));

        text = (CGUIText *)Add(new CGUIText(0x034E, 286, 467));
        text->CreateTextureA(9, std::string("CrossUO beta v") + RC_PRODUCE_VERSION_STR);
    }
    else
    {
        Add(new CGUIGumppic(0x014e, 0, 0));

        //Accout button
        Add(new CGUIButton(ID_MS_UOACCOUNT, 0x5C4, 0x5C2, 0x5C3, 10, 25));
        //UO.com button
        Add(new CGUIButton(ID_MS_UOCOM, 0x5C7, 0x5C5, 0x5C6, 10, 135));
        //Quit Button
        Add(new CGUIButton(ID_MS_QUIT, 0x5CA, 0x5C8, 0x5C9, 20, 240));

        //Help Button
        Add(new CGUIButton(ID_MS_HELP, 0x5D3, 0x5D1, 0x5D2, 530, 35));
        //Movie button
        Add(new CGUIButton(ID_MS_MOVIE, 0x5D6, 0x5D4, 0x5D5, 530, 80));
        //Credit button
        Add(new CGUIButton(ID_MS_CREDITS, 0x5D0, 0x5CE, 0x5CF, 530, 125));

        //TEEN RATING
        Add(new CGUIGumppic(0x2335, 30, 380));

        Add(new CGUIResizepic(ID_MS_ACCOUNT, 0x0BB8, 220, 280, 210, 30));
        Add(new CGUIResizepic(ID_MS_PASSWORD, 0x0BB8, 220, 335, 210, 30));
        CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_ACCOUNT, 0x034F, 0x03E3, 0x0021, 228, 283, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;

        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_PASSWORD, 0x034F, 0x03E3, 0x0021, 228, 338, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;

        m_Arrow =
            (CGUIButton *)Add(new CGUIButton(ID_MS_ARROW_NEXT, 0x5CD, 0x5CB, 0x5CC, 280, 365));
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x00D2, 0x00D3, 0x00D2, 328, 417));
        g_MainScreen.m_SavePassword->SetTextParameters(9, "Save Password", 0x802, STP_RIGHT_CENTER);

        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 183, 417));
        g_MainScreen.m_AutoLogin->SetTextParameters(9, "Auto Login", 0x802, STP_RIGHT_CENTER);

        text = (CGUIText *)Add(new CGUIText(0x802, 235, 440));
        text->CreateTextureA(9, std::string("UO Version " + g_Config.ClientVersionString + "."));
        text = (CGUIText *)Add(new CGUIText(0x7F2, 200, 460));
        text->CreateTextureA(9, std::string("CrossUO beta v") + RC_PRODUCE_VERSION_STR);
    }
}

void CGumpScreenMain::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips || g_SelectedObject.Object == nullptr)
    {
        return;
    }

    uint32_t id = g_SelectedObject.Serial;

    switch (id)
    {
        case ID_MS_QUIT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        case ID_MS_ARROW_NEXT:
        {
            g_ToolTip.Set(L"Next screen");
            break;
        }
        case ID_MS_ACCOUNT:
        {
            g_ToolTip.Set(L"Click here to enter your user name", 150);
            break;
        }
        case ID_MS_PASSWORD:
        {
            g_ToolTip.Set(L"Click here to enter your password", 150);
            break;
        }
        case ID_MS_SAVEPASSWORD:
        {
            g_ToolTip.Set(L"Save your password in config file", 150);
            break;
        }
        case ID_MS_AUTOLOGIN:
        {
            g_ToolTip.Set(L"Auto LogIn (without selection screens)", 150);
            break;
        }
        default:
            break;
    }
}

void CGumpScreenMain::GUMP_BUTTON_EVENT_C
{
    switch (serial)
    {
        case ID_MS_QUIT:
        {
            g_MainScreen.CreateSmoothAction(CMainScreen::ID_SMOOTH_MS_QUIT);
            break;
        }
        case ID_MS_ARROW_NEXT:
        {
            g_MainScreen.CreateSmoothAction(CMainScreen::ID_SMOOTH_MS_CONNECT);
            break;
        }
        case ID_MS_UOACCOUNT:
        {
            Platform::OpenBrowser("https://accounts.eamythic.com/");
            break;
        }
        case ID_MS_UOCOM:
        {
            Platform::OpenBrowser("https://uo.com");
            break;
        }
        case ID_MS_HELP:
        {
            Platform::OpenBrowser("https://crossuo.com/");
            break;
        }
        case ID_MS_MOVIE:
        {
            Platform::OpenBrowser("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
            break;
        }
        case ID_MS_CREDITS:
        {
            Platform::OpenBrowser("https://github.com/crossuo/crossuo/graphs/contributors");
            break;
        }
    }
}

void CGumpScreenMain::GUMP_TEXT_ENTRY_EVENT_C
{
    if (serial == ID_MS_PASSWORD)
    {
        g_MainScreen.m_Password->SetPos(m_PasswordFake->Pos());
    }
}
