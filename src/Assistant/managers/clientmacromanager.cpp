// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/clientmacromanager.cpp). The cache is fed
// from the client's 0xFC OIPMT_MACRO_LIST packet (packetmanager).

#include "clientmacromanager.h"

namespace xuoassist {

vector<CClientMacro> g_ClientMacro;

void ClientMacroReset() { g_ClientMacro.clear(); }

void ClientMacroInsert(const string &name, const vector<string> &actions)
{
    CClientMacro macro(name);
    macro.m_Actions = actions;
    g_ClientMacro.push_back(macro);
}

} // namespace xuoassist
