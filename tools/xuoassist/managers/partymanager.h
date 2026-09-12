// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/partymanager.h), Qt-diet
// (QString -> std::string).

#ifndef XUOASSIST_PARTYMANAGER_H
#define XUOASSIST_PARTYMANAGER_H

#include "core/global.h"
#include "model/datareader.h"

namespace xuoassist {

struct PARTY_MEMBER_INFO
{
    uint Serial{ 0 };
    int X{ 0 };
    int Y{ 0 };
    uint Map{ 0 };
    string Name{ "" };
};

class CPartyManager
{
    //Серийник лидера пати
    SETGET(uint, Leader, 0)
    //Серийник пригласившего в пати
    SETGET(uint, Inviter, 0)
    //Может ли группа лутать труп игрока
    SETGET(bool, CanLoot, false)

public:
    CPartyManager();

    //Члены группы
    uint Member[10];
    PARTY_MEMBER_INFO m_MemberInfo[10];

    //Обработка пакетов пати
    bool OnPacketReceived(CDataReader &reader);

    //Содержит ли пати игрока с данным серийником
    bool Contains(const uint &serial);

    void SaveMemberInfo(class CGameCharacter *obj);

    void SetMemberInfo(const PARTY_MEMBER_INFO &info);

    //Очистить пати
    void Clear();
};

extern CPartyManager g_PartyManager;

} // namespace xuoassist

#endif // XUOASSIST_PARTYMANAGER_H
