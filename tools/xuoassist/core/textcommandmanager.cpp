// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/textcommandmanager.cpp) with the
// commandmanager OnCommand* bodies inlined (see textcommandmanager.h).

#include "textcommandmanager.h"
#include "assist.h"
#include "packets.h"
#include "target.h"
#include "assistantevents.h"
#include "../script/scriptgump.h" // the text window (the g_TextDialog port)
#include "../script/journal.h"    // the script journal ring (the g_JournalManager port)
#include "../script/scripthandle.h" // ScriptManager_Terminate (the running-script registry)
#include "../config/assistantconfig.h"
#include "../model/gameobjects/gameworld.h"
#include "../model/gameobjects/gameplayer.h"
#include "../managers/spellmanager.h"
#include "../managers/skillmanager.h"
#include "../managers/menumanager.h"
#include "../managers/gumpmanager.h"

namespace xuoassist {

CTextCommandManager g_TextCommandManager;
COMMANDS_MAP CTextCommandManager::m_Commands;

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CTextCommandManager::CTextCommandManager() {}

CTextCommandManager::~CTextCommandManager() {}

#define COMMAND(name, flags, description)                                                          \
    m_Commands[#name] =                                                                            \
        std::make_pair(CCommand(true, flags, description), &CTextCommandManager::Command##name)

void CTextCommandManager::InitCommands()
{
    COMMAND(info, XAEC_INFORMATION | XAEC_TEXT_WINDOW, "Usage: info [serial=targetRequest]");
    COMMAND(infotile, XAEC_INFORMATION | XAEC_TEXT_WINDOW, "Usage: infotile ['lasttile'=targetRequest]");
    COMMAND(saveconfig, 0, "Usage: saveconfig");
    COMMAND(setdressbag, 0, "Usage: setdressbag [serial=targetRequest]");
    COMMAND(unsetdressbag, 0, "Usage: unsetdressbag");
    COMMAND(getstatus, 0, "Usage: getstatus [serial=self]");
    COMMAND(click, 0, "Usage: click [serial=self]");
    COMMAND(useobject, 0, "Usage: useobject [serial=self]");
    COMMAND(attack, 0, "Usage: attack [serial=self]");
    COMMAND(usetype, XAEC_SEARCH_CONTAINER, "Usage: usetype graphic [color=0xFFFF] [container=self] [recurse=true]");
    COMMAND(usefromground, XAEC_SEARCH_GROUND, "Usage: usefromground graphic [color=0xFFFF] [distance=useObjectsDistance]");
    COMMAND(textopen, XAEC_TEXT_WINDOW, "Usage: textopen");
    COMMAND(textclose, XAEC_TEXT_WINDOW, "Usage: textclose");
    COMMAND(textclear, XAEC_TEXT_WINDOW, "Usage: textclear");
    COMMAND(textprint, XAEC_TEXT_WINDOW, "Usage: textprint 'text'");
    COMMAND(terminate, 0, "Usage: terminate scriptName");
    COMMAND(setlight, 0, "Usage: setlight on/off [value=currentLight]");
    COMMAND(setweather, 0, "Usage: setweather on/off [index=currentWeather] [effectsCount=currentCount] [temperature=currentTemperature]");
    COMMAND(setseason, 0, "Usage: setseason on/off [index=currentIndex] [musicIndex=currentMusic]");
    COMMAND(cast, 0, "Usage: cast spellIndex/spellName");
    COMMAND(useskill, 0, "Usage: useskill skillIndex/skillName");
    COMMAND(helpgump, 0, "Usage: helpgump");
    COMMAND(closeuo, 0, "Usage: closeuo");
    COMMAND(warmode, 0, "Usage: warmode [on/off=switch]");
    COMMAND(morph, XAEC_MORPHING, "Usage: morph [graphic=off]");
    COMMAND(resend, XAEC_RESEND, "Usage: resend");
    COMMAND(sound, 0, "Usage: sound index");
    COMMAND(emoteaction, XAEC_EMOTE_ACTIONS, "Usage: emoteaction actionName");
    COMMAND(hide, XAEC_HIDE, "Usage: hide [serial=target]");
    COMMAND(blockmoving, XAEC_MOVING, "Usage: blockmoving on/off");
    COMMAND(drop, XAEC_MOVE_ITEMS, "Usage: drop serial count x y z");
    COMMAND(drophere, XAEC_MOVE_ITEMS, "Usage: drophere [serial=target] [count=all]");
    COMMAND(moveitem, XAEC_MOVE_ITEMS, "Usage: moveitem serial [count=all] [container=backpack] [x y] [z]");
    COMMAND(setarm, 0, "Usage: setarm setName");
    COMMAND(unsetarm, 0, "Usage: unsetarm setName");
    COMMAND(setdress, 0, "Usage: setdress setName");
    COMMAND(unsetdress, 0, "Usage: unsetdress setName");
    COMMAND(savehotkeys, 0, "Usage: savehotkeys fileName");
    COMMAND(loadhotkeys, 0, "Usage: loadhotkeys fileName");
    COMMAND(track, 0, "Usage: track [on=off] [x=-1 y=-1]");
    COMMAND(infomenu, XAEC_INFORMATION | XAEC_TEXT_WINDOW, "Usage: infomenu");
    COMMAND(showjournal, XAEC_INFORMATION | XAEC_TEXT_WINDOW | XAEC_JOURNAL, "Usage: showjournal [linesCount=maxLines]");
    COMMAND(clearjournal, XAEC_JOURNAL, "Usage: clearjournal ['pattern'] [serial=0xFFFFFFFF] [color=0xFFFF] [type=all]");
    COMMAND(journalignorecase, XAEC_JOURNAL, "Usage: journalignorecase [on=off]");
    COMMAND(ignorereset, XAEC_FIND_TYPE_OR_IGNORE, "Usage: ignorereset");
    COMMAND(infogump, XAEC_INFORMATION | XAEC_TEXT_WINDOW, "Usage: infogump");
    COMMAND(setpvpcaller, 0, "Usage: setpvpcaller [serial=targetRequest]");
}

void CTextCommandManager::DoCommand(const string &text)
{
    if (g_World == nullptr)
    {
        return;
    }

    CTextParser parser(text, " ", "#", "''\"\"()");

    while (!parser.IsEOF())
    {
        vector<string> list = parser.ReadTokens();

        if (list.empty())
        {
            continue;
        }

        string str = list.at(0);

        if (str.length() == 0)
        {
            continue;
        }

        COMMANDS_MAP::iterator command = m_Commands.find(to_lower_copy(str));

        if (command == m_Commands.end())
        {
            g_Assistant.ClientPrint(string("Unknown command: ") + str);
            continue;
        }

        CCommand &cmd = command->second.first;
        CMD_HANDLER handler = command->second.second;

        if (cmd.GetEnabled())
        {
            list.erase(list.begin());
            (this->*handler)(list, cmd);
        }
        else
        {
            g_Assistant.ClientPrint("This command is disabled!");
        }
    }
}

bool CTextCommandManager::RunCommand(const string &name, const COMMAND_ARGS &args)
{
    if (g_World == nullptr)
    {
        return false;
    }

    COMMANDS_MAP::iterator command = m_Commands.find(to_lower_copy(name));

    if (command == m_Commands.end())
    {
        g_Assistant.ClientPrint(string("Unknown command: ") + name);
        return false;
    }

    CCommand &cmd = command->second.first;
    CMD_HANDLER handler = command->second.second;

    if (cmd.GetEnabled())
    {
        (this->*handler)(args, cmd);
        return true;
    }

    g_Assistant.ClientPrint("This command is disabled!");
    return false;
}

void CTextCommandManager::UpdateCommands()
{
    for (COMMANDS_MAP::iterator i = m_Commands.begin(); i != m_Commands.end(); ++i)
    {
        CCommand &command = i->second.first;
        uint64_t flags = command.GetFlags();

        if ((flags == 0u) || ((g_EnabledCommands & flags) == flags))
        {
            command.SetEnabled(true);
        }
        else
        {
            command.SetEnabled(false);
        }
    }
}

ushort CTextCommandManager::GetFindFlags(const string &text)
{
    // Was CCommandManager::GetFindFlags.
    ushort flags = FTF_NORMAL;
    ushort flagsType = FTF_NORMAL;

    CTextParser parser(text, "|", "", "");

    vector<string> list = parser.ReadTokens();

    for (const string &str : list)
    {
        if (str == "fast")
        {
            flags = FTF_FAST;
        }
        else if (str == "near")
        {
            flags = FTF_NEAREST;
        }
        else if (str == "mobile")
        {
            flagsType |= FTF_MOBILE;
        }
        else if (str == "item")
        {
            flagsType = FTF_ITEM;
        }
        else if (str == "human")
        {
            flagsType |= (FTF_HUMAN | FTF_MOBILE);
        }
        else if (str == "live")
        {
            flagsType |= (FTF_LIVE | FTF_MOBILE);
        }
        else if (str == "dead")
        {
            flagsType |= (FTF_DEAD | FTF_MOBILE);
        }
        else if (str == "injured")
        {
            flagsType |= (FTF_INJURED | FTF_MOBILE);
        }
        else if (str == "next")
        {
            flagsType |= (FTF_NEXT | FTF_FAST);
        }
        else if (str == "ignorefriends")
        {
            flagsType |= FTF_IGNORE_FRIENDS;
        }
        else if (str == "ignoreenemies")
        {
            flagsType |= FTF_IGNORE_ENEMIES;
        }
    }

    return flags | flagsType;
}

uchar CTextCommandManager::GetJournalFlags(const string &text)
{
    // Was CCommandManager::GetJournalFlags.
    uchar flags = 0;

    CTextParser parser(text, "|", "", "");

    vector<string> list = parser.ReadTokens();

    for (const string &str : list)
    {
        if (str == "my" || str == "self")
        {
            flags |= JMT_MY;
        }
        else if (str == "sys" || str == "system")
        {
            flags |= JMT_SYSTEM;
        }
    }

    return flags;
}

uchar CTextCommandManager::GetSkillsFlags(const string &text)
{
    // Was CCommandManager::GetSkillsFlags.
    uchar flags = SF_REAL;

    CTextParser parser(text, "|", "", "");

    vector<string> list = parser.ReadTokens();

    for (const string &str : list)
    {
        if (str == "base")
        {
            flags = SF_BASE;
        }
        else if (str == "cap")
        {
            flags = SF_CAP;
        }
        else if (str == "lock")
        {
            flags = SF_LOCK;
        }
    }

    return flags;
}

void CTextCommandManager::CommandTok(COMMAND_ARGS &args, const string &in)
{
    (void)args;
    (void)in;
}

#define HCOMMAND(name)                                                                             \
    void CTextCommandManager::Command##name(const COMMAND_ARGS &args, const CCommand &command)

HCOMMAND(info)
{
    (void)command;
    string serial = args.empty() ? "" : args[0];

    if (g_World == nullptr || !EnabledCommandInformation())
    {
        return;
    }

    CGameObject *obj = nullptr;

    if (serial.length() != 0)
    {
        obj = g_World->FindWorldObject(CAssistant::TextToSerial(serial));
    }

    if (obj != nullptr)
    {
        g_Assistant.DumpObjectInfo(obj);
    }
    else
    {
        g_TargetHandler = &CAssistant::HandleTargetInfo;
        g_Assistant.RequestTarget("Select a object for obtain info");
    }
}

HCOMMAND(infotile)
{
    (void)command;
    string lasttile = args.empty() ? "" : args[0];

    if (g_World == nullptr || !EnabledCommandInformation())
    {
        return;
    }

    if (to_lower_copy(lasttile) == "lasttile")
    {
        g_Target.DumpLastTileInfo();
    }
    else
    {
        g_TargetHandler = &CAssistant::HandleTargetInfoTile;
        g_Assistant.RequestTarget("Select a tile for obtain info");
    }
}

HCOMMAND(saveconfig)
{
    (void)command;
    (void)args;

    if (g_World == nullptr)
    {
        return;
    }

    g_AssistantConfig.SaveProfile();
}

HCOMMAND(setdressbag)
{
    (void)command;
    string serial = args.empty() ? "" : args[0];

    if (g_World == nullptr)
    {
        return;
    }

    if (serial.length() != 0)
    {
        g_Assistant.SetDressBag(CAssistant::TextToSerial(serial));
    }
    else
    {
        g_TargetHandler = &CAssistant::HandleTargetDressBag;
        g_Assistant.RequestTarget("Select a dress bag.");
    }
}

HCOMMAND(unsetdressbag)
{
    (void)args;
    (void)command;

    if (g_World == nullptr)
    {
        return;
    }

    g_Assistant.SetDressBag(0);
}

HCOMMAND(getstatus)
{
    (void)command;
    string serial = "self";

    if (!args.empty())
    {
        serial = args[0];
    }

    if (g_World == nullptr || serial.length() == 0)
    {
        return;
    }

    g_Assistant.GetStatus(CAssistant::TextToSerial(serial));
}

HCOMMAND(click)
{
    (void)command;
    string serial = "self";

    if (!args.empty())
    {
        serial = args[0];
    }

    if (g_World == nullptr || serial.length() == 0)
    {
        return;
    }

    g_Assistant.Click(CAssistant::TextToSerial(serial));
}

HCOMMAND(useobject)
{
    (void)command;
    string serial = "self";

    if (!args.empty())
    {
        serial = args[0];
    }

    if (g_World == nullptr || serial.length() == 0)
    {
        return;
    }

    g_Assistant.DoubleClick(CAssistant::TextToSerial(serial));
}

HCOMMAND(attack)
{
    (void)command;

    if (g_World == nullptr || args.empty() || args[0].length() == 0)
    {
        return;
    }

    g_Assistant.Attack(CAssistant::TextToSerial(args[0]));
}

HCOMMAND(usetype)
{
    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    string color = "0xFFFF";
    string container = "self";
    bool recurseSearch = true;

    if (size > 1)
    {
        color = args[1];

        if (size > 2)
        {
            container = args[2];

            if (container == "ground")
            {
                recurseSearch = false;
            }
            else if (size > 3)
            {
                recurseSearch = CAssistant::RawStringToBool(args[3]);
            }
        }
    }

    if (g_World == nullptr)
    {
        return;
    }

    vector<string> list;

    CGameObject *found = g_Assistant.FindType(
        vector<string>{ args[0] },
        vector<string>{ color },
        container,
        "usedistance",
        GetFindFlags("fast"),
        "",
        recurseSearch,
        list);

    if (found != nullptr)
    {
        g_Assistant.DoubleClick(found->GetSerial());
    }
}

HCOMMAND(usefromground)
{
    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    string distance = "usedistance";
    string flags = "";

    if (size > 2)
    {
        distance = args[2];
    }

    if (g_World == nullptr)
    {
        return;
    }

    vector<string> list;

    CGameObject *found = g_Assistant.FindType(
        vector<string>{ args[0] },
        vector<string>{ size > 1 ? args[1] : string("0xFFFF") },
        "ground",
        distance,
        GetFindFlags(flags),
        "",
        false,
        list);

    if (found != nullptr)
    {
        g_Assistant.DoubleClick(found->GetSerial());
    }
}

HCOMMAND(textopen)
{
    (void)command;
    (void)args;

    if (!EnabledCommandTextWindow())
    {
        return;
    }

    // the g_TextDialog->show() port (scriptgump.cpp, the 4b-2 text window)
    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

HCOMMAND(textclose)
{
    (void)command;
    (void)args;

    if (!EnabledCommandTextWindow())
    {
        return;
    }

    // the g_TextDialog->hide() port
    ScriptTextWindow_Command(TW_CLOSE, "", 0, 0, "");
}

HCOMMAND(textclear)
{
    (void)command;
    (void)args;

    if (!EnabledCommandTextWindow())
    {
        return;
    }

    // the g_TextDialog->ClearText() port
    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
}

HCOMMAND(textprint)
{
    (void)command;

    if (!EnabledCommandTextWindow())
    {
        return;
    }

    string text = "";
    bool first = true;

    for (const string &str : args)
    {
        if (!first)
        {
            text += " ";
        }
        first = false;
        text += str;
    }

    // the g_TextDialog->AddText(text) port (scriptgump.cpp)
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");
}

HCOMMAND(terminate)
{
    (void)command;

    if (args.empty() || args[0].length() == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
    }

    // was g_TabScripts->TerminateScript(args[0], functionsSave)
    // (textcommandmanager.cpp:425): the second argument is the "|"-separated
    // keep-list for the "all" form; ScriptManager_Terminate owns both.
    if (!args.empty())
    {
        ScriptManager_Terminate(args[0], args.size() > 1 ? args[1] : string());
    }
}

HCOMMAND(setlight)
{
    if (!g_AssistantConfig.LightFilter() || g_World == nullptr ||
        !EnabledFeatureLightFilter())
    {
        return;
    }

    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    bool on = CAssistant::RawStringToBool(args[0]);
    uchar value = 0;

    if (on)
    {
        if (size > 1)
        {
            value = (uchar)atoi(args[1].c_str());
        }
        else
        {
            value = g_LightLevel;
        }
    }

    CPacketLight(value).SendClient();
}

HCOMMAND(setweather)
{
    if (!g_AssistantConfig.WeatherFilter() || g_World == nullptr ||
        !EnabledFeatureWeatherFilter())
    {
        return;
    }

    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    WEATHER_DATA weather = g_Weather;
    bool on = CAssistant::RawStringToBool(args[0]);

    if (!on)
    {
        weather.Type = 0xFF;
    }
    else if (size > 1)
    {
        weather.Type = (uchar)atoi(args[1].c_str());

        if (size > 2)
        {
            weather.Count = (uchar)atoi(args[2].c_str());

            if (size > 3)
            {
                weather.Temperature = (uchar)atoi(args[3].c_str());
            }
        }
    }

    CPacketWeather(weather).SendClient();
}

HCOMMAND(setseason)
{
    if (!g_AssistantConfig.SeasonFilter() || g_World == nullptr ||
        !EnabledFeatureSeasonFilter())
    {
        return;
    }

    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    SEASON_DATA season = g_Season;
    bool on = CAssistant::RawStringToBool(args[0]);

    if (!on)
    {
        season.Index = ST_SUMMER;
    }
    else if (size > 1)
    {
        season.Index = (uchar)atoi(args[1].c_str());

        if (size > 2)
        {
            season.Music = (uchar)atoi(args[2].c_str());
        }
    }

    CPacketSeason(season).SendClient();
}

HCOMMAND(cast)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    g_SpellManager.Cast(args[0]);
}

HCOMMAND(useskill)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    g_SkillManager.Use(args[0]);
}

HCOMMAND(helpgump)
{
    (void)args;
    (void)command;

    if (g_World == nullptr)
    {
        return;
    }

    CPacketHelpRequest().SendServer();
}

HCOMMAND(closeuo)
{
    (void)args;
    (void)command;

    if (g_World == nullptr)
    {
        return;
    }

    // was SendMessage(hwnd, WM_CLOSE) - the assistant plugin asked the host
    // window to close; in-process the request goes through the events seam
    // and the UI sink pushes the client's own quit event (CWindow::Destroy).
    g_AssistantEvents->OnClientCloseRequest();
}

HCOMMAND(warmode)
{
    (void)command;

    if (g_World == nullptr)
    {
        return;
    }

    uint state = 2;

    if (!args.empty())
    {
        state = (uint)CAssistant::RawStringToBool(args[0]);
    }

    uchar warmode = (uchar)!g_Player->GetWarmode();

    if (state < 2)
    {
        warmode = (uchar)state;
    }

    CPacketChangeWarmode(warmode).SendServer();
}

HCOMMAND(morph)
{
    (void)command;

    if (g_World == nullptr || !EnabledCommandMorphing())
    {
        return;
    }

    ushort graphic = 0;

    if (!args.empty())
    {
        graphic = CAssistant::TextToGraphic(args[0]);
    }

    if (graphic == 0u)
    {
        g_MorphGraphic = 0;
        graphic = g_OriginalGraphic;
    }
    else
    {
        g_MorphGraphic = graphic;
    }

    g_SetInt(VKI_SET_PLAYER_GRAPHIC, graphic);
}

HCOMMAND(resend)
{
    (void)args;
    (void)command;

    if (g_World == nullptr || !EnabledCommandResend())
    {
        return;
    }

    static uint lastResendTime = g_Ticks;
    uint currentTime = g_Ticks;

    if (lastResendTime > currentTime)
    {
        return;
    }

    lastResendTime = currentTime + 10000;

    CPacketResend().SendServer();
}

HCOMMAND(sound)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    CPacketPlaySound((ushort)CAssistant::TextToGraphic(args[0]), g_Player->GetX(), g_Player->GetY(), g_Player->GetZ())
        .SendClient();
}

HCOMMAND(emoteaction)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr || !EnabledCommandEmoteActions())
    {
        return;
    }

    CPacketEmoteAction(args[0].c_str()).SendServer();
}

