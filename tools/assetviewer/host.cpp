// SPDX-License-Identifier: AGPL-3.0-or-later

// Asset viewer host for the standalone app: the viewer owns the audio lane and
// decides which data folder is browsed.

#include "AssetViewerHost.h"

#if defined(XUO_ASSISTANT)

#include <xuocore/uodata.h>        // g_Index
#include <xuocore/uodata_reader.h> // uo_art_pixels (the shared art parser)

#include "app.h"
#include "audio.h"

namespace
{
// the data folder the viewer was handed, remembered by the app
const std::string &DataPathRef()
{
    return AssetViewerApp::Instance().DataPath();
}
} // namespace

std::vector<uint16_t>
AssetViewer_DecodeArt(uint16_t id, CIndexObject &io, bool run, short &width, short &height)
{
    // the same parser the client runs: no filters requested, so the stored art
    // is shown as-is (the game's stump/cave substitutions are client-only)
    return uo_art_pixels(id, io, run, width, height, nullptr);
}

bool AssetViewerHost_SoundEnabled()
{
    return true; // no game settings: the viewer plays its previews
}

bool AssetViewerHost_MusicEnabled()
{
    return true;
}

void *AssetViewerHost_PlaySoundPreview(uint16_t id)
{
    std::string error;
    return avaudio::PlaySoundPreview(id, error);
}

void AssetViewerHost_StopSoundPreview(void *handle)
{
    avaudio::StopSoundPreview(handle);
}

bool AssetViewerHost_PlayMusic(const char *fileName, int index, bool loop)
{
    std::string error;
    return avaudio::PlayMusic(fileName, index, loop, error);
}

bool AssetViewerHost_MusicStillPlaying(int index)
{
    return avaudio::MusicStillPlaying(index);
}

int AssetViewerHost_CurrentMusicIndex()
{
    return avaudio::CurrentMusicIndex();
}

void AssetViewerHost_StopMusic(bool owned, int index)
{
    // the standalone app has no game around it: the viewer owns every track
    // it started; `owned` still guards against a stop with no preview record
    if (owned || (index >= 0 && avaudio::MusicStillPlaying(index)))
    {
        avaudio::StopMusic();
    }
}

std::string AssetViewerHost_DataPath()
{
    return DataPathRef();
}

std::string AssetViewerHost_DataStatus()
{
    return AssetViewerApp::Instance().DataStatus();
}

bool AssetViewerHost_RequestDataPath()
{
    // the picker only exists in the standalone app; the client reports false
    AssetViewerApp::Instance().RequestDataPathPicker();
    return true;
}

#endif // XUO_ASSISTANT
