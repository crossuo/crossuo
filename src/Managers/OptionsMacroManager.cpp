// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "OptionsMacroManager.h"
#include "MacroManager.h"
#include "../Macro.h"

COptionsMacroManager g_OptionsMacroManager;

COptionsMacroManager::COptionsMacroManager()
{
}

COptionsMacroManager::~COptionsMacroManager()
{
}

void COptionsMacroManager::LoadFromMacro()
{
    Clear();

    for (CMacro *obj = (CMacro *)g_MacroManager.m_Items; obj != nullptr; obj = (CMacro *)obj->m_Next) { Add(obj->GetCopy()); }

    if (m_Items == nullptr)
    {
        Add(CMacro::CreateBlankMacro());
    }
}
