// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenMain.h"
#include "revision.h"
#include "../Config.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/MainScreen.h"
#include "../GUI/GUIColoredPolygone.h"
#include "../LauncherWindow.h"

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
    ID_MS_LAUNCHER,

    ID_MS_COUNT,

    //1998 (1.25.32) title screen buttons; MENU1/2/3/5 are the original
    //client's sub-screen buttons (credits, movie, etc.) kept for looks only
    ID_1998_LOGIN,
    ID_1998_LOGON,
    ID_1998_BACK,
    ID_1998_MENU1,
    ID_1998_MENU2,
    ID_1998_MENU3,
    ID_1998_MENU5,
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

    //1.25.32 login: mirror the original's entry art state machine - the
    //filler-text box shows while the field is unfocused and empty, the
    //plain box once it holds text, and the bright variant while focused
    if (g_Config.ClientVersion < CV_200 && m_1998AccountArt != nullptr)
    {
        const bool accountFocused = g_EntryPointer == &m_1998AccountEntry->m_Entry;
        int accountArt = 0x411;
        if (accountFocused)
        {
            accountArt = 0x420;
        }
        else if (m_1998AccountEntry->m_Entry.GetTextA().length() != 0)
        {
            accountArt = 0x412;
        }
        if (m_1998AccountArt->Graphic != accountArt)
        {
            m_1998AccountArt->Graphic = accountArt;
            WantRedraw = true;
        }

        const bool passwordFocused = g_EntryPointer == m_PasswordFake;
        int passwordArt = 0x413;
        if (passwordFocused)
        {
            passwordArt = 0x422;
        }
        else if (m_PasswordFake->GetTextA().length() != 0)
        {
            passwordArt = 0x414;
        }
        if (m_1998PasswordArt->Graphic != passwordArt)
        {
            m_1998PasswordArt->Graphic = passwordArt;
            WantRedraw = true;
        }
    }

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

    // The 1.25.32 (1998) login screens paint their own backdrop art over a
    // cleared framebuffer; the modern tiled background is not part of them.
    if (g_Config.ClientVersion >= CV_200)
    {
        Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));

        Add(new CGUIGumppic(0x157C, 0, 0));
    }

    if (DebugRenderScreen())
    {
        // draw-order regression aids: a colored polygone (like the minimap
        // notoriety dots) followed by a textured item, everything drawn after
        // the polygone must not inherit its draw state
        Add(new CGUIColoredPolygone(0, 0, 0, 0, 40, 40, 0xFF0000FF));
        Add(new CGUIGumppic(0x157C, 320, 0));
        return;
    }

    // The 1.25.32 (1998) client's login screens, reproduced with the gump art
    // ids and pixel positions of the original client's MainMenu/AcctLogin.
    // Both screens live in this gump: the title backdrop below is added
    // before any page so it stays visible behind the account panel (the
    // original client did not clear the framebuffer between the two screens);
    // the pages are swapped from GUMP_BUTTON_EVENT_C.
    if (g_Config.ClientVersion < CV_200)
    {
        //fullscreen backdrop (640x480): the inside-of-the-box artwork the
        //original blits into its framebuffer before the title screen; gump
        //0x587 is the post-intro variant, 0x3E8 the intro-skipped one. The
        //framebuffer is never cleared afterwards, so this stays behind both
        //screens
        Add(new CGUIGumppic(0x3E8, 0, 0));

        Add(new CGUIPage(1)); //page 1: the MainMenu (title) plate + buttons

        //MainMenu (title) backdrop (422x240) - page 1 only: the original
        //hides the title gump when the account screen opens, leaving just
        //the baked backdrop behind the login panel
        Add(new CGUIGumppic(0x3F2, 106, 117));

        // The six original title buttons (art up/down/pressed).
        Add(new CGUIButton(ID_1998_MENU1, 0x3F6, 0x3F7, 0x3F8, 145, 199));
        Add(new CGUIButton(ID_1998_MENU2, 0x3FC, 0x3FD, 0x3FE, 257, 130));
        Add(new CGUIButton(ID_1998_MENU3, 0x3FF, 0x400, 0x401, 265, 155));
        Add(new CGUIButton(ID_1998_LOGIN, 0x3F3, 0x3F4, 0x3F5, 247, 187));
        Add(new CGUIButton(ID_1998_MENU5, 0x3F9, 0x3FA, 0x3FB, 415, 199));
        Add(new CGUIButton(ID_MS_QUIT, 0x402, 0x403, 0x404, 281, 284));

        Add(new CGUIPage(2)); //page 2: the AcctLogin (account login) panel

        //login panel (371x256)
        Add(new CGUIGumppic(0x410, 128, 108));

        //account name entry: the three 190x21 box variants are complete
        //pieces with an identical shape - 0x411 carries the "USER I.D."
        //filler text (unfocused + empty), 0x412 is the empty unfocused one
        //and 0x420 the focused one; PrepareContent swaps them by state
        m_1998AccountArt = (CGUIGumppic *)Add(new CGUIGumppic(0x411, 219, 138));
        CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_ACCOUNT, 0x0386, 0x0386, 0x0386, 224, 139, 190, false, 2, TS_LEFT, 0, 30));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;
        m_1998AccountEntry = entry;

        //password entry: the entry itself is not masked, the fake entry keeps
        //the asterisks while the real text goes to g_MainScreen.m_Password;
        //0x413 carries the "PASSWORD" filler, 0x414 empty unfocused, 0x422 focused
        m_1998PasswordArt = (CGUIGumppic *)Add(new CGUIGumppic(0x413, 217, 173));
        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_PASSWORD, 0x0386, 0x0386, 0x0386, 224, 175, 190, false, 2, TS_LEFT, 0, 30));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;

        //save password toggle
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x41B, 0x41D, 0x41B, 261, 212));

        //logon button
        Add(new CGUIButton(ID_1998_LOGON, 0x418, 0x419, 0x41A, 371, 222));
        //back button
        Add(new CGUIButton(ID_1998_BACK, 0x415, 0x416, 0x417, 161, 222));

        Add(new CGUIPage(3)); //page 3: never shown, never selected

        // CMainScreen::Load()/Save() always dereference the auto login
        // checkbox, so keep one alive off-screen (the 1998 screens have none).
        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 0, 0));

        // the title screen reset lives in CMainScreen::Init: a rebuild here
        // runs after button handlers set the page (ID_1998_LOGIN opens the
        // AcctLogin panel) and would yank the user back mid-flow
        return;
    }

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
#if defined(XUO_LAUNCHER_EMBEDDED)
        Add(new CGUIButton(ID_MS_LAUNCHER, 0x08D1, 0x08D1, 0x08D1, 590, 430));
