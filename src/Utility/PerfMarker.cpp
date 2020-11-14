// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#include "PerfMarker.h"

// #define PERFMARKERS_ENABLED

// TODO renderer add a cmd for this or it won't work unless in immediate mode
// 'label' lifetime is assumed to be valid until the current cmd list is executed
static void PushGPUMarker(const char *label)
{
#ifdef PERFMARKERS_ENABLED
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, OGL_USERPERFMARKERS_ID, -1, label);
#endif
}

static void PopGPUMarker()
{
#ifdef PERFMARKERS_ENABLED
    glPopDebugGroup();
#endif
}

void PushPerfMarker(const char *label)
{
#ifdef PERFMARKERS_ENABLED
    PushGPUMarker(label);
#endif
}

void PopPerfMarker()
{
#ifndef PERFMARKERS_ENABLED
#else
    PopGPUMarker();
#endif
}