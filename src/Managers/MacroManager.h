// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/fs.h>
#include "../BaseQueue.h"
#include "../Platform.h" // Keycode
#include "../Globals.h"  // MACRO_RETURN_CODE

class CMacroObject;

class CMacroManager : public CBaseQueue
{
public:
    bool WaitingBandageTarget = false;
    uint32_t WaitForTargetTimer = 0;
    bool SendNotificationToPlugin = false;

private:
    uint32_t m_NextTimer{ 0 };
    static uint8_t m_SkillIndexTable[24];

    void ProcessSubMenu();
    Keycode ConvertStringToKeyCode(const std::vector<astr_t> &strings);

public:
    CMacroManager() = default;
    virtual ~CMacroManager() = default;

    class CMacro *FindMacro(Keycode key, bool alt, bool ctrl, bool shift);
    bool Convert(const fs_path &path);
    bool Load(const fs_path &path, const fs_path &originalPath);
    void Save(const fs_path &path);
    void LoadFromOptions();
    void ChangePointer(CMacroObject *macro);
    void Execute();
    MACRO_RETURN_CODE Process();
    MACRO_RETURN_CODE Process(CMacroObject *macro);
};

extern CMacroManager g_MacroManager;
