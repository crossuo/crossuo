// MIT License
// Copyright (C) October 2017 Hotride

#pragma once

class CObjectProperty
{
public:
    uint32_t Serial = 0;
    uint32_t Revision = 0;
    wstring Name = {};
    wstring Data = {};

    CObjectProperty() {}
    CObjectProperty(int serial, int revision, const wstring &name, const wstring &data);
    bool Empty();
    wstring CreateTextData(bool extended);
};

typedef map<uint32_t, CObjectProperty> OBJECT_PROPERTIES_MAP;

class CObjectPropertiesManager
{
    uint32_t Timer = 0;

private:
    OBJECT_PROPERTIES_MAP m_Map;
    class CRenderObject *m_Object{ nullptr };

public:
    CObjectPropertiesManager() {}
    virtual ~CObjectPropertiesManager();

    void Reset();
    bool RevisionCheck(int serial, int revision);
    void OnItemClicked(int serial);
    void Display(int serial);
    void Add(int serial, const CObjectProperty &objectProperty);
};

extern CObjectPropertiesManager g_ObjectPropertiesManager;
