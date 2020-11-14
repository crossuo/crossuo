// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#pragma once

#define OGL_USERPERFMARKERS_ID (0xbabebabe)

void PushPerfMarker(const char *label);
void PopPerfMarker();

struct ScopedPerfMarker
{
    ScopedPerfMarker(const char *label) { PushPerfMarker(label); }

    ~ScopedPerfMarker() { PopPerfMarker(); }
};