// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gumphook.cpp), verbatim.

#include "gumphook.h"

namespace xuoassist {

CGumpHook::CGumpHook(CGumpHook *original)
{
    if (original != nullptr)
    {
        m_Index = original->GetIndex();

        if (!original->m_Entries.empty())
        {
            for (const CGumpHookEntry &entry : original->m_Entries)
            {
                m_Entries.push_back(entry);
            }
        }

        if (!original->m_Checks.empty())
        {
            for (const CGumpHookCheck &check : original->m_Checks)
            {
                m_Checks.push_back(check);
            }
        }
    }
}

} // namespace xuoassist
