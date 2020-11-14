// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include "BaseQueue.h"
#include <xuocore/enumlist.h>
#include "Wisp.h"

struct CMappedFile;

extern const char *s_MacroAction[MSC_TOTAL_COUNT];
extern const char *s_MacroActionName[MC_COUNT];

class CMacroObject : public CBaseQueueItem
{
public:
    MACRO_CODE Code = MC_NONE;
    MACRO_SUB_CODE SubCode = MSC_NONE;
    char HasSubMenu = 0;
    CMacroObject(const MACRO_CODE &code, const MACRO_SUB_CODE &subCode);
    virtual ~CMacroObject() = default;
    virtual bool HaveString() { return false; }
};

class CMacroObjectString : public CMacroObject
{
public:
    astr_t m_String;
    CMacroObjectString(const MACRO_CODE &code, const MACRO_SUB_CODE &subCode, const astr_t &str);
    virtual ~CMacroObjectString() = default;
    virtual bool HaveString() { return true; }
};

class CMacro : public CBaseQueueItem
{
public:
    Keycode Key = 0;
    bool Alt = false;
    bool Ctrl = false;
    bool Shift = false;
    CMacro(Keycode key, bool alt, bool ctrl, bool shift);
    virtual ~CMacro() = default;
    void ChangeObject(CMacroObject *source, CMacroObject *obj);
    void Save(Wisp::CBinaryFileWriter &writer);
    CMacro *GetCopy();
    static CMacro *Load(CMappedFile &file);
    static CMacro *CreateBlankMacro();
    static CMacroObject *CreateMacro(const MACRO_CODE &code);
    static void GetBoundByCode(const MACRO_CODE &code, int &count, int &offset);
};

extern CMacroObject *g_MacroPointer;
