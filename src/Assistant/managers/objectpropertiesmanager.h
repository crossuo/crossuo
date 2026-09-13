// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/objectpropertiesmanager.h), trimmed
// (maps are std::map, strings are std::string).

#pragma once

#include <map>

#include "core/global.h"

namespace xuoassist {

class CObjectProperty
{
    SETGET(uint, Revision, 0)
    SETGET(string, Data, "")

public:
    CObjectProperty() {}
    CObjectProperty(const uint &revision, const string &data)
        : m_Revision(revision)
        , m_Data(data)
    {
    }
};

typedef std::map<uint, CObjectProperty> OBJECT_PROPERTIES_MAP;

class CObjectPropertiesManager
{
private:
    OBJECT_PROPERTIES_MAP m_Map;

public:
    CObjectPropertiesManager() {}
    virtual ~CObjectPropertiesManager();

    string GetData(const uint &serial);

    void Add(const uint &serial, const CObjectProperty &objectProperty);
};

extern CObjectPropertiesManager g_ObjectPropertiesManager;

} // namespace xuoassist

