// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>
#include <vector>
#include <xuocore/mappedfile.h>

//! A server-sent map marker (world map waypoint)
struct CWaypoint
{
    uint32_t Serial = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    int8_t Z = 0;
    uint8_t MapIndex = 0;
    uint16_t Type = 0;
    bool IgnoreObject = false;
    uint32_t Cliloc = 0;
    wstr_t Name;
};

//! Storage for the waypoints sent by the server (packets 0xE5 / 0xE6)
class CWaypoints
{
    std::vector<CWaypoint> m_List;

public:
    const std::vector<CWaypoint> &List() const { return m_List; }

    //! Add a waypoint, replacing any marker already stored under the same serial
    void Add(const CWaypoint &waypoint);

    //! Drop the waypoint registered under the given serial (if any)
    void Remove(uint32_t serial);

    void Clear();
};

extern CWaypoints g_Waypoints;
