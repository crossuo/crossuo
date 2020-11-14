// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "CrossUO.h"

#include <common/str.h>
#include "SDL_wrapper.h"

#include "revision.h"
#include "Config.h"
#include "Misc.h"

#include "Point.h"
#include "Macro.h"
#include "CityList.h"
#include "Target.h"
#include "Weather.h"
#include "TargetGump.h"
#include "StumpsData.h"
#include "Party.h"
#include "ShaderData.h"
#include "ServerList.h"
#include "PressedObject.h"
#include "SelectedObject.h"
#include "ClickObject.h"
#include "GameWindow.h"
#include "UseItemsList.h"
#include "ContainerStack.h"
#include "Container.h"
#include "CharacterList.h"
#include "DateTimeStamp.h"
#include "Application.h"
#include "RenderWorldObject.h"

#if USE_PING
#include "Utility/PingThread.h"
#endif // USE_PING

#include "Crypt/CryptEntry.h"
#include <common/checksum.h>
#include <xuocore/mappedfile.h>
#include <xuocore/commoninterfaces.h>
#include <xuocore/uodata.h>
#include <xuocore/text_parser.h>
#include <common/utils.h>

#include "Walker/Walker.h"
#include "Walker/PathFinder.h"

#include "GameObjects/LandObject.h"
#include "GameObjects/GamePlayer.h"
#include "GameObjects/ObjectOnCursor.h"
#include "GameObjects/MapBlock.h"
#include "GameObjects/GameCharacter.h"

#include "TextEngine/EntryText.h"
#include "TextEngine/GameConsole.h"
#include "TextEngine/Journal.h"
#include "TextEngine/RenderTextObject.h"
#include "TextEngine/TextData.h"
#include "TextEngine/TextRenderer.h"

#include "Network/UOHuffman.h"
#include "Network/Packets.h"
#include "Network/PluginPackets.h"
#include "Network/Connection.h"

#include "Managers/AnimationManager.h"
#include "Managers/CityManager.h"
#include "Managers/ClilocManager.h"
#include "Managers/CustomHousesManager.h"
#include "Managers/IntlocManager.h"
#include "Managers/ObjectPropertiesManager.h"
#include "Managers/ColorManager.h"
#include "Managers/ConfigManager.h"
#include "Managers/ConnectionManager.h"
#include "Managers/CreateCharacterManager.h"
#include "Managers/EffectManager.h"
#include "Managers/FontsManager.h"
#include "Managers/GumpManager.h"
#include "Managers/MacroManager.h"
#include "Managers/MapManager.h"
#include "Managers/MouseManager.h"
#include "Managers/OptionsMacroManager.h"
#include "Managers/PacketManager.h"
#include "Managers/PluginManager.h"
#include "Managers/ProfessionManager.h"
#include "Managers/ScreenEffectManager.h"
#include "Managers/SkillGroupManager.h"
#include "Managers/SoundManager.h"
#include "Managers/SpeechManager.h"
#include "Managers/UOFileReader.h"
#include "Managers/CorpseManager.h"
#include "Managers/SkillsManager.h"

#include "ScreenStages/BaseScreen.h"
#include "ScreenStages/CharacterListScreen.h"
#include "ScreenStages/ConnectionScreen.h"
#include "ScreenStages/CreateCharacterScreen.h"
#include "ScreenStages/GameBlockedScreen.h"
#include "ScreenStages/GameScreen.h"
#include "ScreenStages/MainScreen.h"
#include "ScreenStages/SelectProfessionScreen.h"
#include "ScreenStages/SelectTownScreen.h"
#include "ScreenStages/ServerScreen.h"

#include "Gumps/GumpAbility.h"
#include "Gumps/GumpBaseScroll.h"
#include "Gumps/GumpBook.h"
#include "Gumps/GumpBuff.h"
#include "Gumps/GumpBulletinBoard.h"
#include "Gumps/GumpBulletinBoardItem.h"
#include "Gumps/GumpCombatBook.h"
#include "Gumps/GumpConsoleType.h"
#include "Gumps/GumpContainer.h"
#include "Gumps/GumpCustomHouse.h"
#include "Gumps/GumpDrag.h"
#include "Gumps/GumpSelectColor.h"
#include "Gumps/GumpDye.h"
#include "Gumps/GumpGeneric.h"
#include "Gumps/GumpGrayMenu.h"
#include "Gumps/GumpJournal.h"
#include "Gumps/GumpMap.h"
#include "Gumps/GumpMenu.h"
#include "Gumps/GumpMenubar.h"
#include "Gumps/GumpMinimap.h"
#include "Gumps/GumpNotify.h"
#include "Gumps/GumpOptions.h"
#include "Gumps/GumpPaperdoll.h"
#include "Gumps/GumpPartyManifest.h"
#include "Gumps/GumpPopupMenu.h"
#include "Gumps/GumpProfile.h"
#include "Gumps/GumpQuestion.h"
#include "Gumps/GumpRacialAbilitiesBook.h"
#include "Gumps/GumpRacialAbility.h"
#include "Gumps/GumpResourceTracker.h"
#include "Gumps/GumpScreenCharacterList.h"
#include "Gumps/GumpScreenConnection.h"
#include "Gumps/GumpScreenCreateCharacter.h"
#include "Gumps/GumpScreenGame.h"
#include "Gumps/GumpScreenMain.h"
#include "Gumps/GumpScreenSelectProfession.h"
#include "Gumps/GumpScreenSelectTown.h"
#include "Gumps/GumpScreenServer.h"
#include "Gumps/GumpSecureTrading.h"
#include "Gumps/GumpSelectFont.h"
#include "Gumps/GumpShop.h"
#include "Gumps/GumpSkill.h"
#include "Gumps/GumpSkills.h"
#include "Gumps/GumpSpell.h"
#include "Gumps/GumpSpellbook.h"
#include "Gumps/GumpStatusbar.h"
#include "Gumps/GumpTargetSystem.h"
#include "Gumps/GumpTextEntryDialog.h"
#include "Gumps/GumpTip.h"
#include "Gumps/GumpWorldMap.h"
#include "Gumps/GumpProperty.h"
#include "Gumps/GumpPropertyIcon.h"

#include <external/popts.h>
#include "Renderer/RenderAPI.h"

extern po::parser g_cli;

#if !defined(XUO_WINDOWS)
REVERSE_PLUGIN_INTERFACE g_oaReverse;
#endif

PLUGIN_CLIENT_INTERFACE g_PluginClientInterface = {};

bool CDECL PluginRecvFunction(uint8_t *buf, size_t size);
bool CDECL PluginSendFunction(uint8_t *buf, size_t size);

typedef void CDECL PLUGIN_INIT_TYPE(PLUGIN_INFO *);
static PLUGIN_INIT_TYPE *g_PluginInit = nullptr;

struct CFileWriter
{
    CFileWriter() {}
    ~CFileWriter() { Close(); }

    void Close()
    {
        if (m_File == nullptr)
        {
            return;
        }

        fs_close(m_File);
        m_File = nullptr;
    }

    void Init(const fs_path &filePath)
    {
        if (m_File != nullptr)
        {
            fs_close(m_File);
        }
        m_File = fs_open(filePath, FS_WRITE);
        FileName = filePath;
    }

    void Print(const char *format, ...)
    {
        if (m_File == nullptr)
        {
            return;
        }

        va_list arg;
        va_start(arg, format);
        vfprintf(m_File, format, arg);
        va_end(arg);
        fflush(m_File);
    }

    FILE *m_File = nullptr;
    fs_path FileName;
};

CGame g_Game;

CGame::CGame()
{
}

CGame::~CGame()
{
}

astr_t CGame::DecodeArgumentString(const char *text, int length)
{
    astr_t result{};

    for (int i = 0; i < length; i += 2)
    {
        char buf[5] = { '0', 'x', text[i], text[i + 1], 0 };

        char *end = nullptr;
        result += (char)strtoul(buf, &end, 16);
    }

    return result;
}

void CGame::ProcessCommandLine()
{
    if (g_cli["nocrypt"].was_set())
    {
        g_Config.EncryptionType = ET_NOCRYPT;
    }

    if (g_cli["fastlogin"].was_set())
    {
        g_GameWindow.CreateTimer(FASTLOGIN_TIMER_ID, 50);
    }

    if (g_cli["autologin"].was_set())
    {
        g_MainScreen.m_AutoLogin->Checked = g_cli["autologin"].get().u32 != 0;
    }

    if (g_cli["savepassword"].was_set())
    {
        g_MainScreen.m_SavePassword->Checked = g_cli["savepassword"].get().u32 != 0;
    }

    if (g_cli["host"].was_set())
    {
        m_OverrideServerAddress = g_cli["host"].get().string;
        m_OverrideServerPort = g_cli["port"].get().u32;
    }

    if (g_cli["port"].was_set())
    {
        m_OverrideServerPort = g_cli["port"].get().u32;
    }

    if (g_cli["proxy-host"].was_set() && g_cli["proxy-port"].was_set())
    {
        g_ConnectionManager.SetUseProxy(true);
        g_ConnectionManager.SetProxyAddress(g_cli["proxy-host"].get().string);
        g_ConnectionManager.SetProxyPort(g_cli["proxy-port"].get().u32);
    }

    if (g_cli["proxy-user"].was_set())
    {
        g_ConnectionManager.SetProxySocks5(true);
        g_ConnectionManager.SetProxyAccount(g_cli["proxy-user"].get().string);
    }

    if (g_cli["proxy-password"].was_set())
    {
        g_ConnectionManager.SetProxyPassword(g_cli["proxy-password"].get().string);
    }

    if (g_cli["login"].was_set())
    {
        auto l = g_cli["login"].get().string;
        auto p = g_cli["password"].get().string;
        g_MainScreen.SetAccounting(l, p);
    }

    if (g_cli["character"].was_set())
    {
        auto name = g_cli["character"].get().string;
        if (g_PacketManager.AutoLoginNames.length() != 0u)
        {
            g_PacketManager.AutoLoginNames = astr_t("|") + name + g_PacketManager.AutoLoginNames;
        }
        else
        {
            g_PacketManager.AutoLoginNames = astr_t("|") + name;
        }
    }
}

bool CGame::Install()
{
    Info(Client, "CGame::Install()");
    auto buildStamp = GetBuildDateTimeStamp();
    Info(Client, "CrossUO version is: %s (build %s)", PRODUCT_VERSION_STR, buildStamp.c_str());
    if (g_Config.LocaleOverride.empty())
    {
        Platform::SetLanguageFromSystemLocale();
    }
    else
    {
        g_Language = str_lower(g_Config.LocaleOverride);
    }
    fs_path_create(g_App.ExeFilePath("screenshots"));

    Info(Config, "client config loaded");
    InitSpells();

    LoadAutoLoginNames();

    Info(Client, "loading files");
    if (!g_FileManager.Load())
    {
        auto errMsg =
            astr_t(
                "Error loading a memmapped file. Please check the log for more info.\nUsing UO search path: ") +
            fs_path_str(g_App.m_UOPath);
        g_GameWindow.ShowMessage(errMsg, "FileManager::Load");
        Error(Client, "%s", errMsg.c_str());
        return false;
    }

    g_SkillsManager.Init();
    g_ColorManager.Init();
    g_SpeechManager.LoadSpeech();

    InitStaticAnimList();

    Info(Client, "loading fonts");
    if (!g_FontManager.LoadFonts())
    {
        Error(Client, "loading fonts");
        g_GameWindow.ShowMessage("Error loading fonts", "Error");
        return false;
    }

    CheckStaticTileFilterFiles();
    CSize statusbarDims = GetGumpDimension(0x0804);

    CGumpStatusbar::m_StatusbarDefaultWidth = statusbarDims.Width;
    CGumpStatusbar::m_StatusbarDefaultHeight = statusbarDims.Height;

    Info(Client, "loading cursors");
    if (!g_MouseManager.LoadCursorTextures())
    {
        Info(Client, "loading cursors");
        g_GameWindow.ShowMessage("Error loading cursors", "Error");
        return false;
    }

    if (!g_SoundManager.Init())
    {
        g_GameWindow.ShowMessage("Failed to init audio system.", "Warning");
    }

    ContainerInit();

    g_CityManager.Init();

    g_EntryPointer = nullptr;

    Info(Client, "load prof");
    g_ProfessionManager.Load();
    g_ProfessionManager.Selected = (CBaseProfession *)g_ProfessionManager.m_Items;

    ExecuteStaticArt(0x0EED); //gp 1
    ExecuteStaticArt(0x0EEE); //gp 2-5
    ExecuteStaticArt(0x0EEF); //gp 6+

    g_CreateCharacterManager.Init();

    Info(Client, "loading client startup config");
    LoadClientStartupConfig();

    uint16_t b = 0x0000;
    uint16_t r = 0xFC00; // 0xFF0000FF;
    uint16_t g = 0x83E0; // 0xFF00FF00;

    // clang-format off
    // lock pixmap open/closed
    uint16_t pdwlt[2][140] =
    {
        {
            b, b, b, g, g, g, g, g, g, g,
            b, b, b, g, g, g, g, g, g, g,
            b, b, b, b, g, g, b, b, b, b,
            b, b, b, b, g, g, b, b, b, b,
            b, b, b, b, g, g, b, b, b, b,
            g, g, g, g, g, g, g, g, g, b,
            g, g, g, g, g, g, g, g, g, b,
            g, g, b, b, b, b, b, g, g, b,
            g, g, b, g, g, b, b, g, g, b,
            g, g, b, b, g, b, b, g, g, b,
            g, g, b, b, g, b, b, g, g, b,
            g, g, g, b, b, b, g, g, g, b,
            b, g, g, g, g, g, g, g, b, b,
            b, b, g, g, g, g, g, b, b, b
        },
        {
            b, r, r, r, r, r, r, r, b, b,
            b, r, r, r, r, r, r, r, b, b,
            b, b, r, r, b, r, r, b, b, b,
            b, b, r, r, b, r, r, b, b, b,
            b, b, r, r, b, r, r, b, b, b,
            r, r, r, r, r, r, r, r, r, b,
            r, r, r, r, r, r, r, r, r, b,
            r, r, b, b, b, b, b, r, r, b,
            r, r, b, r, r, b, b, r, r, b,
            r, r, b, b, r, b, b, r, r, b,
            r, r, b, b, r, b, b, r, r, b,
            r, r, r, b, b, b, r, r, r, b,
            b, r, r, r, r, r, r, r, b, b,
            b, b, r, r, r, r, r, b, b, b
        }
    };
    // clang-format on

    for (int i = 0; i < 2; i++)
    {
#ifndef NEW_RENDERER_ENABLED
        g_GL.BindTexture16(g_TextureGumpState[i], 10, 14, &pdwlt[i][0]);
#else
        g_TextureGumpState[i].Width = 10;
        g_TextureGumpState[i].Height = 14;
        g_TextureGumpState[i].Texture = Render_CreateTexture2D(
            10,
            14,
            TextureGPUFormat::TextureGPUFormat_RGB5_A1,
            &pdwlt[i][0],
            TextureFormat::TextureFormat_Unsigned_A1_BGR5);
        assert(g_TextureGumpState[i].Texture != RENDER_TEXTUREHANDLE_INVALID);
#endif
    }

    memset(&m_WinterTile[0], 0, sizeof(m_WinterTile));

    m_WinterTile[196] = 0x011A;
    m_WinterTile[197] = 0x011B;
    m_WinterTile[198] = 0x011C;
    m_WinterTile[199] = 0x011D;
    m_WinterTile[206] = 0x05C0;
    m_WinterTile[248] = 0x011A;
    m_WinterTile[249] = 0x011B;
    m_WinterTile[250] = 0x011C;
    m_WinterTile[251] = 0x011D;
    m_WinterTile[804] = 0x0391;
    m_WinterTile[805] = 0x0392;
    m_WinterTile[806] = 0x0393;
    m_WinterTile[807] = 0x0394;
    m_WinterTile[808] = 0x0395;
    m_WinterTile[809] = 0x0396;
    m_WinterTile[1521] = 0x011A;
    m_WinterTile[1522] = 0x011B;
    m_WinterTile[1523] = 0x011C;
    m_WinterTile[1524] = 0x011D;
    m_WinterTile[1529] = 0x011A;
    m_WinterTile[1530] = 0x011B;
    m_WinterTile[1531] = 0x011C;
    m_WinterTile[1532] = 0x011D;
    m_WinterTile[1533] = 0x011B;
    m_WinterTile[1534] = 0x011C;
    m_WinterTile[1535] = 0x011D;
    m_WinterTile[1536] = 0x011B;
    m_WinterTile[1537] = 0x011C;
    m_WinterTile[1538] = 0x011D;
    m_WinterTile[1539] = 0x011C;
    m_WinterTile[1540] = 0x011D;

    Info(Client, "initializing light buffer");
    g_LightBuffer.Init(640, 480);

    Info(Client, "creating object handles");
    CreateObjectHandlesBackground();

    Info(Client, "creating aura");
    CreateAuraTexture();

    Info(Client, "loading shaders");
    LoadShaders();

    Info(Client, "updating main screen content");
    g_MainScreen.UpdateContent();

    Info(Client, "initializing screen");
    InitScreen(GS_MAIN);

    Info(Client, "initialization completed");
    return true;
}

template <typename T, size_t SIZE>
void ValidateSpriteIsDeleted(T (&arr)[SIZE])
{
    for (int i = 0; i < SIZE; ++i)
    {
        CIndexObject &obj = arr[i];
        (void)obj;
        assert(obj.UserData == nullptr);
    }
}

void DestroySprite(CIndexObject *obj)
{
    auto spr = (CSprite *)obj->UserData;
    if (spr != nullptr)
    {
        spr->Clear();
        delete spr;
        obj->UserData = nullptr;
    }
}

void CGame::UnloadIndexFiles()
{
    std::deque<CIndexObject *> *lists[] = {
        &m_UsedLandList, &m_UsedStaticList, &m_UsedGumpList, &m_UsedTextureList, &m_UsedLightList
    };

    for (int i = 0; i < countof(lists); i++)
    {
        auto &list = *lists[i];
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            DestroySprite(*it);
        }
        list.clear();
    }

    for (auto it = m_UsedSoundList.begin(); it != m_UsedSoundList.end(); ++it)
    {
        CIndexSound *obj = *it;
        if (obj->UserData != SOUND_NULL)
        {
            g_SoundManager.UpdateSoundEffect((SoundHandle)obj->UserData, -1);
            obj->UserData = SOUND_NULL;
        }
    }
    m_UsedSoundList.clear();

    ValidateSpriteIsDeleted(g_Index.m_Land);
    ValidateSpriteIsDeleted(g_Index.m_Static);
    ValidateSpriteIsDeleted(g_Index.m_Gump);
    ValidateSpriteIsDeleted(g_Index.m_Texture);
    ValidateSpriteIsDeleted(g_Index.m_Light);
}

void CGame::Uninstall()
{
    Info(Client, "CGame::Uninstall()");
    SaveLocalConfig(g_PacketManager.ConfigSerial);
    g_MainScreen.Save();
    SaveGlobalConfig();
    g_GumpManager.OnDelete();

    Disconnect();

    UnloadIndexFiles();

    g_EntryPointer = nullptr;
    g_CurrentScreen = nullptr;

    g_AuraTexture.Clear();

    for (int i = 0; i < MAX_MAPS_COUNT; i++)
    {
        g_MapTexture[i].Clear();
    }

    for (int i = 0; i < 2; i++)
    {
        g_TextureGumpState[i].Clear();
    }

    g_SoundManager.Free();
    g_FileManager.Unload();
    g_LightBuffer.Free();

#ifndef NEW_RENDERER_ENABLED
    g_GL.Uninstall();
#else
    Render_Shutdown();
#endif
}

