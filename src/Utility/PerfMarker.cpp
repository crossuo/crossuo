#include "PerfMarker.h"

// #define PERFMARKERS_ENABLED

// TODO renderer add a cmd for this or it won't work unless in immediate mode
// 'label' lifetime is assumed to be valid until the current cmd list is executed
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