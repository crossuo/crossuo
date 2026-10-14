// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "Waypoints.h"

CWaypoints g_Waypoints;

void CWaypoints::Add(const CWaypoint &waypoint)
{
    Remove(waypoint.Serial);
    m_List.push_back(waypoint);
}

void CWaypoints::Remove(uint32_t serial)
{
    for (size_t i = 0; i < m_List.size();)
    {
        if (m_List[i].Serial == serial)
        {
            m_List.erase(m_List.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

void CWaypoints::Clear()
{
    m_List.clear();
}