void CGame::InitScreen(GAME_STATE state)
{
    g_GameState = state;
    g_SelectedObject.Clear();
    g_LastSelectedObject.Clear();
    g_PressedObject.ClearAll();

    switch (state)
    {
        case GS_MAIN:
        {
            g_CurrentScreen = &g_MainScreen;
            break;
        }
        case GS_MAIN_CONNECT:
        {
            g_CurrentScreen = &g_ConnectionScreen;
            break;
        }
        case GS_SERVER:
        {
            g_CurrentScreen = &g_ServerScreen;
            break;
        }
        case GS_SERVER_CONNECT:
        {
            g_CurrentScreen = &g_ConnectionScreen;
            break;
        }
        case GS_CHARACTER:
        {
            g_CurrentScreen = &g_CharacterListScreen;
            break;
        }
        case GS_DELETE:
        {
            g_CurrentScreen = &g_ConnectionScreen;
            break;
        }
        case GS_PROFESSION_SELECT:
        {
            g_CurrentScreen = &g_SelectProfessionScreen;
            break;
        }
        case GS_CREATE:
        {
            g_CurrentScreen = &g_CreateCharacterScreen;
            break;
        }
        case GS_SELECT_TOWN:
        {
            g_CurrentScreen = &g_SelectTownScreen;
            break;
        }
        case GS_GAME_CONNECT:
        {
            g_CurrentScreen = &g_ConnectionScreen;
            break;
        }
        case GS_GAME:
        {
            g_CurrentScreen = &g_GameScreen;
            break;
        }
        case GS_GAME_BLOCKED:
        {
            g_CurrentScreen = &g_GameBlockedScreen;
            break;
        }
        default:
            break;
    }

    if (g_CurrentScreen != nullptr)
    {
        g_CurrentScreen->Init();
    }
}

uint16_t CGame::TextToGraphic(const char *text)
{
    if (strlen(text) > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        long l = strtol(text + 2, nullptr, 16);

        if (l < 0 || l > 0xFFFF)
        {
            return 0xFFFF;
        }

        return (uint16_t)l;
    }
    if (text[0] >= '0' && text[0] <= '9')
    {
        return atoi(text);
    }

    return 0;
}

void CGame::CheckStaticTileFilterFiles()
{
    memset(&m_StaticTilesFilterFlags[0], 0, sizeof(m_StaticTilesFilterFlags));

    auto path = g_App.FilePath("data");
    fs_path_create(path);

    auto filePath = fs_path_join(path, "cave.txt");
    if (!fs_path_exists(filePath))
    {
        CFileWriter file;
        file.Init(filePath);
        file.Print("#Format: graphic\n");
        for (int i = 0x053B; i < 0x0553 + 1; i++)
        {
            if (i != 0x0550)
            {
                file.Print("0x%04X\n", i);
            }
        }
    }

    filePath = fs_path_join(path, "vegetation.txt");
    CFileWriter vegetationFile;
    if (!fs_path_exists(filePath))
    {
        vegetationFile.Init(filePath);
        vegetationFile.Print("#Format: graphic\n");
        static const int vegetationTilesCount = 178;
        static const uint16_t vegetationTiles[vegetationTilesCount] = {
            0x0D45, 0x0D46, 0x0D47, 0x0D48, 0x0D49, 0x0D4A, 0x0D4B, 0x0D4C, 0x0D4D, 0x0D4E, 0x0D4F,
            0x0D50, 0x0D51, 0x0D52, 0x0D53, 0x0D54, 0x0D5C, 0x0D5D, 0x0D5E, 0x0D5F, 0x0D60, 0x0D61,
            0x0D62, 0x0D63, 0x0D64, 0x0D65, 0x0D66, 0x0D67, 0x0D68, 0x0D69, 0x0D6D, 0x0D73, 0x0D74,
            0x0D75, 0x0D76, 0x0D77, 0x0D78, 0x0D79, 0x0D7A, 0x0D7B, 0x0D7C, 0x0D7D, 0x0D7E, 0x0D7F,
            0x0D80, 0x0D83, 0x0D87, 0x0D88, 0x0D89, 0x0D8A, 0x0D8B, 0x0D8C, 0x0D8D, 0x0D8E, 0x0D8F,
            0x0D90, 0x0D91, 0x0D93, 0x12B6, 0x12B7, 0x12BC, 0x12BD, 0x12BE, 0x12BF, 0x12C0, 0x12C1,
            0x12C2, 0x12C3, 0x12C4, 0x12C5, 0x12C6, 0x12C7, 0x0CB9, 0x0CBC, 0x0CBD, 0x0CBE, 0x0CBF,
            0x0CC0, 0x0CC1, 0x0CC3, 0x0CC5, 0x0CC6, 0x0CC7, 0x0CF3, 0x0CF4, 0x0CF5, 0x0CF6, 0x0CF7,
            0x0D04, 0x0D06, 0x0D07, 0x0D08, 0x0D09, 0x0D0A, 0x0D0B, 0x0D0C, 0x0D0D, 0x0D0E, 0x0D0F,
            0x0D10, 0x0D11, 0x0D12, 0x0D13, 0x0D14, 0x0D15, 0x0D16, 0x0D17, 0x0D18, 0x0D19, 0x0D28,
            0x0D29, 0x0D2A, 0x0D2B, 0x0D2D, 0x0D34, 0x0D36, 0x0DAE, 0x0DAF, 0x0DBA, 0x0DBB, 0x0DBC,
            0x0DBD, 0x0DBE, 0x0DC1, 0x0DC2, 0x0DC3, 0x0C83, 0x0C84, 0x0C85, 0x0C86, 0x0C87, 0x0C88,
            0x0C89, 0x0C8A, 0x0C8B, 0x0C8C, 0x0C8D, 0x0C8E, 0x0C93, 0x0C94, 0x0C98, 0x0C9F, 0x0CA0,
            0x0CA1, 0x0CA2, 0x0CA3, 0x0CA4, 0x0CA7, 0x0CAC, 0x0CAD, 0x0CAE, 0x0CAF, 0x0CB0, 0x0CB1,
            0x0CB2, 0x0CB3, 0x0CB4, 0x0CB5, 0x0CB6, 0x0C45, 0x0C46, 0x0C49, 0x0C47, 0x0C48, 0x0C4A,
            0x0C4B, 0x0C4C, 0x0C4D, 0x0C4E, 0x0C37, 0x0C38, 0x0CBA, 0x0D2F, 0x0D32, 0x0D33, 0x0D3F,
            0x0D40, 0x0CE9
        };

        for (int i = 0; i < vegetationTilesCount; i++)
        {
            const int64_t flags = g_Game.GetStaticFlags(vegetationTiles[i]);
            if ((flags & 0x00000040) != 0)
            {
                continue;
            }
            vegetationFile.Print("0x%04X\n", vegetationTiles[i]);
        }
    }

    filePath = fs_path_join(path, "stumps.txt");
    if (!fs_path_exists(filePath))
    {
        CFileWriter file;
        file.Init(filePath);
        file.Print("#Format: graphic hatched\n");
        static const int treeTilesCount = 53;
        static const uint16_t treeTiles[treeTilesCount] = {
            0x0CCA, 0x0CCB, 0x0CCC, 0x0CCD, 0x0CD0, 0x0CD3, 0x0CD6, 0x0CD8, 0x0CDA, 0x0CDD, 0x0CE0,
            0x0CE3, 0x0CE6, 0x0D41, 0x0D42, 0x0D43, 0x0D44, 0x0D57, 0x0D58, 0x0D59, 0x0D5A, 0x0D5B,
            0x0D6E, 0x0D6F, 0x0D70, 0x0D71, 0x0D72, 0x0D84, 0x0D85, 0x0D86, 0x0D94, 0x0D98, 0x0D9C,
            0x0DA0, 0x0DA4, 0x0DA8, 0x0C9E, 0x0CA8, 0x0CAA, 0x0CAB, 0x0CC9, 0x0CF8, 0x0CFB, 0x0CFE,
            0x0D01, 0x12B6, 0x12B7, 0x12B8, 0x12B9, 0x12BA, 0x12BB, 0x12BC, 0x12BD
        };

        for (int i = 0; i < treeTilesCount; i++)
        {
            const uint16_t graphic = treeTiles[i];
            uint8_t hatched = 1;
            switch (graphic)
            {
                case 0x0C9E:
                case 0x0CA8:
                case 0x0CAA:
                case 0x0CAB:
                case 0x0CC9:
                case 0x0CF8:
                case 0x0CFB:
                case 0x0CFE:
                case 0x0D01:
                case 0x12B6:
                case 0x12B7:
                case 0x12B8:
                case 0x12B9:
                case 0x12BA:
                case 0x12BB:
                    hatched = 0;
                default:
                    break;
            }

            const int64_t flags = g_Game.GetStaticFlags(graphic);
            if ((flags & 0x00000040) == 0)
            {
                vegetationFile.Print("0x%04X\n", graphic);
            }
            else
            {
                file.Print("0x%04X\t%i\n", graphic, hatched);
            }
        }
    }

    filePath = fs_path_join(path, "cave.txt");
    TextFileParser caveParser(filePath, " \t", "#;//", "");
    while (!caveParser.IsEOF())
    {
        auto strings = caveParser.ReadTokens();
        if (!strings.empty())
        {
            uint16_t graphic = TextToGraphic(strings[0].c_str());
            m_StaticTilesFilterFlags[graphic] |= STFF_CAVE;
            g_Data.m_CaveTiles.push_back(graphic);
        }
    }

    filePath = fs_path_join(path, "stumps.txt");
    TextFileParser stumpParser(filePath, " \t", "#;//", "");
    while (!stumpParser.IsEOF())
    {
        auto strings = stumpParser.ReadTokens();
        if (strings.size() >= 2)
        {
            uint8_t flag = STFF_STUMP;
            if (str_to_int(strings[1]) != 0)
            {
                flag |= STFF_STUMP_HATCHED;
            }
            const uint16_t graphic = TextToGraphic(strings[0].c_str());
            m_StaticTilesFilterFlags[graphic] |= flag;
            g_Data.m_StumpTiles.push_back(graphic);
        }
    }

    filePath = fs_path_join(path, "vegetation.txt");
    TextFileParser vegetationParser(filePath, " \t", "#;//", "");
    while (!vegetationParser.IsEOF())
    {
        auto strings = vegetationParser.ReadTokens();
        if (!strings.empty())
        {
            m_StaticTilesFilterFlags[TextToGraphic(strings[0].c_str())] |= STFF_VEGETATION;
        }
    }
}

void CGame::LoadAutoLoginNames()
{
    TextFileParser file(g_App.UOFilesPath("autologinnames.cfg"), "", "#;", "");

    auto names = g_PacketManager.AutoLoginNames + "|";

    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens(false);

        if (static_cast<unsigned int>(!strings.empty()) != 0u)
        {
            names += strings[0] + "|";
        }
    }

    g_PacketManager.AutoLoginNames = names;
}

void CGame::ProcessDelayedClicks()
{
    if (g_ClickObject.Enabled && g_ClickObject.Timer < g_Ticks)
    {
        uint32_t serial = 0;

        if (g_ClickObject.Object != nullptr)
        {
            serial = g_ClickObject.Object->Serial;
        }

        if (g_ClickObject.Gump == nullptr)
        {
            if (serial != 0u)
            {
                CGameObject *go = (CGameObject *)g_ClickObject.Object;

                if (!g_TooltipsEnabled || (!go->NPC && go->Locked()))
                {
                    Click(serial);
                }

                g_ObjectPropertiesManager.OnItemClicked(serial);

                if (g_PopupEnabled && (!g_ConfigManager.HoldShiftForContextMenus || g_ShiftPressed))
                {
                    CPacketRequestPopupMenu(serial).Send();
                }
            }
        }
        else
        {
            g_ClickObject.Gump->DelayedClick(g_ClickObject.Object);
        }

        g_ClickObject.Clear();
    }
}

void CGame::Process(bool rendering)
{
    if (g_CurrentScreen == nullptr)
    {
        return;
    }

    g_MouseManager.Update();

    if (g_GameState >= GS_CHARACTER && (g_LastSendTime + SEND_TIMEOUT_DELAY) < g_Ticks)
    {
        uint8_t ping[2] = { 0x73, 0 };
        Send(ping, 2);
    }

    const bool oldCtrl = g_CtrlPressed;
    const bool oldShift = g_ShiftPressed;

    auto mod = SDL_GetModState();
    g_AltPressed = ((mod & KMOD_ALT) != 0);
    g_CtrlPressed = ((mod & KMOD_CTRL) != 0);
    g_CmdPressed = ((mod & KMOD_LGUI) != 0);
    g_ShiftPressed = ((mod & KMOD_SHIFT) != 0);

    if (g_GameState >= GS_GAME) // || g_GameState == GS_GAME_BLOCKED)
    {
        if (g_LogoutAfterClick)
        {
            g_LogoutAfterClick = false;
            LogOut();
            return;
        }

#if USE_PING
        if (!g_DisablePing && g_ConfigManager.CheckPing && g_PingTimer < g_Ticks)
        {
            CPingThread *pingThread = new CPingThread(0xFFFFFFFF, m_GameServerIP, 10);
            pingThread->Run();
            g_PingTimer = g_Ticks + (g_ConfigManager.PingTimer * 1000);
        }
#endif // USE_PING

        g_UseItemActions.Process();
        g_ShowGumpLocker = g_ConfigManager.LockGumpsMoving && g_AltPressed && g_CtrlPressed;
        ProcessStaticAnimList();

        g_EffectManager.UpdateEffects();
        CGumpBuff *gumpBuff = (CGumpBuff *)g_GumpManager.GetGump(0, 0, GT_BUFF);
        if (gumpBuff != nullptr)
        {
            gumpBuff->UpdateBuffIcons();
        }

        if (g_World != nullptr)
        {
            g_World->ProcessAnimation();
        }

        g_PathFinder.ProcessAutowalk();
        bool canRenderSelect = false;
        if (g_GameState == GS_GAME)
        {
            g_MouseManager.ProcessWalking();
            g_MacroManager.Execute();
            ProcessDelayedClicks();
            canRenderSelect = true;
            if (g_PressedObject.LeftGump == nullptr && g_PressedObject.LeftObject != nullptr &&
                g_PressedObject.LeftObject->IsGUI())
            {
                canRenderSelect = false;
            }
        }

        if (g_World != nullptr)
        {
            if (g_World->ObjectToRemove != 0)
            {
                CGameObject *removeObj = g_World->FindWorldObject(g_World->ObjectToRemove);
                g_World->ObjectToRemove = 0;
                if (removeObj != nullptr)
                {
                    CGameCharacter *character = g_World->FindWorldCharacter(removeObj->Container);
                    g_World->RemoveObject(removeObj);
                    if (character != nullptr)
                    {
                        character->m_FrameInfo =
                            g_AnimationManager.CollectFrameInformation(character);
                        g_GumpManager.UpdateContent(g_ObjectInHand.Container, 0, GT_PAPERDOLL);
                    }
                }
            }

            if (g_ProcessRemoveRangedTimer < g_Ticks)
            {
                g_Game.RemoveRangedObjects();
                g_ProcessRemoveRangedTimer = g_Ticks + 50;
            }

            if (g_ConfigManager.ObjectHandles && g_CtrlPressed && g_ShiftPressed &&
                (oldCtrl != g_CtrlPressed || oldShift != g_ShiftPressed))
            {
                g_World->ResetObjectHandlesState();
            }

            if (rendering)
            {
                g_GameScreen.CalculateGameWindowBounds();
                g_GameScreen.CalculateRenderList();
                g_GameScreen.RenderListInitalized = true;
                g_SelectedObject.Clear();
                g_SelectedGameObjectHandle = 0;
                if (!g_GameWindow.IsMinimizedWindow())
                {
                    if (canRenderSelect)
                    {
                        g_GameScreen.SelectObject();
                    }

                    CGump::ProcessListing();
                    g_GameScreen.PrepareContent();
                    g_GameScreen.Render();

                    UOI_SELECTED_TILE uoiSelectedObject;
                    if (g_SelectedObject.Object != nullptr &&
                        g_SelectedObject.Object->IsWorldObject())
                    {
                        CRenderWorldObject *rwo = (CRenderWorldObject *)g_SelectedObject.Object;
                        if (rwo->IsLandObject())
                        {
                            uoiSelectedObject.Serial = 0xFFFFFFFF;
                        }
                        else if (!rwo->IsGameObject())
                        {
                            uoiSelectedObject.Serial = 0;
                        }
                        else
                        {
                            uoiSelectedObject.Serial = rwo->Serial;
                        }

                        uoiSelectedObject.Graphic = rwo->Graphic;
                        uoiSelectedObject.Color = rwo->Color;
                        uoiSelectedObject.X = rwo->GetX();
                        uoiSelectedObject.Y = rwo->GetY();
                        uoiSelectedObject.Z = rwo->GetZ();

                        rwo = rwo->GetLand();
                        if (rwo != nullptr)
                        {
                            uoiSelectedObject.LandGraphic = rwo->Graphic;
                            uoiSelectedObject.LandX = rwo->GetX();
                            uoiSelectedObject.LandY = rwo->GetY();
                            uoiSelectedObject.LandZ = rwo->GetZ();
                        }
                        else
                        {
                            uoiSelectedObject.LandGraphic = 0;
                            uoiSelectedObject.LandX = 0;
                            uoiSelectedObject.LandY = 0;
                            uoiSelectedObject.LandZ = 0;
                        }
                    }
                    else
                    {
                        memset(&uoiSelectedObject, 0, sizeof(uoiSelectedObject));
                    }
                    PLUGIN_EVENT(UOMSG_SELECTED_TILE, &uoiSelectedObject);
                }
                g_Target.UnloadMulti();
                g_GameScreen.RenderListInitalized = false;
                g_MapManager.Init(true);
            }
        }
    }
    else if (rendering)
    {
        g_SelectedObject.Clear();
        if (!g_GameWindow.IsMinimizedWindow())
        {
            g_CurrentScreen->SelectObject();
            g_CurrentScreen->PrepareContent();
            CGump::ProcessListing();
            g_CurrentScreen->Render();
        }
    }

    static uint32_t removeUnusedTexturesTime = 0;
    if (removeUnusedTexturesTime < g_Ticks)
    {
        ClearUnusedTextures();
        removeUnusedTexturesTime = g_Ticks + CLEAR_TEXTURES_DELAY;
    }

    g_AnimationManager.GarbageCollect();
}

void CGame::LoadStartupConfig(int serial)
{
    char buf[FS_MAX_PATH] = { 0 };
    CServer *server = g_ServerList.GetSelectedServer();
    const auto &acct = g_MainScreen.m_Account->GetTextA();
    if (server != nullptr)
    {
        sprintf_s(
            buf, "desktop/%s/%s/0x%08X", acct.c_str(), FixServerName(server->Name).c_str(), serial);
    }
    else
    {
        sprintf_s(buf, "desktop/%s/0x%08X", acct.c_str(), serial);
    }

    if (!g_ConfigManager.Load(g_App.FilePath("%s/%s", buf, "options.cfg")))
    {
        // DEPRECATE: 1.0.6
        if (!g_ConfigManager.Load(g_App.ExeFilePath("%s/%s", buf, "options.cfg")))
        {
            g_ConfigManager.Load(g_App.UOFilesPath("%s/%s", buf, "options.cfg"));
        }
    }

    g_SoundManager.SetMusicVolume(g_ConfigManager.GetMusicVolume());
    if (!g_ConfigManager.GetSound())
    {
        AdjustSoundEffects(g_Ticks + 100000);
    }

    if (!g_ConfigManager.GetMusic())
    {
        g_SoundManager.StopMusic();
    }
}

