// SPDX-License-Identifier: AGPL-3.0-or-later

// Standalone viewer audio backend (SoLoud), see audio.cpp.

#pragma once

#include <stdint.h>

#include <string>

namespace avaudio
{
// device init: safe to call repeatedly (no-op once initialized)
bool Init();
void Shutdown();

// one sound-effect preview at a time; the returned opaque handle is freed by
// StopSoundPreview (the viewer's Sound tab does exactly that)
void *PlaySoundPreview(uint16_t id, std::string &error);
void StopSoundPreview(void *handle);

// music lane: one track at a time, replaces whatever plays
bool PlayMusic(const char *fileName, int index, bool loop, std::string &error);
void StopMusic();
bool MusicStillPlaying(int index);
int CurrentMusicIndex();
} // namespace avaudio
