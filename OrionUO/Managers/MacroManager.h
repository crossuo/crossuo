// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../BaseQueue.h"

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
    Keycode ConvertStringToKeyCode(const vector<string> &strings);

public:
    CMacroManager();
    virtual ~CMacroManager();

    class CMacro *FindMacro(Keycode key, bool alt, bool ctrl, bool shift);
    bool Convert(const os_path &path);
    bool Load(const os_path &path, const os_path &originalPath);
    void Save(const os_path &path);
    void LoadFromOptions();
    void ChangePointer(CMacroObject *macro);
    void Execute();
    MACRO_RETURN_CODE Process();
    MACRO_RETURN_CODE Process(CMacroObject *macro);
};

extern CMacroManager g_MacroManager;
