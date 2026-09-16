// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/objectpropertiesmanager.h), trimmed
// (maps are std::map, strings are astr_t).

#pragma once

#include <map>

#include "core/global.h"

namespace xuoassist {

class CObjectProperty
{
public:
    uint Revision = 0;
    astr_t Data = "";

    CObjectProperty() {}
    CObjectProperty(const uint &revision, const astr_t &data)
        : Revision(revision)
        , Data(data)
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

    astr_t GetData(const uint &serial);

    void Add(const uint &serial, const CObjectProperty &objectProperty);
};

extern CObjectPropertiesManager g_ObjectPropertiesManager;

} // namespace xuoassist