HCOMMAND(hide)
{
    (void)command;

    if (g_World == nullptr || !EnabledCommandHide())
    {
        return;
    }

    string serial = args.empty() ? "" : args[0];

    if (serial.length() != 0)
    {
        CPacketHideObject(CAssistant::TextToSerial(serial)).SendClient();
    }
    else
    {
        g_TargetHandler = &CAssistant::HandleTargetHide;
        g_Assistant.RequestTarget("Select a object for hide");
    }
}

HCOMMAND(blockmoving)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr || !EnabledCommandMoving())
    {
        return;
    }

    g_SetInt(VKI_BLOCK_MOVING, (int)CAssistant::RawStringToBool(args[0]));
}

HCOMMAND(drop)
{
    (void)command;

    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr || !EnabledCommandMoveItems())
    {
        return;
    }

    g_MoveItemData.Reset();

    string serial = args[0];

    int count = 0;
    ushort x = 0xFFFF;
    ushort y = 0xFFFF;
    char z = -1;

    if (size > 1)
    {
        count = atoi(args[1].c_str());

        if (size > 3)
        {
            x = (ushort)atoi(args[2].c_str());
            y = (ushort)atoi(args[3].c_str());

            if (size > 4)
            {
                z = (char)atoi(args[4].c_str());
            }
        }
    }

    if (serial.length() == 0)
    {
        g_TargetHandler = &CAssistant::HandleTargetDrop;
        g_Assistant.RequestTarget("Select a object for drop");
    }
    else
    {
        CGameItem *item = g_World->FindWorldItem(CAssistant::TextToSerial(serial));

        if (item != nullptr)
        {
            g_MoveItemData.Reset();

            g_MoveItemData.Count = count;

            if (x != 0xFFFF)
            {
                g_MoveItemData.X = x;
            }
            else
            {
                g_MoveItemData.X = g_Player->GetX();
            }

            if (y != 0xFFFF)
            {
                g_MoveItemData.Y = y;
            }
            else
            {
                g_MoveItemData.Y = g_Player->GetY();
            }

            if ((uchar)z != 0xFF)
            {
                g_MoveItemData.Z = z;
            }
            else
            {
                g_MoveItemData.Z = g_Player->GetZ();
            }

            g_Assistant.MoveItem(item);
        }
    }
}