void CGame::LoadPlugin(const fs_path &libpath, const astr_t &function, int flags)
{
    Info(Client, "Loading plugin: %s", fs_path_ascii(libpath));
    auto dll = SDL_LoadObject(fs_path_ascii(libpath));
    if (dll != nullptr)
    {
        auto initFunc = (PluginEntry *)SDL_LoadFunction(dll, function.c_str());
        if (!InstallPlugin(initFunc, flags))
        {
            SDL_UnloadObject(dll);
        }
        // FIXME: dll leaks, pass handle into CPlugin to be closed
    }
    else
    {
        Info(Client, "Failed with error: %s", SDL_GetError());
    }
}

bool CGame::InstallPlugin(PluginEntry *initFunc, int flags)
{
    CPlugin *plugin = nullptr;
    if (initFunc != nullptr)
    {
        plugin = new CPlugin(flags);
        initFunc(plugin->m_PPS);
    }

    if (plugin == nullptr)
    {
        return false;
    }

    plugin->m_PPS->Owner = plugin;
    if (plugin->CanClientAccess())
    {
        plugin->m_PPS->Client = &g_PluginClientInterface;
    }

    if (plugin->CanParseRecv())
    {
        plugin->m_PPS->Recv = PluginRecvFunction;
    }

    if (plugin->CanParseSend())
    {
        plugin->m_PPS->Send = PluginSendFunction;
    }

    initFunc(plugin->m_PPS);
    g_PluginManager.Add(plugin);
    return true;
}

void CGame::LoadPlugins()
{
    g_PluginClientInterface.Version = 2;
    g_PluginClientInterface.Size = sizeof(g_PluginClientInterface);
    g_PluginClientInterface.GL = &g_Interface_GL;
    g_PluginClientInterface.UO = &g_Interface_UO;
    g_PluginClientInterface.ClilocManager = &g_Interface_ClilocManager;
    g_PluginClientInterface.ColorManager = &g_Interface_ColorManager;
    g_PluginClientInterface.PathFinder = &g_Interface_PathFinder;
    g_PluginClientInterface.FileManager = &g_Interface_FileManager;

    // Load X:UO Assist first, it is a special plugin due Qt mainthread issue
#if defined(XUO_WINDOWS)
    LoadPlugin(g_App.ExeFilePath("xuoassist.dll"), "Install", 0xFFFFFFFF);
#else
    InstallPlugin(g_oaReverse.Install, 0xFFFFFFFF);
#endif

    CPluginPacketSkillsList().SendToPlugin();
    CPluginPacketSpellsList().SendToPlugin();
    CPluginPacketMacrosList().SendToPlugin();

    // FIXME: Load any additional plugin here

    std::vector<astr_t> libName;
    std::vector<astr_t> functions;
    std::vector<uint32_t> flags;

    size_t pluginsInfoCount = Crypt::GetPluginsCount();
    if (pluginsInfoCount == 0u)
    {
        return;
    }

    PLUGIN_INFO *pluginsInfo = new PLUGIN_INFO[pluginsInfoCount];
    g_PluginInit(pluginsInfo);
    for (int i = 0; i < (int)pluginsInfoCount; i++)
    {
        libName.push_back(pluginsInfo[i].FileName);
        functions.push_back(pluginsInfo[i].FunctionName);
        flags.push_back((uint32_t)pluginsInfo[i].Flags);
    }
    delete[] pluginsInfo;

    for (int i = 0; i < (int)libName.size(); i++)
    {
        LoadPlugin(g_App.ExeFilePath(libName[i].c_str()), functions[i], flags[i]);
    }

    if (g_PluginManager.m_Items != nullptr)
    {
        CPluginPacketSkillsList().SendToPlugin();
        CPluginPacketSpellsList().SendToPlugin();
        CPluginPacketMacrosList().SendToPlugin();

        // Share memory with assistant
        /*g_FileManager.SendFilesInfo();

		for (auto i = 0; i < 0x10000; i++)
		{
			CIndexObjectStatic &staticObj = m_StaticDataIndex[i];

			if (staticObj.Address)
			{
				uint64_t compressedSize = 0;

				if (staticObj.UopBlock)
					compressedSize = staticObj.UopBlock->CompressedSize;

				CPluginPacketStaticArtGraphicDataInfo(i, staticObj.Address, staticObj.DataSize, compressedSize).SendToPlugin();
			}
		}

		CPluginPacketFilesTransfered().SendToPlugin();*/
    }

    g_GameWindow.Raise();
}

astr_t CGame::FixServerName(astr_t name)
{
    size_t i = 0;
    while ((i = name.find(':')) != astr_t::npos)
    {
        name.erase(i, 1);
    }
    return name;
}

void CGame::LoadLocalConfig(int serial, astr_t characterName)
{
    if (g_ConfigLoaded)
    {
        return;
    }

    g_CheckContainerStackTimer = g_Ticks + 30000;

    char buf[FS_MAX_PATH] = { 0 };
    CServer *server = g_ServerList.GetSelectedServer();
    const astr_t &acct = g_MainScreen.m_Account->GetTextA();
    if (server != nullptr)
    {
        sprintf_s(
            buf, "desktop/%s/%s/0x%08X", acct.c_str(), FixServerName(server->Name).c_str(), serial);
    }
    else
    {
        sprintf_s(buf, "desktop/%s/0x%08X", acct.c_str(), serial);
    }

    if (!g_ConfigManager.Load(g_App.FilePath("%s/%s", buf, "options.cfg")))
    {
        // DEPRECATE: 1.0.6
        if (!g_ConfigManager.Load(g_App.ExeFilePath("%s/%s", buf, "options.cfg")))
        {
            g_ConfigManager.Init();
            if (g_GameState >= GS_GAME)
            {
                g_GameWindow.MaximizeWindow();
            }
        }
    }

    if (!g_SkillGroupManager.Load(g_App.FilePath("skills.xuo")))
    {
        // DEPRECATE: 1.0.6
        if (!g_SkillGroupManager.Load(g_App.ExeFilePath("skills.xuo")))
        {
            g_SkillGroupManager.Load(g_App.ExeFilePath("skills.cuo"));
        }
    }

    auto macros_xuo1 = g_App.FilePath("%s/%s", buf, "macros.xuo");
    // DEPRECATE: 1.0.6
    auto macros_xuo2 = g_App.ExeFilePath("%s/%s", buf, "macros.xuo");
    auto macros_cuo = g_App.ExeFilePath("%s/%s", buf, "macros.cuo");
    auto macros_xuo = fs_path_exists(macros_xuo1) ? macros_xuo1 : macros_xuo2;
    auto macros_data = fs_path_exists(macros_xuo) ? macros_xuo : macros_cuo;
    if (!g_MacroManager.Load(macros_data, g_App.UOFilesPath("macros.txt")))
    {
        char buf2[FS_MAX_PATH] = { 0 };
        if (server != nullptr)
        {
            sprintf_s(
                buf2,
                "desktop/%s/%s/%s",
                acct.c_str(),
                FixServerName(server->Name).c_str(),
                characterName.c_str());
        }
        else
        {
            sprintf_s(buf2, "desktop/%s/%s", acct.c_str(), characterName.c_str());
        }
    }

    const auto gumps_xuo1 = g_App.FilePath("%s/%s", buf, "gumps.xuo");
    // DEPRECATE: 1.0.6
    const auto gumps_xuo2 = g_App.ExeFilePath("%s/%s", buf, "gumps.xuo");
    const auto gumps_cuo = g_App.ExeFilePath("%s/%s", buf, "gumps.cuo");
    const auto gumps_xuo = fs_path_exists(gumps_xuo1) ? gumps_xuo1 : gumps_xuo2;
    const auto gumps_data = fs_path_exists(gumps_xuo) ? gumps_xuo : gumps_cuo;
    g_GumpManager.Load(gumps_data);

    const auto customhouses_xuo1 = g_App.FilePath("%s/%s", buf, "customhouses.xuo");
    // DEPRECATE: 1.0.6
    const auto customhouses_xuo2 = g_App.ExeFilePath("%s/%s", buf, "customhouses.xuo");
    const auto customhouses_cuo = g_App.ExeFilePath("%s/%s", buf, "customhouses.cuo");
    const auto customhouses_xuo =
        fs_path_exists(customhouses_xuo1) ? customhouses_xuo1 : customhouses_xuo2;
    const auto customhouses_data =
        fs_path_exists(customhouses_xuo) ? customhouses_xuo : customhouses_cuo;
    g_CustomHousesManager.Load(customhouses_data);

    if (g_ConfigManager.OffsetInterfaceWindows)
    {
        ContainerPositionReset();
    }

    if (g_ConfigManager.GetConsoleNeedEnter())
    {
        g_EntryPointer = nullptr;
    }
    else
    {
        g_EntryPointer = &g_GameConsole;
    }

    g_SoundManager.SetMusicVolume(g_ConfigManager.GetMusicVolume());
    if (!g_ConfigManager.GetSound())
    {
        AdjustSoundEffects(g_Ticks + 100000);
    }

    if (!g_ConfigManager.GetMusic())
    {
        g_SoundManager.StopMusic();
    }
    g_ConfigLoaded = true;
}

void CGame::SaveLocalConfig(int serial)
{
    if (!g_ConfigLoaded)
    {
        return;
    }
    auto path = g_App.FilePath("desktop");
    if (!fs_path_exists(path))
    {
        Info(Client, "%s Does not exist, creating.", fs_path_ascii(path));
        fs_path_create(path);
    }

    path = fs_path_join(path, g_MainScreen.m_Account->GetTextA());
    if (!fs_path_exists(path))
    {
        Info(Client, "%s Does not exist, creating.", fs_path_ascii(path));
        fs_path_create(path);
    }
    CServer *server = g_ServerList.GetSelectedServer();
    if (server != nullptr)
    {
        path = fs_path_join(path, FixServerName(server->Name));
    }
    if (!fs_path_exists(path))
    {
        Info(Client, "%s Does not exist, creating.", fs_path_ascii(path));
        fs_path_create(path);
    }
    char serbuf[64] = { 0 };
    sprintf_s(serbuf, "0x%08X", g_PlayerSerial);
    fs_path root = path;
    path = fs_path_join(path, serbuf);
    if (!fs_path_exists(path))
    {
        Info(Client, "%s Does not exist, creating.", fs_path_ascii(path));
        fs_path_create(path);
    }
    else
    {
        Info(Client, "SaveLocalConfig using path: %s", fs_path_ascii(path));
    }

    Info(Client, "managers:saving");
    g_ConfigManager.Save(fs_path_join(path, "options.cfg"));
    g_SkillGroupManager.Save(fs_path_join(path, "skills.xuo"));
    g_MacroManager.Save(fs_path_join(path, "macros.xuo"));
    g_GumpManager.Save(fs_path_join(path, "gumps.xuo"));
    g_CustomHousesManager.Save(fs_path_join(path, "customhouses.xuo"));

    if (g_Player != nullptr)
    {
        Info(Client, "player exists");
        Info(Client, "name len: %zd", g_Player->GetName().length());
        path = fs_path_join(root, astr_t(serbuf) + "_" + g_Player->GetName() + ".xuo");
        if (!fs_path_exists(path))
        {
            Info(Client, "file saving");
            FILE *file = fs_open(path, FS_WRITE); // "wb"

            Info(Client, "file closing");
            if (file != nullptr)
            {
                fs_close(file);
            }
        }
    }
    Info(Client, "SaveLocalConfig end");
}

void CGame::ClearUnusedTextures()
{
    if (g_GameState < GS_GAME)
    {
        return;
    }

    g_MapManager.ClearUnusedBlocks();
    g_GumpManager.PrepareTextures();

    g_Ticks -= CLEAR_TEXTURES_DELAY;
    void *lists[] = {
        &m_UsedLandList, &m_UsedStaticList, &m_UsedGumpList, &m_UsedTextureList, &m_UsedLightList
    };

    int counts[] = { MAX_ART_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR,
                     MAX_ART_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR,
                     MAX_GUMP_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR,
                     MAX_ART_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR,
                     100 };

    for (int i = 0; i < countof(lists); i++)
    {
        int count = 0;
        auto *list = (std::deque<CIndexObject *> *)lists[i];
        int &maxCount = counts[i];
        for (auto it = list->begin(); it != list->end();)
        {
            CIndexObject *obj = *it;
            if (obj->LastAccessTime < g_Ticks)
            {
                DestroySprite(obj);
                it = list->erase(it);
                if (++count >= maxCount)
                {
                    break;
                }
            }
            else
            {
                ++it;
            }
        }
    }

    int count = 0;
    for (auto it = m_UsedSoundList.begin(); it != m_UsedSoundList.end();)
    {
        CIndexSound *obj = *it;
        if (obj->LastAccessTime < g_Ticks)
        {
            if (obj->UserData != SOUND_NULL)
            {
                g_SoundManager.UpdateSoundEffect((SoundHandle)obj->UserData, -1);
                obj->UserData = SOUND_NULL;
            }

            it = m_UsedSoundList.erase(it);
            if (++count >= MAX_SOUND_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR)
            {
                break;
            }
        }
        else
        {
            ++it;
        }
    }

    AdjustSoundEffects(g_Ticks);
    g_Ticks += CLEAR_TEXTURES_DELAY;
}

void CGame::Connect()
{
    InitScreen(GS_MAIN_CONNECT);
    Process(true);

    g_ConnectionManager.Disconnect();
    g_ConnectionManager.Init(); //Configure

    astr_t login{};
    int port;
    LoadLogin(login, port);
    if (g_ConnectionManager.Connect(login, port, g_GameSeed))
    {
        g_ConnectionScreen.SetConnected(true);
        CPacketFirstLogin().Send();
    }
    else
    {
        g_ConnectionScreen.SetConnectionFailed(true);
        g_ConnectionScreen.SetErrorCode(8);
    }
}

void CGame::Disconnect()
{
    g_AbyssPacket03First = true;
    g_PluginManager.Disconnect();

    g_ConnectionManager.Disconnect();

    ClearWorld();
}

int CGame::Send(uint8_t *buf, int size)
{
    uint32_t ticks = g_Ticks;
    g_TotalSendSize += size;
    CPacketInfo &type = g_PacketManager.GetInfo(*buf);
    if (type.save)
    {
#if defined(XUO_WINDOWS)
        time_t rawtime;
        struct tm timeinfo;
        char buffer[80];

        time(&rawtime);
        localtime_s(&timeinfo, &rawtime);
        strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", &timeinfo);
        DEBUG(
            Network,
            "--- ^(%d) s(+%d => %d) %s Client:: %s",
            ticks - g_LastPacketTime,
            size,
            g_TotalSendSize,
            buffer,
            type.Name);
#endif

        if (*buf == 0x80 || *buf == 0x91)
        {
            DEBUG_DUMP(Network, "SEND:", buf, 1);
            SAFE_DEBUG_DUMP(Network, "SEND:", buf, size);
            DEBUG(Network, "**** ACCOUNT AND PASSWORD CENSORED ****");
        }
        else
        {
            DEBUG_DUMP(Network, "SEND:", buf, size);
        }
    }

    int result = 0;

    if (type.Direction != DIR_SEND && type.Direction != DIR_BOTH)
    {
        Info(Network, "invalid message direction: 0x%02X", *buf);
    }
    else
    {
        if (g_PluginManager.PacketSend(buf, size))
        {
            result = g_ConnectionManager.Send(buf, size);
        }
    }

    if (result != 0)
    {
        g_LastPacketTime = ticks;
        g_LastSendTime = ticks;
    }

    return result;
}

void CGame::ServerSelection(int pos)
{
    InitScreen(GS_SERVER_CONNECT);
    Process(true);
    CServer *server = g_ServerList.Select(pos);
    if (server != nullptr)
    {
        auto name = g_ServerList.GetSelectedServer()->Name;
        g_ServerList.LastServerName = name;
        static auto sn = FixServerName(name);
        const char *data = sn.c_str();
        PLUGIN_EVENT(UOMSG_SET_SERVER_NAME, data);
        CPacketSelectServer((uint8_t)server->Index).Send();
    }
}

void CGame::RelayServer(const char *ip, int port, uint8_t *gameSeed)
{
    memcpy(&g_GameSeed[0], &gameSeed[0], 4);
    g_ConnectionManager.Init(gameSeed);
    m_GameServerIP = ip;
#if USE_PING
    memset(&g_GameServerPingInfo, 0, sizeof(g_GameServerPingInfo));
#endif // USE_PING
    if (g_ConnectionManager.Connect(ip, port, gameSeed))
    {
        g_ConnectionScreen.SetConnected(true);
        CPacketSecondLogin().Send();
    }
    else
    {
        g_ConnectionScreen.SetConnectionFailed(true);
        g_ConnectionScreen.SetErrorCode(8);
    }
}

void CGame::CharacterSelection(int pos)
{
    InitScreen(GS_GAME_CONNECT);
    g_ConnectionScreen.SetType(CST_GAME);
    g_CharacterList.LastCharacterName = g_CharacterList.GetName(pos);
    PLUGIN_EVENT(UOMSG_SET_PLAYER_NAME, g_CharacterList.LastCharacterName.c_str());
    CPacketSelectCharacter(pos, g_CharacterList.LastCharacterName).Send();
}

void CGame::LoginComplete(bool reload)
{
    bool load = reload;
    if (!load && !g_ConnectionScreen.GetCompleted())
    {
        load = true;
        g_ConnectionScreen.SetCompleted(true);
        InitScreen(GS_GAME);
    }

    if (load && g_Player != nullptr)
    {
        astr_t title = "Ultima Online - " + g_Player->GetName();
        CServer *server = g_ServerList.GetSelectedServer();
        if (server != nullptr)
        {
            title += " (" + server->Name + ")";
        }

        g_GameWindow.SetTitle(title);
        CPacketSkillsRequest(g_PlayerSerial).Send();
        g_UseItemActions.Add(g_PlayerSerial);

        //CPacketOpenChat({}).Send();
        //CPacketRazorAnswer().Send();
        if (g_Config.ProtocolClientVersion >= CV_306E)
        {
            CPacketClientType().Send();
        }

        if (g_Config.ProtocolClientVersion >= CV_305D)
        {
            CPacketClientViewRange(g_ConfigManager.UpdateRange).Send();
        }
        LoadLocalConfig(g_PacketManager.ConfigSerial, g_Player->GetName());
    }
}

void CGame::ChangeSeason(const SEASON_TYPE &season, int music)
{
    g_Season = season;
    QFOR(item, g_MapManager.m_Items, CMapBlock *)
    {
        for (int x = 0; x < 8; x++)
        {
            for (int y = 0; y < 8; y++)
            {
                QFOR(obj, item->GetRender((int)x, (int)y), CRenderWorldObject *)
                {
                    obj->UpdateGraphicBySeason();
                }
            }
        }
    }

    CGumpMinimap *gump = (CGumpMinimap *)g_GumpManager.UpdateGump(0, 0, GT_MINIMAP);

    if (gump != nullptr)
    {
        gump->LastX = 0;
    }

    if (music != 0)
    {
        g_Game.PlayMusic(music, true);
    }
}

uint16_t CGame::GetLandSeasonGraphic(uint16_t graphic)
{
    if (g_Season == ST_WINTER)
    {
        uint16_t buf = m_WinterTile[graphic];

        if (buf != 0)
        {
            graphic = buf;
        }
    }

    return graphic;
}

uint16_t CGame::GetSeasonGraphic(uint16_t graphic)
{
    switch (g_Season)
    {
        case ST_SPRING:
            return GetSpringGraphic(graphic);
        //case ST_SUMMER:
        //	return GetSummerGraphic(graphic);
        case ST_FALL:
            return GetFallGraphic(graphic);
        //case ST_WINTER:
        //	return GetWinterGraphic(graphic);
        case ST_DESOLATION:
            return GetDesolationGraphic(graphic);
        default:
            break;
    }

    return graphic;
}