#endif
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x00D2, 0x00D3, 0x00D2, 328, 417));
        g_MainScreen.m_SavePassword->SetTextParameters(
            9, "Save Password", 0x0386, STP_RIGHT_CENTER);

        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 183, 417));
        g_MainScreen.m_AutoLogin->SetTextParameters(9, "Auto Login", 0x0386, STP_RIGHT_CENTER);

        text = (CGUIText *)Add(new CGUIText(0x034E, 286, 455));
        text->CreateTextureA(9, astr_t("UO Version " + g_Config.ClientVersionString + "."));

        text = (CGUIText *)Add(new CGUIText(0x034E, 286, 467));
        text->CreateTextureA(9, astr_t(CLIENT_VERSION));
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

#if defined(XUO_LAUNCHER_EMBEDDED)
        Add(new CGUIButton(ID_MS_LAUNCHER, 0x08D1, 0x08D1, 0x08D1, 590, 430));
#endif

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
        text->CreateTextureA(9, astr_t("UO Version " + g_Config.ClientVersionString + "."));
        text = (CGUIText *)Add(new CGUIText(0x7F2, 200, 460));
        text->CreateTextureA(9, astr_t(CLIENT_VERSION));
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
        case ID_MS_LAUNCHER:
        {
            g_ToolTip.Set(L"Open X:UO Launcher", 120);
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
        case ID_1998_LOGON:
        {
            g_MainScreen.CreateSmoothAction(CMainScreen::ID_SMOOTH_MS_CONNECT);
            break;
        }
        case ID_1998_LOGIN:
        {
            Page = 2; //AcctLogin panel page
            WantRedraw = true;
            WantUpdateContent = true; //re-run RecalculateSize for the page-2 gump extent
            break;
        }
        case ID_1998_BACK:
        {
            Page = 1; //MainMenu title page
            WantRedraw = true;
            WantUpdateContent = true;
            break;
        }
#if defined(XUO_LAUNCHER_EMBEDDED)
        case ID_MS_LAUNCHER:
        {
            LauncherWindow_Open();
            break;
        }
#endif
        case ID_MS_UOACCOUNT:
        {
            Platform::OpenBrowser(g_Config.ShardAccount);
            break;
        }
        case ID_MS_UOCOM:
        {
            Platform::OpenBrowser(g_Config.ShardWeb);
            break;
        }
        case ID_MS_HELP:
        {
            Platform::OpenBrowser(g_Config.ShardHelp);
            break;
        }
        case ID_MS_MOVIE:
        {
            Platform::OpenBrowser(g_Config.ShardVideo);
            break;
        }
        case ID_MS_CREDITS:
        {
            Platform::OpenBrowser(g_Config.ShardCredits);
            break;
        }
        default:
            break;
    }
}

void CGumpScreenMain::GUMP_TEXT_ENTRY_EVENT_C
{
    if (serial == ID_MS_PASSWORD)
    {
        g_MainScreen.m_Password->SetPos(m_PasswordFake->Pos());
    }
}