HCOMMAND(drophere)
{
    (void)command;

    int size = (int)args.size();

    string serial = size != 0 ? args[0] : "";
    int count = 0;

    if (size != 0 && size > 1)
    {
        count = atoi(args[1].c_str());
    }

    if (g_World == nullptr || !EnabledCommandMoveItems())
    {
        return;
    }

    if (serial.length() == 0)
    {
        g_TargetHandler = &CAssistant::HandleTargetDrop;
        g_Assistant.RequestTarget("Select a object for drop");
    }
    else
    {
        CGameItem *item = g_World->FindWorldItem(CAssistant::TextToSerial(serial));

        if (item != nullptr)
        {
            g_MoveItemData.Reset();

            g_MoveItemData.Count = count;
            g_MoveItemData.X = g_Player->GetX();
            g_MoveItemData.Y = g_Player->GetY();
            g_MoveItemData.Z = g_Player->GetZ();

            g_Assistant.MoveItem(item);
        }
    }
}

HCOMMAND(moveitem)
{
    (void)command;

    int size = (int)args.size();

    string serial = "";
    string container = "backpack";
    int count = 0;
    ushort x = 0xFFFF;
    ushort y = 0xFFFF;
    char z = -1;

    g_MoveItemData.Reset();
    g_MoveItemData.Container = g_Backpack;

    if (size != 0)
    {
        serial = args[0];

        if (size > 1)
        {
            count = atoi(args[1].c_str());

            if (size > 2)
            {
                container = args[2];

                if (size > 4)
                {
                    x = (ushort)atoi(args[3].c_str());
                    y = (ushort)atoi(args[4].c_str());

                    if (size > 5)
                    {
                        z = (char)atoi(args[5].c_str());
                    }
                }
            }
        }
    }

    if (g_World == nullptr || !EnabledCommandMoveItems())
    {
        return;
    }

    if (serial.length() == 0)
    {
        g_TargetHandler = &CAssistant::HandleTargetDrop;
        g_Assistant.RequestTarget("Select a object for move item");
    }
    else
    {
        CGameItem *item = g_World->FindWorldItem(CAssistant::TextToSerial(serial));

        if (item != nullptr)
        {
            g_MoveItemData.Reset();

            g_MoveItemData.Count = count;
            g_MoveItemData.Container = CAssistant::TextToSerial(container);
            g_MoveItemData.X = x;
            g_MoveItemData.Y = y;
            g_MoveItemData.Z = z;

            g_Assistant.MoveItem(item);
        }
    }
}

