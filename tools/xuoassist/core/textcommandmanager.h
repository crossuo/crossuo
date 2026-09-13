// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/textcommandmanager.h + script/command.h),
// Qt-diet. The command table, names, flags and usage strings are identical.
// The Qt handlers delegated to the CCommandManager signal hub (the QObject
// marshaling layer the plan REPLACES, §4.1); the direct-call bodies of those
// OnCommand* slots are inlined into the handlers below so no signal hub is
// ported. Bodies that belong to a later round (hotkeys Phase 3, scripts
// Phase 4, journal Phase 4, text window Phase 2) keep their behavior
// documented as PORT-TODO(phase-N) markers.

#ifndef XUOASSIST_TEXTCOMMANDMANAGER_H
#define XUOASSIST_TEXTCOMMANDMANAGER_H

#include "global.h"
#include "textparser.h"
#include <map>
#include <utility>

namespace xuoassist {

typedef vector<string> COMMAND_ARGS;

// From the Qt app's managers/commandmanager.h (the flag vocabulary of the
// skill-value command API; survives the replaced signal hub).
enum SKILLS_FLAGS
{
    SF_REAL = 0x00,
    SF_BASE = 0x02,
    SF_CAP = 0x03,
    SF_LOCK = 0x04
};

class CCommand
{
    SETGET(bool, Enabled, false)
    SETGET(string, Description, "")
    SETGET(uint64_t, Flags, 0)

public:
    CCommand() {}
    CCommand(const bool &enabled, const uint64_t &flags, const string &description)
        : m_Enabled(enabled)
        , m_Description(description)
        , m_Flags(flags)
    {
    }
    ~CCommand() {}
};

class CTextCommandManager;
typedef void (CTextCommandManager::*CMD_HANDLER)(const COMMAND_ARGS &, const CCommand &);
typedef std::map<string, std::pair<CCommand, CMD_HANDLER>> COMMANDS_MAP;

#define HANDLER_COMMAND(name) void Command##name(const COMMAND_ARGS &args, const CCommand &command)

class CTextCommandManager
{
private:
    static COMMANDS_MAP m_Commands;

    void CommandTok(COMMAND_ARGS &args, const string &in);

public:
    CTextCommandManager();
    ~CTextCommandManager();

    static void InitCommands();

    void DoCommand(const string &text);

    // Runs one command by name with pre-split args (the script runtime's
    // marshaled [Q] calls; the DoCommand tail after CommandTok's parse).
    // Returns false for unknown commands (the script layer logs that).
    bool RunCommand(const string &name, const COMMAND_ARGS &args);

    void UpdateCommands();

    HANDLER_COMMAND(info);
    HANDLER_COMMAND(infotile);
    HANDLER_COMMAND(saveconfig);
    HANDLER_COMMAND(setdressbag);
    HANDLER_COMMAND(unsetdressbag);
    HANDLER_COMMAND(getstatus);
    HANDLER_COMMAND(click);
    HANDLER_COMMAND(useobject);
    HANDLER_COMMAND(attack);
    HANDLER_COMMAND(usetype);
    HANDLER_COMMAND(usefromground);
    HANDLER_COMMAND(textopen);
    HANDLER_COMMAND(textclose);
    HANDLER_COMMAND(textclear);
    HANDLER_COMMAND(textprint);
    HANDLER_COMMAND(terminate);
    HANDLER_COMMAND(setlight);
    HANDLER_COMMAND(setweather);
    HANDLER_COMMAND(setseason);
    HANDLER_COMMAND(cast);
    HANDLER_COMMAND(useskill);
    HANDLER_COMMAND(helpgump);
    HANDLER_COMMAND(closeuo);
    HANDLER_COMMAND(warmode);
    HANDLER_COMMAND(morph);
    HANDLER_COMMAND(resend);
    HANDLER_COMMAND(sound);
    HANDLER_COMMAND(emoteaction);
    HANDLER_COMMAND(hide);
    HANDLER_COMMAND(blockmoving);
    HANDLER_COMMAND(drop);
    HANDLER_COMMAND(drophere);
    HANDLER_COMMAND(moveitem);
    HANDLER_COMMAND(setarm);
    HANDLER_COMMAND(unsetarm);
    HANDLER_COMMAND(setdress);
    HANDLER_COMMAND(unsetdress);
    HANDLER_COMMAND(savehotkeys);
    HANDLER_COMMAND(loadhotkeys);
    HANDLER_COMMAND(track);
    HANDLER_COMMAND(infomenu);
    HANDLER_COMMAND(showjournal);
    HANDLER_COMMAND(clearjournal);
    HANDLER_COMMAND(journalignorecase);
    HANDLER_COMMAND(ignorereset);
    HANDLER_COMMAND(infogump);
    HANDLER_COMMAND(setpvpcaller);

    // Command-body helpers moved out of the replaced CCommandManager
    // (commandmanager.cpp:493-583 GetFindFlags / GetJournalFlags /
    // GetSkillsFlags); used by the handlers above.
    static ushort GetFindFlags(const string &text);
    static uchar GetJournalFlags(const string &text);
    static uchar GetSkillsFlags(const string &text);
};

extern CTextCommandManager g_TextCommandManager;

} // namespace xuoassist

#endif // XUOASSIST_TEXTCOMMANDMANAGER_H
