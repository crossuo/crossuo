// MIT License
// Copyright (C) October 2017 Hotride

#pragma once

#include <stdint.h>
#include <common/str.h>
#include <map>

class CRenderObject;

class CObjectProperty
{
public:
    uint32_t Serial = 0;
    uint32_t Revision = 0;
    std::wstring Name = {};
    std::wstring Data = {};

    CObjectProperty() {}
    CObjectProperty(int serial, int revision, const std::wstring &name, const std::wstring &data);
    bool Empty();
    std::wstring CreateTextData(bool extended);
};

using ObjectPropertyMap = std::map<uint32_t, CObjectProperty>;

class CObjectPropertiesManager
{
    uint32_t Timer = 0;

private:
    ObjectPropertyMap m_Map;
    CRenderObject *m_Object = nullptr;

public:
    CObjectPropertiesManager() {}
    virtual ~CObjectPropertiesManager();

    void Reset();
    bool RevisionCheck(int serial, int revision) const;
    void OnItemClicked(int serial);
    void Display(int serial);
    void Add(int serial, const CObjectProperty &objectProperty);
};

extern CObjectPropertiesManager g_ObjectPropertiesManager;
