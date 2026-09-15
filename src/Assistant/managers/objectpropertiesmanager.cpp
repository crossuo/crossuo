// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/objectpropertiesmanager.cpp).

#include "objectpropertiesmanager.h"

namespace xuoassist {

CObjectPropertiesManager g_ObjectPropertiesManager;

CObjectPropertiesManager::~CObjectPropertiesManager()
{
    m_Map.clear();
}

string CObjectPropertiesManager::GetData(const uint &serial)
{
    OBJECT_PROPERTIES_MAP::iterator it = m_Map.find(serial);

    if (it == m_Map.end())
    {
        return "";
    }

    return it->second.Data;
}

void CObjectPropertiesManager::Add(const uint &serial, const CObjectProperty &objectProperty)
{
    m_Map[serial] = objectProperty;
}

} // namespace xuoassist
