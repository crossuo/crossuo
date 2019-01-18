// MIT License
// Copyright (C) October 2017 Hotride

#include "ObjectPropertiesManager.h"
#include "ConfigManager.h"
#include "GumpManager.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Gumps/GumpProperty.h"
#include "../Gumps/GumpPropertyIcon.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GameCharacter.h"

CObjectPropertiesManager g_ObjectPropertiesManager;

CObjectProperty::CObjectProperty(int serial, int revision, const wstring &name, const wstring &data)
    : Serial(serial)
    , Revision(revision)
    , Name(name)
    , Data(data)
{
}

bool CObjectProperty::Empty()
{
    return ((Name.length() == 0u) && (Data.length() == 0u));
}

wstring CObjectProperty::CreateTextData(bool extended)
{
    CGameObject *obj = g_World->FindWorldObject(Serial);
    bool coloredStartFont = false;
    wstring result = {};

    if (Name.length() != 0u)
    {
        if (obj != nullptr)
        {
            if (!obj->NPC)
            {
                result = L"<basefont color=\"yellow\">";
                coloredStartFont = true;
            }
            else
            {
                CGameCharacter *gc = obj->GameCharacterPtr();
                coloredStartFont = true;

                switch (gc->Notoriety)
                {
                    case NT_INNOCENT:
                    {
                        result = L"<basefont color=\"cyan\">";
                        break;
                    }
                    case NT_SOMEONE_GRAY:
                    case NT_CRIMINAL:
                    {
                        result = L"<basefont color=\"gray\">";
                        break;
                    }
                    case NT_MURDERER:
                    {
                        result = L"<basefont color=\"red\">";
                        break;
                    }
                    case NT_INVULNERABLE:
                    {
                        result = L"<basefont color=\"yellow\">";
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        result += Name;

        if (coloredStartFont)
        {
            result += L"<basefont color=\"#FFFFFFFF\">";
        }
    }

    if (Data.length() != 0u)
    {
        result += L"\n" + Data;
    }
    else if (extended)
    {
        if (result.length() != 0u)
        {
            result += L"\nNo significant properties...";
        }
        else
        {
            result = L"\nNo Data";
        }
    }

    return result;
}

CObjectPropertiesManager::~CObjectPropertiesManager()
{
    m_Map.clear();
}

void CObjectPropertiesManager::Reset()
{
    m_Object = nullptr;
    g_ToolTip.Reset();
}

bool CObjectPropertiesManager::RevisionCheck(int serial, int revision)
{
    OBJECT_PROPERTIES_MAP::iterator it = m_Map.find(serial);

    if (it == m_Map.end())
    {
        return false;
    }

    return (it->second.Revision == revision);
}

void CObjectPropertiesManager::OnItemClicked(int serial)
{
    if (!g_ConfigManager.GetItemPropertiesIcon() || !g_TooltipsEnabled ||
        g_ConfigManager.GetItemPropertiesMode() != OPM_SINGLE_CLICK)
    {
        return;
    }

    OBJECT_PROPERTIES_MAP::iterator it = m_Map.find(serial);

    if (it == m_Map.end() || it->second.Empty())
    {
        return;
    }

    g_GumpManager.CloseGump(0, 0, GT_PROPERTY);
    g_GumpManager.AddGump(new CGumpProperty(it->second.CreateTextData(true)));
}

void CObjectPropertiesManager::Display(int serial)
{
    OBJECT_PROPERTIES_MAP::iterator it = m_Map.find(serial);

    if (it == m_Map.end() || it->second.Empty())
    {
        if (m_Object != nullptr)
        {
            if (g_ConfigManager.GetItemPropertiesMode() == OPM_AT_ICON)
            {
                CGumpPropertyIcon *gump =
                    (CGumpPropertyIcon *)g_GumpManager.UpdateContent(0, 0, GT_PROPERTY_ICON);

                if (gump != nullptr)
                {
                    gump->Object = nullptr;
                }
            }

            Reset();
        }

        return;
    }

    if (!g_ConfigManager.GetItemPropertiesIcon() ||
        g_ConfigManager.GetItemPropertiesMode() == OPM_FOLLOW_MOUSE)
    {
        g_ToolTip.Set(it->second.CreateTextData(false));
        return;
    }
    if (g_ConfigManager.GetItemPropertiesMode() == OPM_SINGLE_CLICK)
    {
        return;
    }

    CGumpPropertyIcon *gump = (CGumpPropertyIcon *)g_GumpManager.GetGump(0, 0, GT_PROPERTY_ICON);

    if (gump == nullptr)
    {
        return;
    }

    bool condition = (g_ConfigManager.GetItemPropertiesMode() == OPM_ALWAYS_UP);

    CRenderObject *object = g_SelectedObject.Object;

    if (object != m_Object)
    {
        m_Object = object;
        Timer = g_Ticks + g_ConfigManager.ToolTipsDelay;
    }

    if (!condition)
    {
        condition = !(Timer > g_Ticks);
    }

    if (condition && gump->Object != m_Object)
    {
        gump->SetTextW(it->second.CreateTextData(true));
        gump->Object = m_Object;
    }
}

void CObjectPropertiesManager::Add(int serial, const CObjectProperty &objectProperty)
{
    m_Map[serial] = objectProperty;
}
