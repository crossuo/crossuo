#pragma once

#define OGL_USERPERFMARKERS_ID (0xbabebabe)

void PushPerfMarker(const char *label);
void PopPerfMarker();

struct ScopedPerfMarker
{
    ScopedPerfMarker(const char *label) { PushPerfMarker(label); }

    ~ScopedPerfMarker() { PopPerfMarker(); }
};