// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/clientmacromanager.h), Qt-diet
// (QStringList -> vector<string>, QList -> vector).

#ifndef XUOASSIST_CLIENTMACROMANAGER_H
#define XUOASSIST_CLIENTMACROMANAGER_H

#include "core/global.h"

namespace xuoassist {

class CClientMacro
{
    string m_Name;

public:
    explicit CClientMacro(const string &name)
        : m_Name(name)
    {
    }

    // SETGET(Name) in the original
    string GetName() const { return m_Name; }
    void SetName(const string &val) { m_Name = val; }

    vector<string> m_Actions;
};

extern vector<CClientMacro> g_ClientMacro;

// Feeding helpers (packetmanager 0xFC OIPMT_MACRO_LIST branch).
void ClientMacroReset();
void ClientMacroInsert(const string &name, const vector<string> &actions);

} // namespace xuoassist

#endif // XUOASSIST_CLIENTMACROMANAGER_H
