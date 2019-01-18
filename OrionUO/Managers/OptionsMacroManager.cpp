// MIT License
// Copyright (C) August 2016 Hotride

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
    DEBUG_TRACE_FUNCTION;
    Clear();

    QFOR(obj, g_MacroManager.m_Items, CMacro *) { Add(obj->GetCopy()); }

    if (m_Items == nullptr)
    {
        Add(CMacro::CreateBlankMacro());
    }
}
