#include "PerfMarker.h"

#define PERFMARKERS_ENABLED

static void PushGPUMarker(const char *label)
{
#ifndef PERFMARKERS_ENABLED
    UNUSED(label);
#else
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, OGL_USERPERFMARKERS_ID, -1, label);
#endif
}

static void PopGPUMarker()
{
#ifndef PERFMARKERS_ENABLED
#else
    glPopDebugGroup();
#endif
}

void PushPerfMarker(const char *label)
{
#ifndef PERFMARKERS_ENABLED
    UNUSED(label);
#else
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