uint16_t CGame::GetSpringGraphic(uint16_t graphic)
{
    switch (graphic)
    {
        case 0x0CA7:
            graphic = 0x0C84;
            break;
        case 0x0CAC:
            graphic = 0x0C46;
            break;
        case 0x0CAD:
            graphic = 0x0C48;
            break;
        case 0x0CAE:
        case 0x0CB5:
            graphic = 0x0C4A;
            break;
        case 0x0CAF:
            graphic = 0x0C4E;
            break;
        case 0x0CB0:
            graphic = 0x0C4D;
            break;
        case 0x0CB6:
        case 0x0D0D:
        case 0x0D14:
            graphic = 0x0D2B;
            break;
        case 0x0D0C:
            graphic = 0x0D29;
            break;
        case 0x0D0E:
            graphic = 0x0CBE;
            break;
        case 0x0D0F:
            graphic = 0x0CBF;
            break;
        case 0x0D10:
            graphic = 0x0CC0;
            break;
        case 0x0D11:
            graphic = 0x0C87;
            break;
        case 0x0D12:
            graphic = 0x0C38;
            break;
        case 0x0D13:
            graphic = 0x0D2F;
            break;
        default:
            break;
    }

    return graphic;
}

uint16_t CGame::GetSummerGraphic(uint16_t graphic)
{
    return graphic;
}

uint16_t CGame::GetFallGraphic(uint16_t graphic)
{
    switch (graphic)
    {
        case 0x0CD1:
            graphic = 0x0CD2;
            break;
        case 0x0CD4:
            graphic = 0x0CD5;
            break;
        case 0x0CDB:
            graphic = 0x0CDC;
            break;
        case 0x0CDE:
            graphic = 0x0CDF;
            break;
        case 0x0CE1:
            graphic = 0x0CE2;
            break;
        case 0x0CE4:
            graphic = 0x0CE5;
            break;
        case 0x0CE7:
            graphic = 0x0CE8;
            break;
        case 0x0D95:
            graphic = 0x0D97;
            break;
        case 0x0D99:
            graphic = 0x0D9B;
            break;
        case 0x0CCE:
            graphic = 0x0CCF;
            break;
        case 0x0CE9:
        case 0x0C9E:
            graphic = 0x0D3F;
            break;
        case 0x0CEA:
            graphic = 0x0D40;
            break;
        case 0x0C84:
        case 0x0CB0:
            graphic = 0x1B22;
            break;
        case 0x0C8B:
        case 0x0C8C:
        case 0x0C8D:
        case 0x0C8E:
            graphic = 0x0CC6;
            break;
        case 0x0CA7:
            graphic = 0x0C48;
            break;
        case 0x0CAC:
            graphic = 0x1B1F;
            break;
        case 0x0CAD:
            graphic = 0x1B20;
            break;
        case 0x0CAE:
            graphic = 0x1B21;
            break;
        case 0x0CAF:
            graphic = 0x0D0D;
            break;
        case 0x0CB5:
            graphic = 0x0D10;
            break;
        case 0x0CB6:
            graphic = 0x0D2B;
            break;
        case 0x0CC7:
            graphic = 0x0C4E;
            break;
        default:
            break;
    }

    return graphic;
}

uint16_t CGame::GetWinterGraphic(uint16_t graphic)
{
    return graphic;
}

uint16_t CGame::GetDesolationGraphic(uint16_t graphic)
{
    switch (graphic)
    {
        case 0x1B7E:
            graphic = 0x1E34;
            break;
        case 0x0D2B:
            graphic = 0x1B15;
            break;
        case 0x0D11:
        case 0x0D14:
        case 0x0D17:
            graphic = 0x122B;
            break;
        case 0x0D16:
        case 0x0CB9:
        case 0x0CBA:
        case 0x0CBB:
        case 0x0CBC:
        case 0x0CBD:
        case 0x0CBE:
            graphic = 0x1B8D;
            break;
        case 0x0CC7:
            graphic = 0x1B0D;
            break;
        case 0x0CE9:
            graphic = 0x0ED7;
            break;
        case 0x0CEA:
            graphic = 0x0D3F;
            break;
        case 0x0D0F:
            graphic = 0x1B1C;
            break;
        case 0x0CB8:
            graphic = 0x1CEA;
            break;
        case 0x0C84:
        case 0x0C8B:
            graphic = 0x1B84;
            break;
        case 0x0C9E:
            graphic = 0x1182;
            break;
        case 0x0CAD:
            graphic = 0x1AE1;
            break;
        case 0x0C4C:
            graphic = 0x1B16;
            break;
        case 0x0C8E:
        case 0x0C99:
        case 0x0CAC:
            graphic = 0x1B8D;
            break;
        case 0x0C46:
        case 0x0C49:
        case 0x0CB6:
            graphic = 0x1B9D;
            break;
        case 0x0C45:
        case 0x0C48:
        case 0x0C4E:
        case 0x0C85:
        case 0x0CA7:
        case 0x0CAE:
        case 0x0CAF:
        case 0x0CB5:
        case 0x0D15:
        case 0x0D29:
            graphic = 0x1B9C;
            break;
        case 0x0C37:
        case 0x0C38:
        case 0x0C47:
        case 0x0C4A:
        case 0x0C4B:
        case 0x0C4D:
        case 0x0C8C:
        case 0x0C8D:
        case 0x0C93:
        case 0x0C94:
        case 0x0C98:
        case 0x0C9F:
        case 0x0CA0:
        case 0x0CA1:
        case 0x0CA2:
        case 0x0CA3:
        case 0x0CA4:
        case 0x0CB0:
        case 0x0CB1:
        case 0x0CB2:
        case 0x0CB3:
        case 0x0CB4:
        case 0x0CB7:
        case 0x0CC5:
        case 0x0D0C:
        case 0x0D0D:
        case 0x0D0E:
        case 0x0D10:
        case 0x0D12:
        case 0x0D13:
        case 0x0D18:
        case 0x0D19:
        case 0x0D2D:
        case 0x0D2F:
            graphic = 0x1BAE;
            break;
        default:
            break;
    }

    return graphic;
}

int CGame::ValueInt(const VALUE_KEY_INT &key, int value)
{
    switch (key)
    {
        case VKI_SOUND:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetSound());
            }
            else
            {
                g_ConfigManager.SetSound(value != 0);
            }

            break;
        }
        case VKI_SOUND_VALUE:
        {
            if (value == -1)
            {
                value = g_ConfigManager.GetSoundVolume();
            }
            else
            {
                if (value < 0)
                {
                    value = 0;
                }
                else if (value > 255)
                {
                    value = 255;
                }

                g_ConfigManager.SetSoundVolume(value);
            }

            break;
        }
        case VKI_MUSIC:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetMusic());
            }
            else
            {
                g_ConfigManager.SetMusic(value != 0);
            }

            break;
        }
        case VKI_MUSIC_VALUE:
        {
            if (value == -1)
            {
                value = g_ConfigManager.GetMusicVolume();
            }
            else
            {
                if (value < 0)
                {
                    value = 0;
                }
                else if (value > 255)
                {
                    value = 255;
                }

                g_ConfigManager.SetMusicVolume(value);
            }

            break;
        }
        case VKI_USE_TOOLTIPS:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.UseToolTips);
            }
            else
            {
                g_ConfigManager.UseToolTips = (value != 0);
            }

            break;
        }
        case VKI_ALWAYS_RUN:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.AlwaysRun);
            }
            else
            {
                g_ConfigManager.AlwaysRun = (value != 0);
            }

            break;
        }
        case VKI_NEW_TARGET_SYSTEM:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.DisableNewTargetSystem);
            }
            else
            {
                g_ConfigManager.DisableNewTargetSystem =
                    (value == 0); //������ == 0!!! �.�. � ������� ��� Target System enable/disable
            }

            break;
        }
        case VKI_OBJECT_HANDLES:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.ObjectHandles);
            }
            else
            {
                g_ConfigManager.ObjectHandles = (value != 0);
            }

            break;
        }
        case VKI_SCALE_SPEECH_DELAY:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.ScaleSpeechDelay);
            }
            else
            {
                g_ConfigManager.ScaleSpeechDelay = (value != 0);
            }

            break;
        }
        case VKI_SPEECH_DELAY:
        {
            if (value == -1)
            {
                value = g_ConfigManager.SpeechDelay;
            }
            else
            {
                if (value < 0)
                {
                    value = 0;
                }
                else if (value > 999)
                {
                    value = 999;
                }

                g_ConfigManager.SpeechDelay = value;
            }

            break;
        }
        case VKI_IGNORE_GUILD_MESSAGES:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.IgnoreGuildMessage);
            }
            else
            {
                g_ConfigManager.IgnoreGuildMessage = (value != 0);
            }

            break;
        }
        case VKI_IGNORE_ALLIANCE_MESSAGES:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.IgnoreAllianceMessage);
            }
            else
            {
                g_ConfigManager.IgnoreAllianceMessage = (value != 0);
            }

            break;
        }
        case VKI_DARK_NIGHTS:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.DarkNights);
            }
            else
            {
                g_ConfigManager.DarkNights = (value != 0);
            }

            break;
        }
        case VKI_COLORED_LIGHTING:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.ColoredLighting);
            }
            else
            {
                g_ConfigManager.ColoredLighting = (value != 0);
            }

            break;
        }
        case VKI_CRIMINAL_ACTIONS_QUERY:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.CriminalActionsQuery);
            }
            else
            {
                g_ConfigManager.CriminalActionsQuery = (value != 0);
            }

            break;
        }
        case VKI_CIRCLETRANS:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.UseCircleTrans);
            }
            else
            {
                g_ConfigManager.UseCircleTrans = (value != 0);
            }

            break;
        }
        case VKI_CIRCLETRANS_VALUE:
        {
            if (value == -1)
            {
                value = g_ConfigManager.CircleTransRadius;
            }
            else
            {
                if (value < 0)
                {
                    value = 0;
                }
                else if (value > 255)
                {
                    value = 255;
                }

                g_ConfigManager.CircleTransRadius = value;
            }

            break;
        }
        case VKI_LOCK_RESIZING_GAME_WINDOW:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.LockResizingGameWindow);
            }
            else
            {
                g_ConfigManager.LockResizingGameWindow = (value != 0);
            }

            break;
        }
        case VKI_CLIENT_FPS_VALUE:
        {
            if (value == -1)
            {
                value = g_ConfigManager.GetClientFPS();
            }
            else
            {
                if (value < MIN_FPS_LIMIT)
                {
                    value = MIN_FPS_LIMIT;
                }
                else if (value > MAX_FPS_LIMIT)
                {
                    value = MAX_FPS_LIMIT;
                }

                g_ConfigManager.SetClientFPS(value);
            }

            break;
        }
        case VKI_USE_SCALING_GAME_WINDOW:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetUseScaling());
            }
            else
            {
                g_ConfigManager.SetUseScaling((value != 0));
            }

            break;
        }
        case VKI_DRAW_STATUS_STATE:
        {
            if (value == -1)
            {
                value = g_ConfigManager.GetDrawStatusState();
            }
            else
            {
                if (value < 0)
                {
                    value = 0;
                }
                else if (value > DCSS_UNDER)
                {
                    value = DCSS_UNDER;
                }

                g_ConfigManager.SetDrawStatusState(value);
            }

            break;
        }
        case VKI_DRAW_STUMPS:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetDrawStumps());
            }
            else
            {
                g_ConfigManager.SetDrawStumps((value != 0));
            }

            break;
        }
        case VKI_MARKING_CAVES:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetMarkingCaves());
            }
            else
            {
                g_ConfigManager.SetMarkingCaves((value != 0));
            }

            break;
        }
        case VKI_NO_VEGETATION:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetNoVegetation());
            }
            else
            {
                g_ConfigManager.SetNoVegetation((value != 0));
            }

            break;
        }
        case VKI_NO_ANIMATE_FIELDS:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetNoAnimateFields());
            }
            else
            {
                g_ConfigManager.SetNoAnimateFields((value != 0));
            }

            break;
        }
        case VKI_STANDARD_CHARACTERS_DELAY:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.StandartCharactersAnimationDelay);
            }
            else
            {
                g_ConfigManager.StandartCharactersAnimationDelay = (value != 0);
            }

            break;
        }
        case VKI_STANDARD_ITEMS_DELAY:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.StandartItemsAnimationDelay);
            }
            else
            {
                g_ConfigManager.StandartItemsAnimationDelay = (value != 0);
            }

            break;
        }
        case VKI_LOCK_GUMPS_MOVING:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.LockGumpsMoving);
            }
            else
            {
                g_ConfigManager.LockGumpsMoving = (value != 0);
            }

            break;
        }
        case VKI_CONSOLE_NEED_ENTER:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.GetConsoleNeedEnter());
            }
            else
            {
                g_ConfigManager.SetConsoleNeedEnter(value != 0);
            }

            break;
        }
        case VKI_HIDDEN_CHARACTERS_MODE:
        {
            if (value == -1)
            {
                value = g_ConfigManager.HiddenCharactersRenderMode;
            }
            else
            {
                if (value < 0)
                {
                    value = 0;
                }
                else if (value > HCRM_SPECIAL_SPECTRAL_COLOR)
                {
                    value = HCRM_SPECIAL_SPECTRAL_COLOR;
                }

                g_ConfigManager.HiddenCharactersRenderMode = value;
            }

            break;
        }
        case VKI_HIDDEN_CHARACTERS_ALPHA:
        {
            if (value == -1)
            {
                value = g_ConfigManager.HiddenAlpha;
            }
            else
            {
                if (value < 20)
                {
                    value = 20;
                }
                else if (value > 255)
                {
                    value = 255;
                }

                g_ConfigManager.HiddenAlpha = value;
            }

            break;
        }
        case VKI_HIDDEN_CHARACTERS_MODE_ONLY_FOR_SELF:
        {
            if (value == -1)
            {
                value = static_cast<int>(g_ConfigManager.UseHiddenModeOnlyForSelf);
            }
            else
            {
                g_ConfigManager.UseHiddenModeOnlyForSelf = (value != 0);
            }

            break;
        }
        case VKI_TRANSPARENT_SPELL_ICONS:
        {
            if (value == -1)
            {
                value = static_cast<uint8_t>(g_ConfigManager.TransparentSpellIcons);
            }
            else
            {
                g_ConfigManager.TransparentSpellIcons = static_cast<bool>(value != 0);
            }

            break;
        }
        case VKI_SPELL_ICONS_ALPHA:
        {
            if (value == -1)
            {
                value = g_ConfigManager.GetSpellIconAlpha();
            }
            else
            {
                if (value < 30)
                {
                    value = 30;
                }
                else if (value > 255)
                {
                    value = 255;
                }

                g_ConfigManager.SetSpellIconAlpha(value);
            }

            break;
        }
        case VKI_SKILLS_COUNT:
        {
            value = g_SkillsManager.Count;
            break;
        }
        case VKI_SKILL_CAN_BE_USED:
        {
            CSkill *skill = g_SkillsManager.Get(value);

            if (skill != nullptr)
            {
                value = static_cast<int>(skill->Button);
            }

            break;
        }
        case VKI_STATIC_ART_ADDRESS:
        {
            if (value >= 0 && value < (int)g_Data.m_Static.size())
            {
                value = (int)g_Index.m_Static[value].Address;
            }

            break;
        }
        case VKI_USED_LAYER:
        {
            if (value >= 0 && value < (int)g_Data.m_Static.size())
            {
                value = g_Data.m_Static[value].Layer;
            }

            break;
        }
        case VKI_SPELLBOOK_COUNT:
        {
            if (value >= ST_FIRST && value < ST_COUNT)
            {
                switch (value)
                {
                    case 1:
                    {
                        value = int(SpellCount::Magery);
                        break;
                    }
                    case 2:
                    {
                        value = int(SpellCount::Necromancy);
                        break;
                    }
                    case 3:
                    {
                        value = int(SpellCount::Chivalry);
                        break;
                    }
                    case 4:
                    {
                        value = int(SpellCount::Bushido);
                        break;
                    }
                    case 5:
                    {
                        value = int(SpellCount::Ninjitsu);
                        break;
                    }
                    case 6:
                    {
                        value = int(SpellCount::Spellweaving);
                        break;
                    }
                    case 7:
                    {
                        value = int(SpellCount::Mysticism);
                        break;
                    }
                    case 8:
                    {
                        value = int(SpellCount::Mastery);
                        break;
                    }
                    default:
                        assert(false && "unreachable");
                        break;
                }
            }

            break;
        }
        case VKI_BLOCK_MOVING:
        {
            g_PathFinder.BlockMoving = (value != 0);

            break;
        }
        case VKI_SET_PLAYER_GRAPHIC:
        {
            if (g_Player != nullptr && g_Player->Graphic != value)
            {
                g_Player->Graphic = value;
                g_Player->OnGraphicChange(1000);
            }

            break;
        }
        case VKI_FAST_ROTATION:
        {
            g_PathFinder.FastRotation = (value != 0);

            break;
        }
        case VKI_IGNORE_STAMINA_CHECK:
        {
            g_PathFinder.IgnoreStaminaCheck = (value != 0);

            break;
        }
        case VKI_LAST_TARGET:
        {
            if (value == -1)
            {
                value = g_LastTargetObject;
            }
            else
            {
                g_LastTargetObject = value;
                g_Target.SetLastTargetObject(g_LastTargetObject);
            }

            break;
        }
        case VKI_LAST_ATTACK:
        {
            if (value == -1)
            {
                value = g_LastAttackObject;
            }
            else
            {
                g_LastAttackObject = value;
            }

            break;
        }
        case VKI_NEW_TARGET_SYSTEM_SERIAL:
        {
            if (value == -1)
            {
                value = g_NewTargetSystem.Serial;
            }
            else
            {
                g_NewTargetSystem.Serial = value;
            }

            break;
        }
        case VKI_GET_MAP_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (g_MapSize[value].Width << 16) | g_MapSize[value].Height;
            }

            break;
        }
        case VKI_GET_MAP_BLOCK_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (g_MapBlockSize[value].Width << 16) | g_MapBlockSize[value].Height;
            }

            break;
        }
        case VKI_MAP_MUL_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_MapMul[value].Start);
            }

            break;
        }
        case VKI_MAP_MUL_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_MapMul[value].Size;
            }

            break;
        }
        case VKI_STATIC_IDX_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_StaticIdx[value].Start);
            }

            break;
        }
        case VKI_STATIC_IDX_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_StaticIdx[value].Size;
            }

            break;
        }
        case VKI_STATIC_MUL_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_StaticMul[value].Start);
            }

            break;
        }
        case VKI_STATIC_MUL_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_StaticMul[value].Size;
            }

            break;
        }
        case VKI_MAP_DIFL_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_MapDifl[value].Start);
            }

            break;
        }
        case VKI_MAP_DIFL_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_MapDifl[value].Size;
            }

            break;
        }
        case VKI_MAP_DIF_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_MapDif[value].Start);
            }

            break;
        }
        case VKI_MAP_DIF_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_MapDif[value].Size;
            }

            break;
        }
        case VKI_STATIC_DIFL_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_StaDifl[value].Start);
            }

            break;
        }
        case VKI_STATIC_DIFL_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_StaDifl[value].Size;
            }

            break;
        }
        case VKI_STATIC_DIFI_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_StaDifi[value].Start);
            }

            break;
        }
        case VKI_STATIC_DIFI_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_StaDifi[value].Size;
            }

            break;
        }
        case VKI_STATIC_DIF_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_StaDif[value].Start);
            }

            break;
        }
        case VKI_STATIC_DIF_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_StaDif[value].Size;
            }

            break;
        }
        case VKI_VERDATA_ADDRESS:
        {
            value = checked_cast<int>(g_FileManager.m_VerdataMul.Start);

            break;
        }
        case VKI_VERDATA_SIZE:
        {
            value = (int)g_FileManager.m_VerdataMul.Size;

            break;
        }
        case VKI_MAP_UOP_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_MapUOP[value].Start);
            }

            break;
        }
        case VKI_MAP_UOP_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_MapUOP[value].Size;
            }

            break;
        }
        case VKI_MAP_X_UOP_ADDRESS:
        {
            if (value >= 0 && value < 6)
            {
                value = checked_cast<int>(g_FileManager.m_MapXUOP[value].Start);
            }

            break;
        }
        case VKI_MAP_X_UOP_SIZE:
        {
            if (value >= 0 && value < 6)
            {
                value = (int)g_FileManager.m_MapXUOP[value].Size;
            }

            break;
        }
        case VKI_CLILOC_ENU_ADDRESS:
        {
            value = checked_cast<int>(g_ClilocManager.Cliloc("enu")->m_File.Start);

            break;
        }
        case VKI_CLILOC_ENU_SIZE:
        {
            value = (int)g_ClilocManager.Cliloc("enu")->m_File.Size;

            break;
        }
        case VKI_GUMP_ART_ADDRESS:
        {
            if (value >= 0 && value < MAX_GUMP_DATA_INDEX_COUNT)
            {
                value = checked_cast<int>(g_Index.m_Gump[value].Address);
            }

            break;
        }
        case VKI_VIEW_RANGE:
        {
            value = g_ConfigManager.UpdateRange;

            break;
        }
        case VKI_SET_PVPCALLER:
        {
            CGameCharacter *obj = g_World->FindWorldCharacter(value);
            if (obj == nullptr)
            {
                break;
            }

            if (!obj->pvpCaller)
            {
                obj->pvpCaller = true;
            }
            else
            {
                obj->pvpCaller = false;
            }

            break;
        }
        default:
            break;
    }

    return value;
}