HCOMMAND(setarm)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    CDressSetEntry *list = g_AssistantConfig.GetDressSet(to_lower_copy(args[0]));

    if (list == nullptr)
    {
        list = g_AssistantConfig.AddDressSet(args[0]);
    }

    g_AssistantConfig.ClearDressSet(list);

    if (g_Player != nullptr)
    {
        QFOR(item, g_Player->m_Items, CGameItem *)
        {
            int layer = item->GetEquipLayer();

            if ((layer != 0) && layer <= OL_2_HAND)
            {
                (*list).m_Items[layer] = item->GetSerial();
            }
        }
    }
}

HCOMMAND(unsetarm)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    CDressSetEntry *list = g_AssistantConfig.GetDressSet(to_lower_copy(args[0]));

    if (list != nullptr)
    {
        g_AssistantConfig.ClearDressSet(list);
    }
}

HCOMMAND(setdress)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    CDressSetEntry *list = g_AssistantConfig.GetDressSet(to_lower_copy(args[0]));

    if (list == nullptr)
    {
        list = g_AssistantConfig.AddDressSet(args[0]);
    }

    g_AssistantConfig.ClearDressSet(list);

    if (g_Player != nullptr)
    {
        QFOR(item, g_Player->m_Items, CGameItem *)
        {
            int layer = item->GetEquipLayer();

            if (layer > OL_2_HAND && layer < OL_MOUNT && layer != OL_BACKPACK && g_LayerSafe[layer])
            {
                (*list).m_Items[layer] = item->GetSerial();
            }
        }
    }
}

