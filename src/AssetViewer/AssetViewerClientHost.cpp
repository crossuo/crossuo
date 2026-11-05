// SPDX-License-Identifier: AGPL-3.0-or-later

// Asset viewer host for the in-client mode: the viewer shares the game's
// music lane and sound manager, and its music stop must never silence music
// the game started (only its own preview).

#include "AssetViewerHost.h"

#if defined(XUO_ASSISTANT)

#include <vector>

#include <xuocore/uodata.h> // g_Index

#include "../CrossUO.h"                // g_Game (music ownership guard)
#include "../Managers/ConfigManager.h" // g_ConfigManager (sound/music enables)
#include "../Managers/SoundManager.h"  // g_SoundManager
#include "../Managers/UOFileReader.h"  // g_UOFileReader (the art parser)
#include "AssetViewerCore.h"

std::vector<uint16_t>
AssetViewer_DecodeArt(uint16_t id, CIndexObject &io, bool run, short &width, short &height)
{
    // the client's own art parser, with the game's stump/cave tile
    // substitutions switched off: the viewer shows the stored art
    return g_UOFileReader.GetArtPixels(id, io, run, width, height, false);
}

bool AssetViewerHost_SoundEnabled()
{
    return g_ConfigManager.GetSound();
}

bool AssetViewerHost_MusicEnabled()
{
    return g_ConfigManager.GetMusic();
}

void *AssetViewerHost_PlaySoundPreview(uint16_t id)
{
    SoundHandle handle = g_SoundManager.LoadSoundEffect(g_Index.m_Sound[id]);
    if (handle == SOUND_NULL)
    {
        return nullptr;
    }
    // one preview at a time (the viewer stops the previous one first): it
    // plays at the client's configured sound volume (game-consistent)
    g_SoundManager.PlaySoundEffect(handle, g_SoundManager.GetVolumeValue());
    return handle;
}

void AssetViewerHost_StopSoundPreview(void *handle)
{
    if (handle == nullptr)
    {
        return;
    }
    g_SoundManager.FreeSound(static_cast<SoundHandle>(handle));
}

bool AssetViewerHost_PlayMusic(const char *fileName, int index, bool loop)
{
    if (fileName == nullptr || fileName[0] == '\0')
    {
        return false;
    }
    g_SoundManager.PlayMP3(fileName, index, loop, false);
    return true;
}

bool AssetViewerHost_MusicStillPlaying(int index)
{
    return index >= 0 && g_SoundManager.CurrentMusicIndex == index &&
           g_SoundManager.IsPlayingNormalMusic();
}

int AssetViewerHost_CurrentMusicIndex()
{
    return g_SoundManager.CurrentMusicIndex;
}

void AssetViewerHost_StopMusic(bool owned, int index)
{
    // the game shares the lane: only a track that is really still the one
    // playing is ever stopped, and only when it is the viewer's own
    g_Game.StopViewerMusic(owned, index);
}

std::string AssetViewerHost_DataPath()
{
    // the client owns its data path (game configuration): the viewer does
    // not show or change it
    return std::string();
}

std::string AssetViewerHost_DataStatus()
{
    return std::string();
}

bool AssetViewerHost_RequestDataPath()
{
    return false;
}

#endif // XUO_ASSISTANT