astr_t CGame::ValueString(const VALUE_KEY_STRING &key, astr_t value)
{
    switch (key)
    {
        case VKS_SKILL_NAME:
        {
            int index = str_to_int(value);

            CSkill *skill = g_SkillsManager.Get(index);

            if (skill != nullptr)
            {
                value = skill->Name;
            }

            break;
        }
        case VKS_SERVER_NAME:
        {
            CServer *server = g_ServerList.GetSelectedServer();

            if (server != nullptr)
            {
                value = server->Name;
            }

            break;
        }
        case VKS_CHARACTER_NAME:
        {
            if (g_Player != nullptr)
            {
                value = g_Player->GetName();
            }

            break;
        }
        case VKS_SPELLBOOK_1_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Magery))
            {
                value = GetSpellByOffsetAndType(index, ST_MAGERY)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_2_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Necromancy))
            {
                value = GetSpellByOffsetAndType(index, ST_NECROMANCY)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_3_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Chivalry))
            {
                value = GetSpellByOffsetAndType(index, ST_CHIVALRY)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_4_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Bushido))
            {
                value = GetSpellByOffsetAndType(index, ST_BUSHIDO)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_5_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Ninjitsu))
            {
                value = GetSpellByOffsetAndType(index, ST_NINJITSU)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_6_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Spellweaving))
            {
                value = GetSpellByOffsetAndType(index, ST_SPELLWEAVING)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_7_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Mysticism))
            {
                value = GetSpellByOffsetAndType(index, ST_MYSTICISM)->Name;
            }
            break;
        }
        case VKS_SPELLBOOK_8_SPELL_NAME:
        {
            int index = str_to_int(value);
            if (index >= 0 && index < int(SpellCount::Mastery))
            {
                value = GetSpellByOffsetAndType(index, ST_MASTERY)->Name;
            }
            break;
        }
        default:
            break;
    }

    return value;
}

