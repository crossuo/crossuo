// SPDX-License-Identifier: AGPL-3.0-or-later

// The standalone viewer's audio: a self-contained SoLoud instance (the same
// library the client's SoundManager wraps) for the Sound and Music tab
// previews. It is built from the client manager's wave-building rules
// (CreateWaveFile: 22050 Hz mono 16-bit WAV out of the 32-byte sound block
// header + 16-byte hint) so previews sound exactly like in the game.

#include "audio.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cstdint>
#include <cstring>
#include <vector>

// ass.h includes <windows.h> when built for Windows; it must land at global
// scope - inside the wrapping namespace the mingw Interlocked shim functions
// end up in the namespace and cannot resolve their long-pointer overloads
// (the client's SoundManager gets the same effect via common/fs.h)
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "../../src/SDL_wrapper.h"

#include <xuocore/mulstruct.h>
#include <xuocore/uodata.h>

// ass.h is designed to be dropped inside a namespace and needs math.h
// included first (the client manager does the same). The static SDL3 backend
// is the only one this tree builds: the classic SDL2 one dlopens SDL2.
#ifndef ASS_IMPLEMENTATION
#define ASS_IMPLEMENTATION
#define WITH_SDL2_STATIC
namespace avaudio
{
namespace audioimpl
{
#include <external/ass.h>
}
} // namespace avaudio
#endif

namespace avaudio
{
namespace
{
using namespace avaudio::audioimpl::SoLoud;
Soloud s_Backend;

bool s_Initialized = false;
int s_CurrentMusicIndex = -1;

struct SoundStream
{
    AudioSource *Source = nullptr;
    handle Voice = 0;
    std::vector<uint8_t> Wave; // the WAV bytes the source reads from
    std::vector<uint8_t> Block;
};

WavStream s_MusicStream;
handle s_MusicVoice = 0;

// WAV header without the 16-byte hint prefix, exactly like the client's
// SoundManager::CreateWaveFile builds it
#pragma pack(push, 1)
struct WaveHeader
{
    char chunkId[4];
    uint32_t chunkSize;
    char format[4];
    char subChunkId[4];
    uint32_t subChunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char dataChunkId[4];
    uint32_t dataSize;
};
#pragma pack(pop)

void StopMusicVoice()
{
    if (s_MusicVoice != 0)
    {
        s_Backend.stop(s_MusicVoice);
        s_MusicVoice = 0;
    }
    s_Backend.stopAudioSource(s_MusicStream);
    s_CurrentMusicIndex = -1;
}

bool BuildWave(uint16_t id, std::vector<uint8_t> &out)
{
    if (id >= MAX_SOUND_DATA_INDEX_COUNT)
    {
        return false;
    }
    CIndexSound &is = g_Index.m_Sound[id];
    std::vector<uint8_t> fetched;
    size_t blockSize = 0;
    const uint8_t *block = uo_sound_block_data(is, fetched, blockSize);
    if (block == nullptr || blockSize <= sizeof(SOUND_BLOCK) + 16)
    {
        return false;
    }
    const size_t dataSize = blockSize - sizeof(SOUND_BLOCK);
    out.resize(sizeof(WaveHeader) + dataSize);
    auto *header = reinterpret_cast<WaveHeader *>(out.data());
    memcpy(header->chunkId, "RIFF", 4);
    memcpy(header->format, "WAVE", 4);
    memcpy(header->subChunkId, "fmt ", 4);
    memcpy(header->dataChunkId, "data", 4);
    header->chunkSize = uint32_t(dataSize + sizeof(WaveHeader));
    header->subChunkSize = 16;
    header->audioFormat = 1;
    header->numChannels = 1;
    header->sampleRate = 22050;
    header->bitsPerSample = 16;
    header->bytesPerSecond = 88200;
    header->blockAlign = 4;
    header->dataSize = uint32_t(dataSize);
    memcpy(out.data() + sizeof(WaveHeader), block + sizeof(SOUND_BLOCK) + 16, dataSize - 16);
    return true;
}
} // namespace

bool Init()
{
    if (s_Initialized)
    {
        return true;
    }
    s_Backend.init();
    s_Backend.setGlobalVolume(0.30f);
    s_Initialized = true;
    return true;
}

void Shutdown()
{
    if (!s_Initialized)
    {
        return;
    }
    StopMusicVoice();
    s_Backend.deinit();
    s_Initialized = false;
}

void *PlaySoundPreview(uint16_t id, std::string &error)
{
    if (!s_Initialized || !Init())
    {
        error = "audio backend unavailable";
        return nullptr;
    }
    auto *stream = new SoundStream();
    if (!BuildWave(id, stream->Wave))
    {
        error = "failed to decode the sound block";
        delete stream;
        return nullptr;
    }
    auto *wav = new Wav();
    if (wav->loadMem(stream->Wave.data(), int(stream->Wave.size()), false, false) != 0)
    {
        error = "failed to decode the wave";
        delete wav;
        delete stream;
        return nullptr;
    }
    stream->Source = wav;
    stream->Voice = s_Backend.play(*wav, 1.0f);
    return stream;
}

void StopSoundPreview(void *handle)
{
    auto *stream = static_cast<SoundStream *>(handle);
    if (stream == nullptr)
    {
        return;
    }
    s_Backend.stop(stream->Voice);
    delete stream->Source;
    delete stream;
}

bool PlayMusic(const char *fileName, int index, bool loop, std::string &error)
{
    if (!s_Initialized || !Init())
    {
        error = "audio backend unavailable";
        return false;
    }
    StopMusicVoice();
    if (s_MusicStream.load(fileName) != 0)
    {
        error = "failed to load the music file";
        return false;
    }
    s_MusicStream.setLooping(loop);
    s_MusicVoice = s_Backend.play(s_MusicStream, 1.0f, 0.0f);
    s_CurrentMusicIndex = index;
    return true;
}

void StopMusic()
{
    StopMusicVoice();
}

bool MusicStillPlaying(int index)
{
    return index >= 0 && s_CurrentMusicIndex == index && s_MusicVoice != 0 &&
           s_Backend.isValidVoiceHandle(s_MusicVoice);
}

int CurrentMusicIndex()
{
    return s_CurrentMusicIndex;
}

} // namespace avaudio