HCOMMAND(unsetdress)
{
    (void)command;

    if (args.empty())
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    CDressSetEntry *list = g_AssistantConfig.GetDressSet(to_lower_copy(args[0]));

    if (list != nullptr)
    {
        g_AssistantConfig.ClearDressSet(list);
    }
}

HCOMMAND(savehotkeys)
{
    (void)command;

    int size = (int)args.size();

    if (size == 0 || args[0].length() == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    // was g_HotkeysForm->Save(path): the hotkey store is the profile's
    // Hotkeys.xml (the same byte-compatible records); the explicit path
    // argument is not honored in-client.
    // DEVIATION: the save target is the profile file, not a chosen path.
    g_AssistantConfig.SaveHotkeys();
    g_Assistant.ClientPrint("Hotkeys have been saved.");
}

HCOMMAND(loadhotkeys)
{
    (void)command;

    int size = (int)args.size();

    if (size == 0 || args[0].length() == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    // was g_HotkeysForm->Load(path); same profile-file deviation as above.
    // DEVIATION: the load source is the profile file, not a chosen path.
    g_AssistantConfig.LoadHotkeys();
    g_Assistant.ClientPrint("Hotkeys have been loaded.");
}

HCOMMAND(track)
{
    (void)command;

    int size = (int)args.size();

    if (size == 0)
    {
        g_Assistant.ClientPrintDebugLevel1(command.GetDescription());
        return;
    }

    if (g_World == nullptr)
    {
        return;
    }

    bool on = CAssistant::RawStringToBool(args[0]);

    short x = -1;
    short y = -1;

    if (!on)
    {
        x = 0;
        y = 0;
    }
    else if (size > 2)
    {
        x = (short)atoi(args[1].c_str());
        y = (short)atoi(args[2].c_str());
    }

    g_Assistant.Track(on, x, y);
}

HCOMMAND(infomenu)
{
    (void)args;
    (void)command;

    if (g_World == nullptr || !EnabledCommandInformation())
    {
        return;
    }

    g_MenuManager.InfoMenu("lastmenu");
}

HCOMMAND(showjournal)
{
    (void)command;

    int lines = args.empty() ? -1 : atoi(args[0].c_str());

    if (g_World == nullptr || !EnabledCommandInformation() || !EnabledCommandJournal())
    {
        return;
    }

    // was g_JournalManager.ShowLines(lines) (journalmanager.cpp:175): dump
    // the ring into the text window (the g_TextDialog port) and show it
    const vector<CJournalMessage> &list = ScriptJournal_Messages();
    const int size = (int)list.size();

    int i = 0;
    int end = size;
    if (lines > 0)
    {
        i = size - lines;
        if (i < 0)
        {
            i = 0;
        }
        end = i + lines > size ? size : i + lines;
    }

    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");

    char text[64] = { 0 };
    snprintf(text, sizeof(text), "Journal lines[%i]:", end - i);
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");

    for (; i < end; i++)
    {
        const CJournalMessage &item = list[i];

        string line = "";
        if ((item.GetFlags() & JMT_SYSTEM) != 0)
        {
            line = "sys ";
        }
        else if ((item.GetFlags() & JMT_MY) != 0)
        {
            line = "my ";
        }

        char buf[512] = { 0 };
        snprintf(
            buf,
            sizeof(buf),
            "[s=0x%08X c=0x%04X]: %s",
            item.GetSerial(),
            item.GetColor(),
            item.GetText().c_str());
        line += buf;

        ScriptTextWindow_Command(TW_PRINT, line, 0, 0, "");
    }

    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

HCOMMAND(clearjournal)
{
    (void)command;

    string pattern = "";
    string serial = "0xFFFFFFFF";
    ushort color = 0xFFFF;
    string type = "";

    int size = (int)args.size();

    if (size != 0)
    {
        pattern = args[0];

        if (size > 1)
        {
            serial = args[1];

            if (size > 2)
            {
                color = CAssistant::TextToGraphic(args[2]);

                if (size > 3)
                {
                    type = args[3];
                }
            }
        }
    }

    if (g_World == nullptr || !EnabledCommandJournal())
    {
        return;
    }

    // was g_JournalManager.Clear(pattern, serial, color, type)
    // (journalmanager.cpp:96): one consume-pass over the filters, plus the
    // full wipe when the pattern is empty and nothing matched.
    CJournalMessage *hit =
        ScriptJournal_Find(pattern, CAssistant::TextToSerial(serial), color,
                           GetJournalFlags(to_lower_copy(type)), 0, 0, true);
    delete hit;

    if (hit == nullptr && pattern.length() == 0)
    {
        ScriptJournal_Clear();
    }
}

HCOMMAND(journalignorecase)
{
    (void)command;

    if (g_World == nullptr || !EnabledCommandJournal())
    {
        return;
    }

    bool ignoreCase = args.empty() ? false : CAssistant::RawStringToBool(args[0]);

    // was g_JournalManager.SetIgnoreCase(on)
    ScriptJournal_SetIgnoreCase(ignoreCase);
}

HCOMMAND(ignorereset)
{
    (void)args;
    (void)command;

    if (g_World == nullptr || !EnabledCommandFindTypeOrIgnore())
    {
        return;
    }

    g_World->IgnoreReset();
}

HCOMMAND(infogump)
{
    (void)args;
    (void)command;

    if (g_World == nullptr || !EnabledCommandInformation())
    {
        return;
    }

    g_GumpManager.InfoGump("lastgump");
}

HCOMMAND(setpvpcaller)
{
    (void)command;
    string serial = args.empty() ? "" : args[0];

    if (serial.length() != 0)
    {
        g_SetInt(VKI_SET_PVPCALLER, (int)CAssistant::TextToSerial(serial));
    }
    else
    {
        g_TargetHandler = &CAssistant::HandleSetPVPCaller;
        g_Assistant.RequestTarget("Set/Unset a target as a caller");
    }
}

} // namespace xuoassist