void CGame::ClearRemovedStaticsTextures()
{
    for (auto it = m_UsedStaticList.begin(); it != m_UsedStaticList.end();)
    {
        CIndexObject *obj = *it;
        if (obj->LastAccessTime == 0u)
        {
            DestroySprite(obj);
            it = m_UsedStaticList.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CGame::ClearTreesTextures()
{
    for (uint16_t graphic : g_Data.m_StumpTiles)
    {
        g_Index.m_Static[graphic].LastAccessTime = 0;
    }
    ClearRemovedStaticsTextures();
}

bool CGame::InTileFilter(uint16_t graphic)
{
    if (m_IgnoreInFilterTiles.empty())
    {
        return false;
    }
    for (const auto &i : m_IgnoreInFilterTiles)
    {
        if (i.first == graphic || ((i.second != 0u) && IN_RANGE(graphic, i.first, i.second)))
        {
            return true;
        }
    }
    return false;
}

bool CGame::IsTreeTile(uint16_t graphic, int &index)
{
    if (!g_ConfigManager.GetDrawStumps() || InTileFilter(graphic))
    {
        return false;
    }

    uint8_t flags = m_StaticTilesFilterFlags[graphic];

    if ((flags & STFF_STUMP) != 0)
    {
        if ((flags & STFF_STUMP_HATCHED) != 0)
        {
            index = g_StumpHatchedID;
        }
        else
        {
            index = g_StumpID;
        }

        return true;
    }

    return false;
}

void CGame::ClearCaveTextures()
{
    for (uint16_t graphic : g_Data.m_CaveTiles)
    {
        g_Index.m_Static[graphic].LastAccessTime = 0;
    }
    ClearRemovedStaticsTextures();
}

bool CGame::IsCaveTile(uint16_t graphic)
{
    return (
        g_ConfigManager.GetMarkingCaves() &&
        ((m_StaticTilesFilterFlags[graphic] & STFF_CAVE) != 0));
}

bool CGame::IsVegetation(uint16_t graphic)
{
    return (m_StaticTilesFilterFlags[graphic] & STFF_VEGETATION) != 0;
}

void CGame::LoadLogin(astr_t &login, int &port)
{
    login = m_OverrideServerAddress;
    port = m_OverrideServerPort;
    if (m_OverrideServerPort != 0)
    {
        return;
    }

    if (!g_Config.ServerAddress.empty())
    {
        login = g_Config.ServerAddress;
        port = g_Config.ServerPort;
        return;
    }

    TextFileParser file(g_App.UOFilesPath("login.cfg"), "=,", "#;", "");
    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens();
        if (strings.size() >= 3)
        {
            auto lo = str_lower(strings[0]);
            if (lo == "loginserver")
            {
                login = strings[1];
                port = str_to_int(strings[2]);
            }
        }
    }
}

void CGame::GoToWebLink(const astr_t &url)
{
    if (url.length() != 0u)
    {
        std::size_t found = url.find("http://");
        if (found == astr_t::npos)
        {
            found = url.find("https://");
        }
        const astr_t header = "http://";
        if (found != astr_t::npos)
        {
            Platform::OpenBrowser(url.c_str());
        }
        else
        {
            Platform::OpenBrowser((header + url).c_str());
        }
    }
}

static uint16_t CalculateLightColor(uint16_t id)
{
    uint16_t color = 0;
    if (id < 0x3E27)
    {
        //color = ???;
    }
    else
    {
        color = 666;

        //if (id > 0x3E3A)
        //	color = ???;
    }

    switch (id)
    {
        case 0x088C:
            color = 31;
            break;
        case 0x0FAC:
            color = 30;
            break;
        case 0x0FB1:
            color = 60;
            break;
        case 0x1647:
            color = 61;
            break;
        case 0x19BB:
        case 0x1F2B:
            color = 40;
            break;
        default:
            break;
    };

    if (id < 0x09FB || id > 0x0A14)
    {
        if (id < 0x0A15 || id > 0x0A29)
        {
            if (id < 0x0B1A || id > 0x0B1F)
            {
                if (id < 0x0B20 || id > 0x0B25)
                {
                    if (id < 0x0B26 || id > 0x0B28)
                    {
                        if (id < 0x0DE1 || id > 0x0DEA)
                        {
                            if (id < 0x1849 || id > 0x1850)
                            {
                                if (id < 0x1853 || id > 0x185A)
                                {
                                    if (id < 0x197A || id > 0x19A9)
                                    {
                                        if (id < 0x19AB || id > 0x19B6)
                                        {
                                            if ((id >= 0x1ECD && id <= 0x1ECF) ||
                                                (id >= 0x1ED0 && id <= 0x1ED2))
                                            {
                                                color = 1;
                                            }
                                        }
                                        else
                                        {
                                            color = 60;
                                        }
                                    }
                                    else
                                    {
                                        color = 60;
                                    }
                                }
                                else
                                {
                                    color = 61;
                                }
                            }
                            else
                            {
                                color = 61;
                            }
                        }
                        else
                        {
                            color = 31;
                        }
                    }
                    else
                    {
                        color = 666;
                    }
                }
                else
                {
                    color = 666;
                }
            }
            else
            {
                color = 666;
            }
        }
        else
        {
            color = 666;
        }
    }
    else
    {
        color = 30;
    }

    if (id == 0x1FD4 || id == 0x0F6C)
    {
        color = 2;
    }

    if (id < 0x0E2D || id > 0x0E30)
    {
        if (id < 0x0E31 || id > 0x0E33)
        {
            if (id < 0x0E5C || id > 0x0E6A)
            {
                if (id < 0x12EE || id > 0x134D)
                {
                    if (id < 0x306A || id > 0x329B)
                    {
                        if (id < 0x343B || id > 0x346C)
                        {
                            if (id < 0x3547 || id > 0x354C)
                            {
                                if (id < 0x3914 || id > 0x3929)
                                {
                                    if (id < 0x3946 || id > 0x3964)
                                    {
                                        if (id < 0x3967 || id > 0x397A)
                                        {
                                            if (id < 0x398C || id > 0x399F)
                                            {
                                                if (id < 0x3E02 || id > 0x3E0B)
                                                {
                                                    if (id < 0x3E27 || id > 0x3E3A)
                                                    {
                                                        switch (id)
                                                        {
                                                            case 0x40FE:
                                                                color = 40;
                                                                break;
                                                            case 0x40FF:
                                                                color = 10;
                                                                break;
                                                            case 0x4100:
                                                                color = 20;
                                                                break;
                                                            case 0x4101:
                                                                color = 32;
                                                                break;
                                                            default:
                                                                if ((id >= 0x983B &&
                                                                     id <= 0x983D) ||
                                                                    (id >= 0x983F && id <= 0x9841))
                                                                {
                                                                    color = 30;
                                                                }
                                                                break;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        color = 31;
                                                    }
                                                }
                                                else
                                                {
                                                    color = 1;
                                                }
                                            }
                                            else
                                            {
                                                color = 31;
                                            }
                                        }
                                        else
                                        {
                                            color = 6;
                                        }
                                    }
                                    else
                                    {
                                        color = 6;
                                    }
                                }
                                else
                                {
                                    color = 1;
                                }
                            }
                            else
                            {
                                color = 31;
                            }
                        }
                        else
                        {
                            color = 31;
                        }
                    }
                    else
                    {
                        color = 31;
                    }
                }
                else
                {
                    color = 31;
                }
            }
            else
            {
                color = 6;
            }
        }
        else
        {
            color = 40;
        }
    }
    else
    {
        color = 62;
    }

    /*uint16_t color = m_StaticData[id].Hue;

	if (!color)
	{
		switch (id)
		{
			case 0x0B1A:
			case 0x0B1B:
			case 0x0B1C:
			case 0x0B1D:
			case 0x0B1E:
			case 0x0B1F:
			case 0x0B20:
			case 0x0B21:
			case 0x0B22:
			case 0x0B23:
			case 0x0B24:
			case 0x0B25:
			case 0x0B26:
			case 0x0B27:
			case 0x0B28:
			{
				color = 0x029A;
				break;
			}
			case 0x0E2D:
			case 0x0E2E:
			case 0x0E2F:
			case 0x0E30:
			{
				color = 0x003E;
				break;
			}
			case 0x088C:
			{
				color = 0x001F;
				break;
			}
			//fire pit
			case 0x0FAC:
			{
				color = 0x001E;
				break;
			}
			//forge
			case 0x0FB1:
			{
				color = 0x003C;
				break;
			}
			case 0x1647:
			{
				color = 0x003D;
				break;
			}
			//blue moongate
			case 0x0F6C:
			//moongate
			case 0x1FD4:
			{
				color = 0x0002;
				break;
			}
			//brazier
			case 0x0E31:
			case 0x0E32:
			case 0x0E33:
			case 0x19BB:
			case 0x1F2B:
			{
				color = 0x0028;
				break;
			}
			//lava
			case 0x3547:
			case 0x3548:
			case 0x3549:
			case 0x354A:
			case 0x354B:
			case 0x354C:
			{
				color = 0x001F;
				break;
			}
			default:
				break;
		}

		if (!color)
		{
			//wall sconce
			if (IN_RANGE(id, 0x09FB, 0x0A14))
				color = 0x001E;
			//lava
			else if (IN_RANGE(id, 0x12EE, 0x134D) || IN_RANGE(id, 0x306A, 0x329B) || IN_RANGE(id, 0x343B, 0x346C))
				color = 0x001F;
			//?
			else if (IN_RANGE(id, 0x3E27, 0x3E3A))
				color = 0x001F;
			//?
			else if (IN_RANGE(id, 0x3E02, 0x3E0B))
				color = 0x0001;
			//fire field
			else if (IN_RANGE(id, 0x398C, 0x399F))
				color = 0x001F;
			//paralyze field
			else if (IN_RANGE(id, 0x3967, 0x397A))
				color = 0x0006;
			//energy field
			else if (IN_RANGE(id, 0x3946, 0x3964))
				color = 0x0006;
			//poison field
			else if (IN_RANGE(id, 0x3914, 0x3929))
				color = 0x0001;
			//glowing runes
			else if (IN_RANGE(id, 0x0E5C, 0x0E6A))
				color = 0x0006;
			//?
			else if (IN_RANGE(id, 0x3E02, 0x3E0B))
				color = 0x0002;
			else if (IN_RANGE(id, 0x3E27, 0x3E3A) || IN_RANGE(id, 0x398C, 0x399F))
				color = 0x029A;
			else if (IN_RANGE(id, 0x1ECD, 0x1ECF) || IN_RANGE(id, 0x1ED0, 0x1ED2))
				color = 0x0001;
			else if (IN_RANGE(id, 0x19AB, 0x19B6) || IN_RANGE(id, 0x197A, 0x19A9))
				color = 0x003C;
			else if (IN_RANGE(id, 0x1853, 0x185A) || IN_RANGE(id, 0x1849, 0x1850))
				color = 0x003D;
			else if (IN_RANGE(id, 0x0DE1, 0x0DEA))
				color = 0x001F;
			else if (IN_RANGE(id, 0x0A15, 0x0A29))
				color = 0x029A;
			else if (IN_RANGE(id, 0x09FB, 0x0A14))
				color = 0x001E;
		}
	}*/
    return color;
}

void CGame::InitStaticAnimList()
{
    if (g_Data.m_Anim.empty())
    {
        return;
    }
    const uintptr_t lastElement =
        (uintptr_t)(&g_Data.m_Anim[0] + g_Data.m_Anim.size() - sizeof(ANIM_DATA));
    for (int i = 0; i < (int)g_Data.m_Static.size(); i++)
    {
        g_Index.m_Static[i].Index = (uint16_t)i;
        g_Index.m_Static[i].LightColor = CalculateLightColor((uint16_t)i);

        bool isField = false;
        //fire field
        if (IN_RANGE(i, 0x398C, 0x399F))
        {
            isField = true;
            //paralyze field
        }
        else if (IN_RANGE(i, 0x3967, 0x397A))
        {
            isField = true;
            //energy field
        }
        else if (IN_RANGE(i, 0x3946, 0x3964))
        {
            isField = true;
            //poison field
        }
        else if (IN_RANGE(i, 0x3914, 0x3929))
        {
            isField = true;
        }

        g_Index.m_Static[i].IsFiled = isField;
        if (IsAnimated(g_Data.m_Static[i].Flags))
        {
            const uintptr_t addr = (uintptr_t)((i * 68) + 4 * ((i / 8) + 1));
            const uintptr_t offset = (uintptr_t)(&g_Data.m_Anim[0] + addr);
            if (offset <= lastElement)
            {
                m_StaticAnimList.push_back(&g_Index.m_Static[i]);
            }
        }
    }
}

void CGame::ProcessStaticAnimList()
{
    if (g_Data.m_Anim.empty() || g_ProcessStaticAnimationTimer >= g_Ticks)
    {
        return;
    }

    int delay =
        (g_ConfigManager.StandartItemsAnimationDelay ? ORIGINAL_ITEMS_ANIMATION_DELAY :
                                                       XUO_ITEMS_ANIMATION_DELAY);
    bool noAnimateFields = g_ConfigManager.GetNoAnimateFields();
    uint32_t nextTime = g_Ticks + 500;

    for (auto it = m_StaticAnimList.begin(); it != m_StaticAnimList.end(); ++it)
    {
        CIndexObjectStatic &obj = *(*it);
        if (noAnimateFields && obj.IsFiled)
        {
            obj.AnimIndex = 0;
            continue;
        }

        if (obj.ChangeTime < g_Ticks)
        {
            uint32_t addr = (obj.Index * 68) + 4 * ((obj.Index / 8) + 1);
            ANIM_DATA &pad = *(ANIM_DATA *)(&g_Data.m_Anim[0] + addr);
            int offset = obj.AnimIndex;
            if (pad.FrameInterval > 0)
            {
                obj.ChangeTime = g_Ticks + (pad.FrameInterval * delay);
            }
            else
            {
                obj.ChangeTime = g_Ticks + delay;
            }

            if (offset < pad.FrameCount)
            {
                obj.Offset = pad.FrameData[offset++];
            }

            if (offset >= pad.FrameCount)
            {
                offset = 0;
            }
            obj.AnimIndex = offset;
        }

        if (obj.ChangeTime < nextTime)
        {
            nextTime = obj.ChangeTime;
        }
    }
    g_ProcessStaticAnimationTimer = nextTime;
}

void CGame::CreateAuraTexture()
{
    int16_t width = 0, height = 0;
    auto pixels = CreateCircleSprite(30, width, height);
    for (int i = 0; i < (int)pixels.size(); i++)
    {
        uint32_t &pixel = pixels[i];
        if (pixel != 0u)
        {
            uint16_t value = pixel << 3;
            if (value > 0xFF)
            {
                value = 0xFF;
            }
            pixel = (value << 24) | (value << 16) | (value << 8) | value;
        }
    }
#ifndef NEW_RENDERER_ENABLED
    g_GL.BindTexture32(g_AuraTexture, width, height, pixels.data());
#else
    g_AuraTexture.Width = width;
    g_AuraTexture.Height = height;
    g_AuraTexture.Texture = Render_CreateTexture2D(
        width,
        height,
        TextureGPUFormat::TextureGPUFormat_RGBA4,
        pixels.data(),
        TextureFormat::TextureFormat_Unsigned_RGBA8);
    assert(g_AuraTexture.Texture != RENDER_TEXTUREHANDLE_INVALID);
#endif
}

void CGame::CreateObjectHandlesBackground()
{
    CSprite *th[9] = { nullptr };
    uint16_t gumpID[9] = { 0 };
    for (int i = 0; i < 9; i++)
    {
        auto pth = ExecuteGump(0x24EA + (uint16_t)i);
        if (pth == nullptr)
        {
            Error(Client, "failed to create Object Handles background data");
            return;
        }

        if (i == 4)
        {
            th[8] = pth;
            gumpID[8] = 0x24EA + (uint16_t)i;
        }
        else if (i > 4)
        {
            th[i - 1] = pth;
            gumpID[i - 1] = 0x24EA + (uint16_t)i;
        }
        else
        {
            th[i] = pth;
            gumpID[i] = 0x24EA + (uint16_t)i;
        }
    }

    for (int i = 0; i < 8; i++)
    {
        if (i == 3 || i == 4)
        {
            continue;
        }

        CIndexObject &io = g_Index.m_Gump[gumpID[i]];
        int drawWidth = io.Width;
        int drawHeight = io.Height;
        int drawX = 0;
        int drawY = 0;

        switch (i)
        {
            case 1:
            {
                drawX = th[0]->Width;
                drawWidth = g_ObjectHandlesWidth - th[0]->Width - th[2]->Width;
                break;
            }
            case 2:
            {
                drawX = g_ObjectHandlesWidth - drawWidth;
                break;
            }
            case 3:
            {
                drawY = th[0]->Height;
                drawHeight = g_ObjectHandlesHeight - th[0]->Height - th[5]->Height;
                break;
            }
            case 4:
            {
                drawX = g_ObjectHandlesWidth - drawWidth;
                drawY = th[2]->Height;
                drawHeight = g_ObjectHandlesHeight - th[2]->Height - th[7]->Height;
                break;
            }
            case 5:
            {
                drawY = g_ObjectHandlesHeight - drawHeight;
                break;
            }
            case 6:
            {
                drawX = th[5]->Width;
                drawY = g_ObjectHandlesHeight - drawHeight;
                drawWidth = g_ObjectHandlesWidth - th[5]->Width - th[7]->Width;
                break;
            }
            case 7:
            {
                drawX = g_ObjectHandlesWidth - drawWidth;
                drawY = g_ObjectHandlesHeight - drawHeight;
                break;
            }
            case 8:
            {
                drawX = th[0]->Width;
                drawY = th[0]->Height;
                drawWidth = g_ObjectHandlesWidth - th[0]->Width - th[2]->Width;
                drawHeight = g_ObjectHandlesHeight - th[2]->Height - th[7]->Height;
                break;
            }
            default:
                break;
        }

        if (drawX < 0)
        {
            drawX = 0;
        }

        if (drawY < 0)
        {
            drawY = 0;
        }

        drawWidth += drawX;
        if (drawWidth > g_ObjectHandlesWidth)
        {
            drawWidth = g_ObjectHandlesWidth - drawX;
        }

        drawHeight += drawY;
        if (drawHeight > g_ObjectHandlesHeight)
        {
            drawHeight = g_ObjectHandlesHeight - drawY;
        }

        std::vector<uint16_t> pixels = g_UOFileReader.GetGumpPixels(io);
        if (!pixels.empty())
        {
            int gumpWidth = io.Width;
            int gumpHeight = io.Height;
            int srcX = 0;
            for (int x = drawX; x < drawWidth; x++)
            {
                int srcY = 0;
                for (int y = drawY; y < drawHeight; y++)
                {
                    uint16_t &pixel =
                        g_ObjectHandlesBackgroundPixels[(y * g_ObjectHandlesWidth) + x];

                    if (pixel == 0)
                    {
                        pixel = pixels[((srcY % gumpHeight) * gumpWidth) + (srcX % gumpWidth)];
                    }
                    srcY++;
                }
                srcX++;
            }
        }
    }
}

void CGame::LoadShaders()
{
#if UO_USE_SHADER_FILES == 1
    CMappedFile frag;
    CMappedFile vert;

    if (vert.Load(g_App.FilePath("shaders/Shader.vert")))
    {
        frag.Load(g_App.FilePath("shaders/DeathShader.frag"));
        g_DeathShader.Init((char *)vert.Start, (char *)frag.Start, 0);
        frag.Unload();
        frag.Load(g_App.FilePath("shaders/ColorizerShader.frag"));
        g_ColorizerShader.Init(1, (char *)vert.Start, (char *)frag.Start, 1);
        frag.Unload();
        frag.Load(g_App.FilePath("shaders/FontColorizerShader.frag"));
        g_FontColorizerShader.Init(1, (char *)vert.Start, (char *)frag.Start, 1);
        frag.Unload();
        frag.Load(g_App.FilePath("shaders/LightColorizerShader.frag"));
        g_LightColorizerShader.Init((char *)vert.Start, (char *)frag.Start, 1);
        frag.Unload();
        vert.Unload();
    }
#else
    g_DeathShader.Init(g_Vert_ShaderData, g_Frag_DeathShaderData, 0);
    g_ColorizerShader.Init(g_Vert_ShaderData, g_Frag_ColorizerShaderData, 1);
    g_FontColorizerShader.Init(g_Vert_ShaderData, g_Frag_FontShaderData, 1);
    g_LightColorizerShader.Init(g_Vert_ShaderData, g_Frag_LightShaderData, 1);
#endif
}

void CGame::LoadClientStartupConfig()
{
    if (!g_ConfigManager.Load(g_App.FilePath("options.cfg")))
    {
        // DEPRECATE: 1.0.6
        if (!g_ConfigManager.Load(g_App.ExeFilePath("options.cfg")))
        {
            g_ConfigManager.Load(g_App.UOFilesPath("options.cfg"));
        }
    }

    g_SoundManager.SetMusicVolume(g_ConfigManager.GetMusicVolume());
    if (g_ConfigManager.GetMusic())
    {
        if (g_Config.ClientVersion >= CV_7000)
        {
            PlayMusic(78);
        }
        else if (g_Config.ClientVersion > CV_308Z)
        { //from 4.x the music played is 0, the first one
            PlayMusic(0);
        }
        else
        {
            PlayMusic(8);
        }
    }
}

void CGame::PlayMusic(int index, bool warmode)
{
    if (!g_ConfigManager.GetMusic() || index >= MAX_MUSIC_DATA_INDEX_COUNT)
    {
        return;
    }
    if (!warmode && g_SoundManager.CurrentMusicIndex == index &&
        g_SoundManager.IsPlayingNormalMusic())
    {
        return;
    }

    if (g_Config.ClientVersion >= CV_306E)
    {
        CIndexMusic &mp3Info = g_Index.m_MP3[index];
        g_SoundManager.PlayMP3(mp3Info.FilePath.c_str(), index, mp3Info.Loop, warmode);
    }
    else
    {
        g_SoundManager.PlayMidi(index, warmode);
    }
}

void CGame::PlaySoundEffectAtPosition(uint16_t id, int x, int y)
{
    auto distance = GetDistance(g_Player, CPoint2Di(x, y));
    g_Game.PlaySoundEffect(id, g_SoundManager.GetVolumeValue(distance));
}

void CGame::PlaySoundEffect(uint16_t id, float volume)
{
    if (id >= 0x0800 || !g_ConfigManager.GetSound())
    {
        return;
    }

    CIndexSound &is = g_Index.m_Sound[id];
    if (is.Address == 0)
    {
        return;
    }

    if (is.UserData == SOUND_NULL)
    {
        is.UserData = g_SoundManager.LoadSoundEffect(is);
        if (is.UserData == SOUND_NULL)
        {
            return;
        }
        m_UsedSoundList.push_back(&g_Index.m_Sound[id]);
    }
    else
    {
        if (is.LastAccessTime + is.Delay > g_Ticks)
        {
            return;
        }
        g_SoundManager.FreeSound((SoundHandle)is.UserData);
        is.UserData = g_SoundManager.LoadSoundEffect(is);
    }

    if (volume <= 0)
    {
        volume = g_SoundManager.GetVolumeValue();
    }

    if (volume > 0)
    {
        g_SoundManager.PlaySoundEffect((SoundHandle)is.UserData, volume);
        is.LastAccessTime = g_Ticks;
    }
}

void CGame::AdjustSoundEffects(int ticks, float volume)
{
    for (auto it = m_UsedSoundList.begin(); it != m_UsedSoundList.end();)
    {
        CIndexSound *obj = *it;
        if (obj->UserData == SOUND_NULL)
        {
            ++it;
            continue;
        }
        if (static_cast<int>(obj->LastAccessTime + obj->Delay) < ticks)
        {
            if (!g_SoundManager.UpdateSoundEffect((SoundHandle)obj->UserData, volume))
            {
                obj->UserData = SOUND_NULL;
            }
            it = m_UsedSoundList.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CGame::PauseSound() const
{
    g_SoundManager.PauseSound();
}

void CGame::ResumeSound() const
{
    g_SoundManager.ResumeSound();
}

CSprite *CGame::ExecuteGump(uint16_t id)
{
    g_DebugContext.lastGumpId = id;
    if (int(id) >= MAX_GUMP_DATA_INDEX_COUNT)
    {
        return nullptr;
    }
    CIndexObject &io = g_Index.m_Gump[id];
    if (io.UserData == nullptr)
    {
        if (io.Address == 0u)
        {
            return nullptr;
        }

        io.UserData = g_UOFileReader.ReadGump(io);
        if (io.UserData != nullptr)
        {
            m_UsedGumpList.push_back(&g_Index.m_Gump[id]);
        }
    }
    io.LastAccessTime = g_Ticks;
    return (CSprite *)io.UserData;
}

CSprite *CGame::ExecuteLandArt(uint16_t id)
{
    g_DebugContext.lastLandArtId = id;
    if (id >= MAX_LAND_DATA_INDEX_COUNT)
    {
        return nullptr;
    }
    CIndexObject &io = g_Index.m_Land[id];
    if (io.UserData == nullptr)
    {
        if (io.Address == 0u)
        { //nodraw tiles banned
            return nullptr;
        }

        io.UserData = g_UOFileReader.ReadArt(id, io, false);
        if (io.UserData != nullptr)
        {
            m_UsedLandList.push_back(&g_Index.m_Land[id]);
        }
    }
    io.LastAccessTime = g_Ticks;
    return (CSprite *)io.UserData;
}

CSprite *CGame::ExecuteStaticArtAnimated(uint16_t id)
{
    g_DebugContext.lastAnimId = id;
    return ExecuteStaticArt(id + g_Index.m_Static[id].Offset);
}

CSprite *CGame::ExecuteStaticArt(uint16_t id)
{
    g_DebugContext.lastStaticId = id;
    if (int(id) >= MAX_STATIC_DATA_INDEX_COUNT)
    {
        return nullptr;
    }

    CIndexObject &io = g_Index.m_Static[id];
    if (io.UserData == nullptr)
    {
        if (io.Address == 0u)
        { //nodraw tiles banned
            return nullptr;
        }

        io.UserData = g_UOFileReader.ReadArt(id, io, true);
        if (io.UserData != nullptr)
        {
            auto spr = (CSprite *)io.UserData;
            io.Width = ((spr->Width / 2) + 1);
            io.Height = spr->Height - 20;
            m_UsedStaticList.push_back(&g_Index.m_Static[id]);
        }
    }
    io.LastAccessTime = g_Ticks;
    return (CSprite *)io.UserData;
}

CSprite *CGame::ExecuteTexture(uint16_t id)
{
    g_DebugContext.lastTextureId = id;
    id = g_Data.m_Land[id].TexID;
    if ((id == 0u) || id >= MAX_LAND_TEXTURES_DATA_INDEX_COUNT)
    {
        return nullptr;
    }

    CIndexObject &io = g_Index.m_Texture[id];
    if (io.UserData == nullptr)
    {
        if (io.Address == 0u)
        {
            return nullptr;
        }

        io.UserData = g_UOFileReader.ReadTexture(io);
        if (io.UserData != nullptr)
        {
            m_UsedTextureList.push_back(&g_Index.m_Texture[id]);
        }
    }
    io.LastAccessTime = g_Ticks;
    return (CSprite *)io.UserData;
}

CSprite *CGame::ExecuteLight(uint8_t id)
{
    g_DebugContext.lastLightId = id;
    if (id >= MAX_LIGHTS_DATA_INDEX_COUNT)
    {
        id = 0;
    }

    CIndexObject &io = g_Index.m_Light[id];
    if (io.UserData == nullptr)
    {
        if (io.Address == 0u)
        {
            return nullptr;
        }

        io.UserData = g_UOFileReader.ReadLight(io);
        if (io.UserData != nullptr)
        {
            m_UsedLightList.push_back(&g_Index.m_Light[id]);
        }
    }
    io.LastAccessTime = g_Ticks;
    return (CSprite *)io.UserData;
}

bool CGame::ExecuteGumpPart(uint16_t id, int count)
{
    g_DebugContext.lastGumpPartId = id;
    bool result = true;
    for (int i = 0; i < count; i++)
    {
        if (ExecuteGump(id + (uint16_t)i) == nullptr)
        {
            result = false;
        }
    }
    return result;
}

// FIXME: gfx
void CGame::DrawGump(uint16_t id, uint16_t color, int x, int y, bool partialHue)
{
    auto spr = ExecuteGump(id);
    if (spr != nullptr)
    {
        if (!g_GrayedPixels && (color != 0u))
        {
            auto uniformValue = partialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = uniformValue;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

            g_ColorManager.SendColorsToShader(color);
        }
        else
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_NO_COLOR;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        }

        spr->Texture->Draw(x, y);
    }
}

// FIXME: gfx
void CGame::DrawGump(
    uint16_t id, uint16_t color, int x, int y, int width, int height, bool partialHue)
{
    auto spr = ExecuteGump(id);
    if (spr != nullptr)
    {
        if (!g_GrayedPixels && (color != 0u))
        {
            auto uniformValue = partialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = uniformValue;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

            g_ColorManager.SendColorsToShader(color);
        }
        else
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_NO_COLOR;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        }
        spr->Texture->Draw(x, y, width, height);
    }
}

static void DrawResizepicGump_Internal(uint16_t id, int x, int y, int width, int height)
{
    CGLTexture *th[9] = { nullptr };
    for (int i = 0; i < 9; i++)
    {
        auto spr = g_Game.ExecuteGump(id + (uint16_t)i);
        if (spr == nullptr)
        {
            return;
        }

        assert(spr->Texture != nullptr);
        auto pth = spr->Texture;
        if (i == 4)
        {
            th[8] = pth;
        }
        else if (i > 4)
        {
            th[i - 1] = pth;
        }
        else
        {
            th[i] = pth;
        }
    }
#ifndef NEW_RENDERER_ENABLED
    g_GL.DrawResizepic(th, x, y, width, height);
#else
    int offsetTop = std::max(th[0]->Height, th[2]->Height) - th[1]->Height;
    int offsetBottom = std::max(th[5]->Height, th[7]->Height) - th[6]->Height;
    int offsetLeft = std::max(th[0]->Width, th[5]->Width) - th[3]->Width;
    int offsetRight = std::max(th[2]->Width, th[7]->Width) - th[4]->Width;

    for (int i = 0; i < 9; i++)
    {
        int drawWidth = th[i]->Width;
        int drawHeight = th[i]->Height;
        float drawCountX = 1.0f;
        float drawCountY = 1.0f;
        int drawX = x;
        int drawY = y;

        switch (i)
        {
            case 1:
            {
                drawX += th[0]->Width;

                drawWidth = width - th[0]->Width - th[2]->Width;

                drawCountX = drawWidth / (float)th[i]->Width;

                break;
            }
            case 2:
            {
                drawX += width - drawWidth;
                drawY += offsetTop;

                break;
            }
            case 3:
            {
                drawY += th[0]->Height;
                drawX += offsetLeft;

                drawHeight = height - th[0]->Height - th[5]->Height;

                drawCountY = drawHeight / (float)th[i]->Height;

                break;
            }
            case 4:
            {
                drawX += width - drawWidth - offsetRight;
                drawY += th[2]->Height;

                drawHeight = height - th[2]->Height - th[7]->Height;

                drawCountY = drawHeight / (float)th[i]->Height;

                break;
            }
            case 5:
            {
                drawY += height - drawHeight;

                break;
            }
            case 6:
            {
                drawX += th[5]->Width;
                drawY += height - drawHeight - offsetBottom;

                drawWidth = width - th[5]->Width - th[7]->Width;

                drawCountX = drawWidth / (float)th[i]->Width;

                break;
            }
            case 7:
            {
                drawX += width - drawWidth;
                drawY += height - drawHeight;

                break;
            }
            case 8:
            {
                drawX += th[0]->Width;
                drawY += th[0]->Height;

                drawWidth = width - th[0]->Width - th[2]->Width;

                drawHeight = height - th[2]->Height - th[7]->Height;

                drawCountX = drawWidth / (float)th[i]->Width;
                drawCountY = drawHeight / (float)th[i]->Height;

                break;
            }
            default:
                break;
        }

        if (drawWidth < 1 || drawHeight < 1)
        {
            continue;
        }

        RenderAdd_DrawQuad(
            g_renderCmdList,
            DrawQuadCmd{ th[i]->Texture,
                         drawX,
                         drawY,
                         uint32_t(drawWidth),
                         uint32_t(drawHeight),
                         drawCountX,
                         drawCountY });
    }
#endif
}

void CGame::DrawResizepicGump(uint16_t id, int x, int y, int width, int height, bool checktrans)
{
#ifndef NEW_RENDERER_ENABLED
    if (checktrans)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        DrawResizepicGump_Internal(id, x, y, height, height);
        glDisable(GL_BLEND);
        glEnable(GL_STENCIL_TEST);
        DrawResizepicGump_Internal(id, x, y, width, height);
        glDisable(GL_STENCIL_TEST);
    }
    else
    {
        DrawResizepicGump_Internal(id, x, y, width, height);
    }
#else
    if (checktrans)
    {
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        DrawResizepicGump_Internal(id, x, y, width, height);
        RenderAdd_DisableBlend(g_renderCmdList);

        // FIXME epatitucci what were the original values for func, op, ref & mask?
        RenderAdd_SetStencil(g_renderCmdList, StencilStateCmd{});
        DrawResizepicGump_Internal(id, x, y, width, height);
        RenderAdd_DisableStencil(g_renderCmdList);
    }
    else
    {
        DrawResizepicGump_Internal(id, x, y, width, height);
    }
#endif
}

void CGame::DrawLandTexture(CLandObject *land, uint16_t color, int x, int y)
{
    uint16_t id = land->Graphic;
    auto spr = ExecuteTexture(id);
    if (spr == nullptr)
    {
        DrawLandArt(id, color, x, y);
    }
    else
    {
        if (g_OutOfRangeColor != 0u)
        {
            color = g_OutOfRangeColor;
        }
        if (!g_GrayedPixels && (color != 0u))
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_LAND_COLORED);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_LAND_COLORED;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
            g_ColorManager.SendColorsToShader(color);
        }
        else
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_LAND);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_LAND;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        }
        assert(spr->Texture != nullptr);
#ifndef NEW_RENDERER_ENABLED
        g_GL.DrawLandTexture(*spr->Texture, x, y + (land->GetZ() * 4), land);
#else
        DrawLandTileCmd cmd{
            spr->Texture->Texture,
            x,
            y + (land->GetZ() * 4),
            { land->m_Rect.x, land->m_Rect.y, uint32_t(land->m_Rect.w), uint32_t(land->m_Rect.h) },
            {
                { float(land->m_Normals[0].X),
                  float(land->m_Normals[0].Y),
                  float(land->m_Normals[0].Z) },
                { float(land->m_Normals[1].X),
                  float(land->m_Normals[1].Y),
                  float(land->m_Normals[1].Z) },
                { float(land->m_Normals[2].X),
                  float(land->m_Normals[2].Y),
                  float(land->m_Normals[2].Z) },
                { float(land->m_Normals[3].X),
                  float(land->m_Normals[3].Y),
                  float(land->m_Normals[3].Z) },
            }
        };
        RenderAdd_DrawLandTile(g_renderCmdList, cmd);
#endif
    }
}

