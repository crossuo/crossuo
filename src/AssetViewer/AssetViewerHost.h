// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Asset viewer host abstraction: the few things the viewer cannot own itself
// (the shared music lane, the sound preview stream and where the data came
// from) are provided by whoever embeds the viewer. Two hosts exist:
//
//  - the client (src/AssetViewer/AssetViewerClientHost.cpp): the viewer runs
//    inside the game on the client's SoundManager, and its music stop must
//    never silence music the game started;
//  - the standalone app (tools/assetviewer): the viewer is the only sound
//    owner and loads its own data folder.
//
// Everything else the viewer needs comes straight from xuocore (g_Index,
// g_Data, g_FileManager) and its own texture cache, so both hosts decode the
// same bytes with the same rules.

#if defined(XUO_ASSISTANT)

#include <stdint.h>

#include <string>

// --- audio -------------------------------------------------------------------

// Sound effects enabled in the host's settings (the viewer's Play buttons and
// the WAV export hints follow it).
bool AssetViewerHost_SoundEnabled();
// Music enabled in the host's settings.
bool AssetViewerHost_MusicEnabled();

// Starts a sound-effect preview for g_Index.m_Sound[id]; returns the host's
// preview handle (nullptr on failure). The handle is freed by
// AssetViewerHost_StopSoundPreview.
void *AssetViewerHost_PlaySoundPreview(uint16_t id);
// Stops and frees a handle returned by AssetViewerHost_PlaySoundPreview.
void AssetViewerHost_StopSoundPreview(void *handle);

// Starts a music preview (the client fades the previous lane itself). Returns
// false when the host cannot play it.
bool AssetViewerHost_PlayMusic(const char *fileName, int index, bool loop);
// True while the track at `index` is still the one playing on the music lane.
bool AssetViewerHost_MusicStillPlaying(int index);
// Current track index on the music lane (-1 when none).
int AssetViewerHost_CurrentMusicIndex();
// Stops the music lane for a viewer-owned track; `owned` says whether the
// track at `index` is still the viewer's preview (`index` is -1 when the
// viewer never started one). A host must never silence music it did not start.
void AssetViewerHost_StopMusic(bool owned, int index);

// --- data set ----------------------------------------------------------------

// The UO data folder currently loaded, empty when none.
std::string AssetViewerHost_DataPath();
// Human readable state of the data set (the header line under the title).
std::string AssetViewerHost_DataStatus();
// Ask the host to change the data folder (the client has no such UI: it
// reports false and the viewer keeps showing the hint only).
bool AssetViewerHost_RequestDataPath();

#endif // XUO_ASSISTANT