// FIXME: gfx
void CGame::DrawLandArt(uint16_t id, uint16_t color, int x, int y)
{
    auto spr = ExecuteLandArt(id);
    if (spr != nullptr)
    {
        if (g_OutOfRangeColor != 0u)
        {
            color = g_OutOfRangeColor;
        }

        if (!g_GrayedPixels && (color != 0u))
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_COLORED;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
            g_ColorManager.SendColorsToShader(color);
        }
        else
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_NO_COLOR;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        }
        assert(spr->Texture != nullptr);
        spr->Texture->Draw(x - 22, y - 22);
    }
}

// FIXME: gfx
void CGame::DrawStaticArt(uint16_t id, uint16_t color, int x, int y, bool selection)
{
    auto spr = ExecuteStaticArt(id);
    if (spr != nullptr && id > 1)
    {
        if (g_OutOfRangeColor != 0u)
        {
            color = g_OutOfRangeColor;
        }

        auto uniformValue = SDM_NO_COLOR;
        if (!g_GrayedPixels && (color != 0u))
        {
            uniformValue = SDM_COLORED;
            if (!selection && IsPartialHue(GetStaticFlags(id)))
            {
                uniformValue = SDM_PARTIAL_HUE;
            }
            g_ColorManager.SendColorsToShader(color);
        }
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        assert(spr->Texture != nullptr);
        spr->Texture->Draw(x - g_Index.m_Static[id].Width, y - g_Index.m_Static[id].Height);
    }
}

// FIXME: gfx
void CGame::DrawStaticArtAnimated(uint16_t id, uint16_t color, int x, int y, bool selection)
{
    DrawStaticArt(id + g_Index.m_Static[id].Offset, color, x, y, selection);
}

// FIXME: gfx
void CGame::DrawStaticArtRotated(uint16_t id, uint16_t color, int x, int y, float angle)
{
    auto spr = ExecuteStaticArt(id);
    if (spr != nullptr && id > 1)
    {
        if (g_OutOfRangeColor != 0u)
        {
            color = g_OutOfRangeColor;
        }

        auto uniformValue = SDM_NO_COLOR;
        if (!g_GrayedPixels && (color != 0u))
        {
            uniformValue = SDM_COLORED;
            g_ColorManager.SendColorsToShader(color);
        }
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        assert(spr->Texture != nullptr);
        spr->Texture->DrawRotated(x, y, angle);
    }
}

void CGame::DrawStaticArtAnimatedRotated(uint16_t id, uint16_t color, int x, int y, float angle)
{
    DrawStaticArtRotated(id + g_Index.m_Static[id].Offset, color, x, y, angle);
}

void CGame::DrawStaticArtTransparent(uint16_t id, uint16_t color, int x, int y, bool selection)
{
    auto spr = ExecuteStaticArt(id);
    if (spr != nullptr && id > 1)
    {
        if (g_OutOfRangeColor != 0u)
        {
            color = g_OutOfRangeColor;
        }
        auto uniformValue = SDM_NO_COLOR;
        if (!g_GrayedPixels && (color != 0u))
        {
            uniformValue = SDM_COLORED;
            if (!selection && IsPartialHue(GetStaticFlags(id)))
            {
                uniformValue = SDM_PARTIAL_HUE;
            }
            g_ColorManager.SendColorsToShader(color);
        }
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        assert(spr->Texture != nullptr);
        spr->Texture->DrawTransparent(
            x - g_Index.m_Static[id].Width, y - g_Index.m_Static[id].Height);
    }
}

// FIXME: gfx
void CGame::DrawStaticArtAnimatedTransparent(
    uint16_t id, uint16_t color, int x, int y, bool selection)
{
    DrawStaticArtTransparent(id + g_Index.m_Static[id].Offset, color, x, y, selection);
}

// FIXME: gfx
void CGame::DrawStaticArtInContainer(
    uint16_t id, uint16_t color, int x, int y, bool selection, bool onMouse)
{
    auto spr = ExecuteStaticArt(id);
    if (spr != nullptr)
    {
        if (onMouse)
        {
            x -= spr->Width / 2;
            y -= spr->Height / 2;
        }

        auto uniformValue = SDM_NO_COLOR;
        if (!g_GrayedPixels && (color != 0u))
        {
            uniformValue = SDM_COLORED;
            if (color >= 0x4000)
            {
                color = 0x1;
            }
            else if (!selection && IsPartialHue(GetStaticFlags(id)))
            {
                uniformValue = SDM_PARTIAL_HUE;
            }
            g_ColorManager.SendColorsToShader(color);
        }
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        assert(spr->Texture != nullptr);
        spr->Texture->Draw(x, y);
    }
}

void CGame::DrawLight(LIGHT_DATA &light)
{
    auto spr = ExecuteLight(light.ID);
    if (spr != nullptr)
    {
        auto uniformValue = SDM_NO_COLOR;
        if (light.Color != 0u)
        {
            uniformValue = SDM_COLORED;
            g_ColorManager.SendColorsToShader(light.Color);
        }
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        assert(spr->Texture != nullptr);
        spr->Texture->Draw(
            light.DrawX - g_RenderBounds.GameWindowPosX - (spr->Width / 2),
            light.DrawY - g_RenderBounds.GameWindowPosY - (spr->Height / 2));
    }
}

// FIXME: bounding box, typo
bool CGame::PolygonePixelsInXY(int x, int y, int width, int height)
{
    x = g_MouseManager.Position.X - x;
    y = g_MouseManager.Position.Y - y;
    return !(x < 0 || y < 0 || x >= width || y >= height);
}

bool CGame::GumpPixelsInXY(uint16_t id, int x, int y)
{
    auto spr = (CSprite *)g_Index.m_Gump[id].UserData;
    if (spr != nullptr)
    {
        return spr->Select(x, y);
    }
    return false;
}

bool CGame::GumpPixelsInXY(uint16_t id, int x, int y, int width, int height)
{
    auto spr = (CSprite *)g_Index.m_Gump[id].UserData;
    if (spr == nullptr)
    {
        return false;
    }

    x = g_MouseManager.Position.X - x;
    y = g_MouseManager.Position.Y - y;
    if (x < 0 || y < 0 || (width > 0 && x >= width) || (height > 0 && y >= height))
    {
        return false;
    }

    const int spriteWidth = spr->Width;
    const int spriteHeight = spr->Height;
    if (width == 0)
    {
        width = spriteWidth;
    }
    if (height == 0)
    {
        height = spriteHeight;
    }

    while (x > spriteWidth && width > spriteWidth)
    {
        x -= spriteWidth;
        width -= spriteWidth;
    }
    if (x < 0 || x > width)
    {
        return false;
    }

    while (y > spriteHeight && height > spriteHeight)
    {
        y -= spriteHeight;
        height -= spriteHeight;
    }
    if (y < 0 || y > height)
    {
        return false;
    }
    return spr->TestHit(x, y, true);
}

bool CGame::ResizepicPixelsInXY(uint16_t id, int x, int y, int width, int height)
{
    const int tempX = g_MouseManager.Position.X - x;
    const int tempY = g_MouseManager.Position.Y - y;
    if (tempX < 0 || tempY < 0 || tempX >= width || tempY >= height)
    {
        return false;
    }

    CSprite *th[9] = { nullptr };
    for (int i = 0; i < 9; i++)
    {
        auto pth = (CSprite *)g_Index.m_Gump[id + i].UserData;
        if (pth == nullptr)
        {
            return false;
        }

        if (i == 4)
        {
            th[8] = pth;
        }
        else if (i > 4)
        {
            th[i - 1] = pth;
        }
        else
        {
            th[i] = pth;
        }
    }

    int offsetTop = std::max(th[0]->Height, th[2]->Height) - th[1]->Height;
    int offsetBottom = std::max(th[5]->Height, th[7]->Height) - th[6]->Height;
    int offsetLeft = std::max(th[0]->Width, th[5]->Width) - th[3]->Width;
    int offsetRight = std::max(th[2]->Width, th[7]->Width) - th[4]->Width;
    for (int i = 0; i < 9; i++)
    {
        switch (i)
        {
            case 0:
            {
                if (GumpPixelsInXY(id, x, y))
                {
                    return true;
                }
                break;
            }
            case 1:
            {
                int DW = width - th[0]->Width - th[2]->Width;
                if (DW < 1)
                {
                    break;
                }

                if (GumpPixelsInXY(id + 1, x + th[0]->Width, y, DW, 0))
                {
                    return true;
                }
                break;
            }
            case 2:
            {
                if (GumpPixelsInXY(id + 2, x + width - th[i]->Width, y + offsetTop))
                {
                    return true;
                }
                break;
            }
            case 3:
            {
                int DH = height - th[0]->Height - th[5]->Height;
                if (DH < 1)
                {
                    break;
                }

                if (GumpPixelsInXY(id + 3, x + offsetLeft, y + th[0]->Height, 0, DH))
                {
                    return true;
                }
                break;
            }
            case 4:
            {
                int DH = height - th[2]->Height - th[7]->Height;
                if (DH < 1)
                {
                    break;
                }

                if (GumpPixelsInXY(
                        id + 5, x + width - th[i]->Width - offsetRight, y + th[2]->Height, 0, DH))
                {
                    return true;
                }
                break;
            }
            case 5:
            {
                if (GumpPixelsInXY(id + 6, x, y + height - th[i]->Height))
                {
                    return true;
                }
                break;
            }
            case 6:
            {
                int DW = width - th[5]->Width - th[7]->Width;
                if (DW < 1)
                {
                    break;
                }

                if (GumpPixelsInXY(
                        id + 7, x + th[5]->Width, y + height - th[i]->Height - offsetBottom, DW, 0))
                {
                    return true;
                }
                break;
            }
            case 7:
            {
                if (GumpPixelsInXY(id + 8, x + width - th[i]->Width, y + height - th[i]->Height))
                {
                    return true;
                }
                break;
            }
            case 8:
            {
                int DW = width - th[0]->Width - th[2]->Width;
                if (DW < 1)
                {
                    break;
                }

                int DH = height - th[2]->Height - th[7]->Height;
                if (DH < 1)
                {
                    break;
                }

                if (GumpPixelsInXY(id + 4, x + th[0]->Width, y + th[0]->Height, DW, DH))
                {
                    return true;
                }
                break;
            }
            default:
                break;
        }
    }
    return false;
}

bool CGame::StaticPixelsInXY(uint16_t id, int x, int y)
{
    CIndexObject &io = g_Index.m_Static[id];
    auto spr = (CSprite *)io.UserData;
    if (spr != nullptr)
    {
        return spr->Select(x - io.Width, y - io.Height);
    }
    return false;
}

bool CGame::StaticPixelsInXYAnimated(uint16_t id, int x, int y)
{
    return StaticPixelsInXY(id + g_Index.m_Static[id].Offset, x, y);
}

bool CGame::StaticPixelsInXYInContainer(uint16_t id, int x, int y)
{
    auto spr = (CSprite *)g_Index.m_Static[id].UserData;
    if (spr != nullptr)
    {
        return spr->Select(x, y);
    }
    return false;
}

bool CGame::LandPixelsInXY(uint16_t id, int x, int y)
{
    auto spr = (CSprite *)g_Index.m_Land[id].UserData;
    if (spr != nullptr)
    {
        return spr->Select(x - 22, y - 22);
    }
    return false;
}

bool CGame::LandTexturePixelsInXY(int x, int y, const SDL_Rect &r)
{
    y -= 22;
    int testX = g_MouseManager.Position.X - x;
    int testY = g_MouseManager.Position.Y;

    int y0 = -r.x;
    int y1 = 22 - r.y;
    int y2 = 44 - r.w;
    int y3 = 22 - r.h;

    bool result =
        ((testY >= testX * (y1 - y0) / -22 + y + y0) &&
         (testY >= testX * (y3 - y0) / 22 + y + y0) && (testY <= testX * (y3 - y2) / 22 + y + y2) &&
         (testY <= testX * (y1 - y2) / -22 + y + y2));

    return result;
}

void CGame::CreateTextMessageF(uint8_t font, uint16_t color, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    char buf[512] = { 0 };
    vsnprintf(buf, sizeof(buf), format, arg);
    CreateTextMessage(TT_SYSTEM, 0xFFFFFFFF, font, color, buf);
    va_end(arg);
}

void CGame::CreateUnicodeTextMessageF(uint8_t font, uint16_t color, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    char buf[512] = { 0 };
    vsnprintf(buf, sizeof(buf), format, arg);
    CreateUnicodeTextMessage(TT_SYSTEM, 0xFFFFFFFF, font, color, wstr_from(buf));
    va_end(arg);
}

void CGame::CreateTextMessage(
    const TEXT_TYPE &type,
    int serial,
    uint8_t font,
    uint16_t color,
    const astr_t &text,
    CRenderWorldObject *clientObj)
{
    CTextData *td = new CTextData();
    td->Unicode = false;
    td->Font = font;
    td->Serial = serial;
    td->Color = color;
    td->Timer = g_Ticks;
    td->Type = type;
    td->Text = text;

    switch (type)
    {
        case TT_SYSTEM:
        {
            td->GenerateTexture(TEXT_SYSTEM_MESSAGE_MAX_WIDTH);
            AddSystemMessage(td);

            break;
        }
        case TT_OBJECT:
        {
            CGameObject *obj = g_World->FindWorldObject(serial);

            if (obj != nullptr)
            {
                int width = g_FontManager.GetWidthA(font, text);

                td->Color = 0;

                if (width > TEXT_MESSAGE_MAX_WIDTH)
                {
                    width =
                        g_FontManager.GetWidthExA(font, text, TEXT_MESSAGE_MAX_WIDTH, TS_LEFT, 0);
                    td->GenerateTexture(width, 0, TS_LEFT);
                }
                else
                {
                    td->GenerateTexture(0, 0, TS_CENTER);
                }

                td->Color = color;

                uint32_t container = obj->Container;

                if (container == 0xFFFFFFFF)
                {
                    g_WorldTextRenderer.AddText(td);
                }
                else if (!obj->NPC)
                {
                    td->SetX(g_ClickObject.X);
                    td->SetY(g_ClickObject.Y);

                    CGump *gump = g_GumpManager.GetGump(container, 0, GT_CONTAINER);

                    if (gump == nullptr)
                    {
                        CGameObject *topobj = obj->GetTopObject();

                        if (((CGameItem *)obj)->Layer != OL_NONE)
                        {
                            gump = g_GumpManager.GetGump(topobj->Serial, 0, GT_PAPERDOLL);
                        }

                        if (gump == nullptr)
                        {
                            gump = g_GumpManager.GetGump(topobj->Serial, 0, GT_TRADE);

                            if (gump == nullptr)
                            {
                                topobj = (CGameObject *)topobj->m_Items;

                                while (topobj != nullptr && topobj->Graphic != 0x1E5E)
                                {
                                    topobj = (CGameObject *)topobj->m_Next;
                                }

                                if (topobj != nullptr)
                                {
                                    gump = g_GumpManager.GetGump(0, topobj->Serial, GT_TRADE);
                                }
                            }
                        }
                    }

                    if (gump != nullptr)
                    {
                        CTextRenderer *tr = gump->GetTextRenderer();

                        if (tr != nullptr)
                        {
                            tr->AddText(td);
                        }
                    }
                }
                if (g_ConfigManager.AlwaysDisplayHumanoidsName && obj->IsHuman() &&
                    obj->GetName() == td->Text)
                {
                    AddJournalMessage(td, "You see: ");
                }
                else
                {
                    obj->AddText(td);
                }
            }
            else
            {
                td->GenerateTexture(TEXT_SYSTEM_MESSAGE_MAX_WIDTH);
                AddSystemMessage(td);
            }

            break;
        }
        case TT_CLIENT:
        {
            int width = g_FontManager.GetWidthA(font, text);

            if (width > TEXT_MESSAGE_MAX_WIDTH)
            {
                width = g_FontManager.GetWidthExA(font, text, TEXT_MESSAGE_MAX_WIDTH, TS_LEFT, 0);
                td->GenerateTexture(width, 0, TS_LEFT);
            }
            else
            {
                td->GenerateTexture(0, 0, TS_CENTER);
            }

            clientObj->AddText(td);
            g_WorldTextRenderer.AddText(td);

            break;
        }
    }
}

void CGame::CreateUnicodeTextMessage(
    const TEXT_TYPE &type,
    int serial,
    uint8_t font,
    uint16_t color,
    const wstr_t &text,
    CRenderWorldObject *clientObj)
{
    CTextData *td = new CTextData();
    td->Unicode = true;
    td->Font = font;
    td->Serial = serial;
    td->Color = color;
    td->Timer = g_Ticks;
    td->Type = type;
    td->UnicodeText = text;

    switch (type)
    {
        case TT_SYSTEM:
        {
            td->GenerateTexture(TEXT_SYSTEM_MESSAGE_MAX_WIDTH, UOFONT_BLACK_BORDER);
            AddSystemMessage(td);
            break;
        }
        case TT_OBJECT:
        {
            CGameObject *obj = g_World->FindWorldObject(serial);
            if (obj != nullptr)
            {
                int width = g_FontManager.GetWidthW(font, text);
                if (width > TEXT_MESSAGE_MAX_WIDTH)
                {
                    width = g_FontManager.GetWidthExW(
                        font, text, TEXT_MESSAGE_MAX_WIDTH, TS_LEFT, UOFONT_BLACK_BORDER);
                    td->GenerateTexture(width, UOFONT_BLACK_BORDER, TS_LEFT);
                }
                else
                {
                    td->GenerateTexture(0, UOFONT_BLACK_BORDER, TS_CENTER);
                }

                uint32_t container = obj->Container;
                if (container == 0xFFFFFFFF)
                {
                    g_WorldTextRenderer.AddText(td);
                }
                else if (!obj->NPC)
                {
                    td->SetX(g_ClickObject.X);
                    td->SetY(g_ClickObject.Y);

                    CGump *gump = g_GumpManager.GetGump(container, 0, GT_CONTAINER);
                    if (gump == nullptr)
                    {
                        CGameObject *topobj = obj->GetTopObject();
                        if (((CGameItem *)obj)->Layer != OL_NONE)
                        {
                            gump = g_GumpManager.GetGump(topobj->Serial, 0, GT_PAPERDOLL);
                        }

                        if (gump == nullptr)
                        {
                            gump = g_GumpManager.GetGump(topobj->Serial, 0, GT_TRADE);
                            if (gump == nullptr)
                            {
                                topobj = (CGameObject *)topobj->m_Items;
                                while (topobj != nullptr && topobj->Graphic != 0x1E5E)
                                {
                                    topobj = (CGameObject *)topobj->m_Next;
                                }

                                if (topobj != nullptr)
                                {
                                    gump = g_GumpManager.GetGump(0, topobj->Serial, GT_TRADE);
                                }
                            }
                        }
                    }

                    if (gump != nullptr)
                    {
                        CTextRenderer *tr = gump->GetTextRenderer();
                        if (tr != nullptr)
                        {
                            tr->AddText(td);
                        }
                    }
                }
                if (g_ConfigManager.AlwaysDisplayHumanoidsName && obj->IsHuman() &&
                    obj->GetName() == td->Text)
                {
                    AddJournalMessage(td, "You see: ");
                }
                else
                {
                    obj->AddText(td);
                }
            }
            else
            {
                td->GenerateTexture(TEXT_SYSTEM_MESSAGE_MAX_WIDTH, UOFONT_BLACK_BORDER);
                AddSystemMessage(td);
            }

            break;
        }
        case TT_CLIENT:
        {
            int width = g_FontManager.GetWidthW(font, text);
            if (width > TEXT_MESSAGE_MAX_WIDTH)
            {
                width = g_FontManager.GetWidthExW(font, text, TEXT_MESSAGE_MAX_WIDTH, TS_LEFT, 0);
                td->GenerateTexture(width, 0, TS_LEFT);
            }
            else
            {
                td->GenerateTexture(0, UOFONT_BLACK_BORDER, TS_LEFT);
            }

            clientObj->AddText(td);
            g_WorldTextRenderer.AddText(td);
            break;
        }
    }
}

void CGame::AddSystemMessage(CTextData *msg)
{
    g_SystemChat.Add(msg);
    AddJournalMessage(msg, "");
}

void CGame::AddJournalMessage(CTextData *msg, const astr_t &name)
{
    CTextData *jmsg = new CTextData(msg);

    if (!jmsg->Unicode)
    {
        jmsg->Text = name + jmsg->Text;
        jmsg->Font = 9;
    }
    else
    {
        //if (msg->Type == TT_SYSTEM)
        //	jmsg->Color = 0;

        jmsg->UnicodeText = wstr_from(name) + jmsg->UnicodeText;
        jmsg->Font = 0;
    }

    /*if (msg->Type == TT_OBJECT)
		jmsg->GenerateTexture(214, UOFONT_INDENTION | UOFONT_BLACK_BORDER);
	else
		jmsg->GenerateTexture(214, UOFONT_INDENTION);*/

    g_Journal.Add(jmsg);
}

void CGame::ChangeMap(uint8_t newmap)
{
    if (newmap > 5)
    {
        newmap = 0;
    }

    if (g_CurrentMap != newmap)
    {
        g_CurrentMap = newmap;

        if (g_World != nullptr && g_Player != nullptr)
        {
            g_Player->MapIndex = g_CurrentMap;
            g_Player->RemoveRender();

            CGameObject *obj = (CGameObject *)g_World->m_Items;

            while (obj != nullptr)
            {
                CGameObject *next = (CGameObject *)obj->m_Next;

                if (obj->MapIndex != g_CurrentMap)
                {
                    if (g_Party.Contains(obj->Serial))
                    {
                        obj->RemoveRender();
                        g_GumpManager.UpdateContent(obj->Serial, 0, GT_STATUSBAR);
                    }
                    else
                    {
                        g_World->RemoveObject(obj);
                    }
                }

                obj = next;
            }

            g_MapManager.Clear();
            g_MapManager.Init();

            g_MapManager.AddRender(g_Player);
        }
    }
}

void CGame::PickupItem(CGameItem *obj, int count, bool isGameFigure)
{
    if (!g_ObjectInHand.Enabled)
    {
        g_ObjectInHand.Clear();
        g_ObjectInHand.Enabled = true;

        if (count == 0)
        {
            count = obj->Count;
        }

        g_ObjectInHand.Serial = obj->Serial;
        g_ObjectInHand.Graphic = obj->Graphic;
        g_ObjectInHand.Color = obj->Color;
        g_ObjectInHand.Container = obj->Container;
        g_ObjectInHand.Layer = obj->Layer;
        g_ObjectInHand.Flags = obj->GetFlags();
        g_ObjectInHand.TiledataPtr = obj->GetStaticData();
        g_ObjectInHand.Count = count;
        g_ObjectInHand.IsGameFigure = isGameFigure;
        g_ObjectInHand.X = obj->GetX();
        g_ObjectInHand.Y = obj->GetY();
        g_ObjectInHand.Z = obj->GetZ();
        g_ObjectInHand.TotalCount = obj->Count;

        CPacketPickupRequest(g_ObjectInHand.Serial, count).Send();

        g_World->ObjectToRemove = g_ObjectInHand.Serial;
    }
}

void CGame::DropItem(int container, uint16_t x, uint16_t y, char z)
{
    if (g_ObjectInHand.Enabled && g_ObjectInHand.Serial != container)
    {
        if (g_Config.ProtocolClientVersion >= CV_6017)
        {
            CPacketDropRequestNew(g_ObjectInHand.Serial, x, y, z, 0, container).Send();
        }
        else
        {
            CPacketDropRequestOld(g_ObjectInHand.Serial, x, y, z, container).Send();
        }

        g_ObjectInHand.Enabled = false;
        g_ObjectInHand.Dropped = true;
    }
}

void CGame::EquipItem(uint32_t container)
{
    if (g_ObjectInHand.Enabled)
    {
        if (IsWearable(g_ObjectInHand.TiledataPtr->Flags))
        {
            uint16_t graphic = g_ObjectInHand.Graphic;
            if (container == 0u)
            {
                container = g_PlayerSerial;
            }

            CPacketEquipRequest(g_ObjectInHand.Serial, g_Data.m_Static[graphic].Layer, container)
                .Send();

            g_ObjectInHand.Enabled = false;
            g_ObjectInHand.Dropped = true;
        }
    }
}

void CGame::ChangeWarmode(uint8_t status)
{
    uint8_t newstatus = (uint8_t)(!g_Player->Warmode);

    if (status != 0xFF)
    {
        const bool s = status != 0;
        if (g_Player->Warmode == s)
        {
            return;
        }

        newstatus = status;
    }

    if (newstatus == 1 && g_ConfigManager.GetMusic())
    {
        PlayMusic(rand() % 3 + 38, true);
    }
    else if (newstatus == 0)
    {
        g_SoundManager.StopWarMusic();
    }

    CPacketChangeWarmode(newstatus).Send();
}

void CGame::Click(uint32_t serial)
{
    CPacketClickRequest(serial).Send();

    CGameObject *obj = g_World->FindWorldObject(serial);
    if (obj != nullptr)
    {
        obj->Clicked = true;
    }
}

void CGame::DoubleClick(uint32_t serial)
{
    if (serial >= 0x40000000)
    {
        g_LastUseObject = serial;
    }

    CPacketDoubleClickRequest(serial).Send();
}

void CGame::PaperdollReq(uint32_t serial)
{
    //g_LastUseObject = serial;

    CPacketDoubleClickRequest(serial | 0x80000000).Send();
}

void CGame::Attack(uint32_t serial)
{
    if (g_ConfigManager.CriminalActionsQuery && g_World != nullptr)
    {
        CGameCharacter *target = g_World->FindWorldCharacter(serial);

        if (target != nullptr && (NOTORIETY_TYPE)g_Player->Notoriety == NT_INNOCENT &&
            (NOTORIETY_TYPE)target->Notoriety == NT_INNOCENT)
        {
            int x = g_ConfigManager.GameWindowX + (g_ConfigManager.GameWindowWidth / 2) - 40;
            int y = g_ConfigManager.GameWindowY + (g_ConfigManager.GameWindowHeight / 2) - 20;

            CGumpQuestion *newgump =
                new CGumpQuestion(0, x, y, CGumpQuestion::ID_GQ_STATE_ATTACK_REQUEST);
            newgump->ID = serial;

            g_GumpManager.AddGump(newgump);

            InitScreen(GS_GAME_BLOCKED);
            g_GameBlockedScreen.Code = 3;

            return;
        }
    }

    AttackReq(serial);
}

void CGame::AttackReq(uint32_t serial)
{
    g_LastAttackObject = serial;
    //CPacketStatusRequest(serial).Send();
    CPacketAttackRequest(serial).Send();
}

void CGame::SendASCIIText(const char *str, SPEECH_TYPE type)
{
    CPacketASCIISpeechRequest(str, type, 3, g_ConfigManager.SpeechColor).Send();
}

void CGame::CastSpell(int index)
{
    if (index >= 0)
    {
        g_LastSpellIndex = index;
        CPacketCastSpell(index).Send();
    }
}

void CGame::CastSpellFromBook(int index, uint32_t serial)
{
    if (index >= 0)
    {
        g_LastSpellIndex = index;
        CPacketCastSpellFromBook(index, serial).Send();
    }
}

void CGame::UseSkill(int index)
{
    if (index >= 0)
    {
        g_LastSkillIndex = index;
        CPacketUseSkill(index).Send();
    }
}

void CGame::OpenDoor()
{
    CPacketOpenDoor().Send();
}

void CGame::EmoteAction(const char *action)
{
    CPacketEmoteAction(action).Send();
}

void CGame::AllNames()
{
    CGameObject *obj = g_World->m_Items;

    while (obj != nullptr)
    {
        if ((obj->NPC && !obj->IsPlayer()) || obj->IsCorpse())
        {
            CPacketClickRequest(obj->Serial).Send();
        }

        obj = (CGameObject *)obj->m_Next;
    }
}

void CGame::RemoveRangedObjects()
{
    if (g_World != nullptr)
    {
        int objectsRange = g_ConfigManager.UpdateRange;

        CGameObject *go = g_World->m_Items;

        while (go != nullptr)
        {
            CGameObject *next = (CGameObject *)go->m_Next;

            if (go->Container == 0xFFFFFFFF && !go->IsPlayer())
            {
                if (go->NPC)
                {
                    if (GetRemoveDistance(g_RemoveRangeXY, go) > objectsRange)
                    {
                        if (g_Party.Contains(go->Serial))
                        {
                            go->RemoveRender();
                            g_GumpManager.UpdateContent(go->Serial, 0, GT_STATUSBAR);
                        }
                        else
                        {
                            g_World->RemoveObject(go);
                        }
                    }
                }
                else if (((CGameItem *)go)->MultiBody)
                {
                    if (!CheckMultiDistance(g_RemoveRangeXY, go, objectsRange))
                    {
                        ((CGameItem *)go)->ClearMultiItems();
                    }
                    //g_World->RemoveObject(go);
                }
                else if (GetRemoveDistance(g_RemoveRangeXY, go) > objectsRange)
                {
                    g_World->RemoveObject(go);
                }
            }

            go = next;
        }
    }

    g_EffectManager.RemoveRangedEffects();
}

void CGame::ClearWorld()
{
    g_CorpseManager.Clear();
    g_Walker.Reset();
    g_ObjectInHand.Clear();
    g_UseItemActions.Clear();

    g_Ping = 0;
    g_ClickObject.Clear();
    g_Weather.Reset();
    g_ConsolePrompt = PT_NONE;
    g_MacroPointer = nullptr;
    g_Season = ST_SUMMER;
    g_OldSeason = ST_SUMMER;
    g_GlobalScale = 1.0;
    g_PathFinder.BlockMoving = false;
    g_SkillsManager.SkillsTotal = 0.0f;
    g_SkillsManager.SkillsRequested = false;

    if (g_World)
        delete g_World;
    g_World = nullptr;
    Info(Client, "\tworld removed");

    g_PopupMenu = nullptr;

    g_GumpManager.Clear();
    Info(Client, "\tgump Manager cleared");

    g_EffectManager.Clear();
    Info(Client, "\teffect List cleared");

    g_GameConsole.Clear();

    g_EntryPointer = nullptr;
    g_GrayMenuCount = 0;

    g_Target.Reset();

    g_SystemChat.Clear();
    Info(Client, "\tsystem chat cleared");

    g_Journal.Clear();
    Info(Client, "\tjournal cleared");

    g_MapManager.Clear();
    Info(Client, "\tmap cleared");

    g_CurrentMap = 0;

    g_Party.Leader = 0;
    g_Party.Inviter = 0;
    g_Party.Clear();

    g_Ability[0] = AT_DISARM;
    g_Ability[1] = AT_PARALYZING_BLOW;

    g_ResizedGump = nullptr;

    g_DrawStatLockers = false;
}

void CGame::LogOut()
{
    Info(Client, "CGame::LogOut->Start");
    SaveLocalConfig(g_PacketManager.ConfigSerial);

    if (g_SendLogoutNotification)
    {
        CPacketLogoutNotification().Send();
    }

    Disconnect();
    Info(Client, "\tdisconnected");

    ClearWorld();

    Info(Client, "CGame::LogOut->End");
    InitScreen(GS_MAIN);
}

void CGame::ConsolePromptSend()
{
    const auto len = g_GameConsole.Length();
    const bool cancel = (len < 1);
    if (g_ConsolePrompt == PT_ASCII)
    {
        CPacketASCIIPromptResponse(g_GameConsole.GetTextA(), cancel).Send();
    }
    else if (g_ConsolePrompt == PT_UNICODE)
    {
        CPacketUnicodePromptResponse(g_GameConsole.GetTextW(), g_Language, cancel).Send();
    }
    g_ConsolePrompt = PT_NONE;
}

void CGame::ConsolePromptCancel()
{
    if (g_ConsolePrompt == PT_ASCII)
    {
        CPacketASCIIPromptResponse({}, true).Send();
    }
    else if (g_ConsolePrompt == PT_UNICODE)
    {
        CPacketUnicodePromptResponse({}, g_Language, true).Send();
    }
    g_ConsolePrompt = PT_NONE;
}

uint64_t CGame::GetLandFlags(uint16_t id)
{
    if (id < g_Data.m_Land.size())
    {
        return g_Data.m_Land[id].Flags;
    }
    return 0;
}

uint64_t CGame::GetStaticFlags(uint16_t id)
{
    if (id < (int)g_Data.m_Static.size())
    {
        return g_Data.m_Static[id].Flags;
    }
    return 0;
}

uint16_t CGame::GetLightColor(uint16_t id)
{
    return g_Index.m_Static[id].LightColor;
}

CSize CGame::GetStaticArtDimension(uint16_t id)
{
    auto spr = ExecuteStaticArt(id);
    if (spr != nullptr)
    {
        return CSize(spr->Width, spr->Height);
    }
    return CSize();
}

CSize CGame::GetGumpDimension(uint16_t id)
{
    auto spr = ExecuteGump(id);
    if (spr != nullptr)
    {
        return CSize(spr->Width, spr->Height);
    }
    return CSize();
}

void CGame::OpenStatus(uint32_t serial)
{
    int x = g_MouseManager.Position.X - 76;
    int y = g_MouseManager.Position.Y - 30;
    CPacketStatusRequest(serial).Send();
    g_GumpManager.AddGump(new CGumpStatusbar(serial, x, y, true));
}

void CGame::DisplayStatusbarGump(int serial, int x, int y)
{
    CPacketStatusRequest packet(serial);
    UOMsg_Send(packet.Data().data(), packet.Data().size());
    CGump *gump = g_GumpManager.GetGump(serial, 0, GT_STATUSBAR);
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
    else
    {
        g_GumpManager.AddGump(new CGumpStatusbar(serial, x, y, true));
    }
}

void CGame::OpenMinimap()
{
    g_GumpManager.AddGump(new CGumpMinimap(0, 0, true));
}

void CGame::OpenWorldMap()
{
    size_t pluginsInfoCount = Crypt::GetPluginsCount();
    if (pluginsInfoCount > 0u)
    {
        CPluginPacketOpenMap().SendToPlugin(); //TODO: query if plugin supports world map
        return;
    }

    int x = g_ConfigManager.GameWindowX + (g_ConfigManager.GameWindowWidth / 2) - 200;
    int y = g_ConfigManager.GameWindowY + (g_ConfigManager.GameWindowHeight / 2) - 150;

    CGumpWorldMap *gump = new CGumpWorldMap(x, y);
    gump->Called = true;

    g_GumpManager.AddGump(gump);
}

void CGame::OpenJournal()
{
    g_GumpManager.AddGump(new CGumpJournal(0, 0, false, 250));
}

void CGame::OpenSkills()
{
    g_SkillsManager.SkillsRequested = true;
    CPacketSkillsRequest(g_PlayerSerial).Send();
}

void CGame::OpenBackpack()
{
    if (g_Player != nullptr)
    {
        CGameItem *pack = g_Player->FindLayer(OL_BACKPACK);

        if (pack != nullptr)
        {
            DoubleClick(pack->Serial);
        }
    }
}

void CGame::OpenLogOut()
{
    const int w = (g_ConfigManager.GameWindowWidth / 2);
    const int h = (g_ConfigManager.GameWindowHeight / 2);
    const int x = g_ConfigManager.GameWindowX + w - 40;
    const int y = g_ConfigManager.GameWindowY + h - 20;
    g_GumpManager.AddGump(new CGumpQuestion(0, x, y, CGumpQuestion::ID_GQ_STATE_QUIT));
    InitScreen(GS_GAME_BLOCKED);
    g_GameBlockedScreen.Code = 3;
}

void CGame::OpenChat()
{
    CPacketOpenChat({}).Send();
}

void CGame::OpenConfiguration()
{
    const int w = (g_ConfigManager.GameWindowWidth / 2);
    const int h = (g_ConfigManager.GameWindowHeight / 2);
    const int x = (g_GameWindow.GetSize().Width / 2) - w;
    const int y = (g_GameWindow.GetSize().Height / 2) - h;
    g_OptionsConfig = g_ConfigManager;
    g_GumpManager.AddGump(new CGumpOptions(x, y));
}

void CGame::OpenMail()
{
}

void CGame::OpenPartyManifest()
{
    const int x = (g_GameWindow.GetSize().Width / 2) - 272;
    const int y = (g_GameWindow.GetSize().Height / 2) - 240;
    g_GumpManager.AddGump(new CGumpPartyManifest(0, x, y, g_Party.CanLoot));
}

void CGame::OpenResourceTracker()
{
    const int w = (g_ConfigManager.GameWindowWidth / 2);
    const int h = (g_ConfigManager.GameWindowHeight / 2);
    const int x = (g_GameWindow.GetSize().Width / 2) - w;
    const int y = (g_GameWindow.GetSize().Height / 2) - h;
    g_GumpManager.AddGump(new CGumpResourceTracker(x, y));
}

void CGame::OpenProfile(uint32_t serial)
{
    if (serial == 0u)
    {
        serial = g_PlayerSerial;
    }

    CPacketProfileRequest(serial).Send();
}

void CGame::DisconnectGump()
{
    CServer *server = g_ServerList.GetSelectedServer();
    astr_t str = "Disconnected from " + (server != nullptr ? server->Name : "server name...");
    g_Game.CreateTextMessage(TT_SYSTEM, 0, 3, 0x21, str);

    int x = g_ConfigManager.GameWindowX + (g_ConfigManager.GameWindowWidth / 2) - 100;
    int y = g_ConfigManager.GameWindowY + (g_ConfigManager.GameWindowHeight / 2) - 62;

    CGumpNotify *gump =
        new CGumpNotify(x, y, CGumpNotify::ID_GN_STATE_LOGOUT, 200, 125, "Connection lost");

    g_GumpManager.AddGump(gump);

    g_Game.InitScreen(GS_GAME_BLOCKED);
    g_GameBlockedScreen.Code = 0;
}

void CGame::OpenCombatBookGump()
{
    int gameWindowCenterX = (g_ConfigManager.GameWindowX - 4) + g_ConfigManager.GameWindowWidth / 2;
    int gameWindowCenterY =
        (g_ConfigManager.GameWindowY - 4) + g_ConfigManager.GameWindowHeight / 2;

    int x = gameWindowCenterX - 200;
    int y = gameWindowCenterY - 100;

    if (x < 0)
    {
        x = 0;
    }

    if (y < 0)
    {
        y = 0;
    }

    g_GumpManager.AddGump(new CGumpCombatBook(x, y));
}

void CGame::OpenRacialAbilitiesBookGump()
{
    int gameWindowCenterX = (g_ConfigManager.GameWindowX - 4) + g_ConfigManager.GameWindowWidth / 2;
    int gameWindowCenterY =
        (g_ConfigManager.GameWindowY - 4) + g_ConfigManager.GameWindowHeight / 2;

    int x = gameWindowCenterX - 200;
    int y = gameWindowCenterY - 100;

    if (x < 0)
    {
        x = 0;
    }

    if (y < 0)
    {
        y = 0;
    }

    g_GumpManager.AddGump(new CGumpRacialAbilitiesBook(x, y));
}

void CGame::StartReconnect()
{
    if (!g_ConnectionManager.Connected() || g_World == nullptr)
    {
        LogOut();
        g_MainScreen.m_AutoLogin->Checked = true;
        InitScreen(GS_MAIN);
        g_GameWindow.CreateTimer(FASTLOGIN_TIMER_ID, 50);
    }
}
