// MIT License
// Copyright (C) August 2016 Hotride

#include "SoundManager.h"
#include "ConfigManager.h"
#include "../CrossUO.h"
#include "../GameWindow.h"

#if 0
#define SOUND_DEBUG_TRACE DEBUG_TRACE_FUNCTION
#else
#define SOUND_DEBUG_TRACE
#endif

#define WITH_SDL2_STATIC

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include <tsf.h>
#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include <tml.h>

#define ASS_IMPLEMENTATION
#if _MSC_VER
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwritable-strings"
#pragma GCC diagnostic ignored "-Wmissing-variable-declarations"
#endif
#include <ass.h>
#if _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
using namespace SoLoud;
static Soloud s_backend;

//static SoundFont s_Sf2;
//static Midi s_MusicMidi[2];
static WavStream s_MusicStream[2];
static AudioSource *s_MusicSource[] = { &s_MusicStream[0], &s_MusicStream[1] };
static handle s_Music[] = { 0, 0 };

#define GetErrorDescription() __FUNCTION__
#define MAX_SOUNDS 2048
#define VOLUME_FACTOR 1.0f

struct SoundInfo
{
    AudioSource *source = nullptr;
    void *data = nullptr;
    size_t len = 0;
    handle h = 0;
};

// FIXME: this is a temporary hack, we shoud use AudioSource directly as SoundStream
static std::unordered_map<void *, AudioSource *> s_audioSources;
static SoundInfo *getSound(void *data, size_t len)
{
    static bool init = false;
    if (!init)
    {
        s_audioSources.reserve(MAX_SOUNDS);
        init = false;
    }
    assert(s_audioSources.size() < MAX_SOUNDS);

    auto it = s_audioSources.find(data);
    auto *sfx = new SoundInfo;
    assert(sfx && "Could not allocate SoundInfo");
    if (it == s_audioSources.end())
    {
        Wav *w = new Wav;
        w->loadMem((unsigned char *)data, checked_cast<int>(len), false, false);
        sfx->source = w;
        sfx->data = data;
        sfx->len = len;
        s_audioSources.emplace(std::make_pair(data, w));
        return sfx;
    }

    AudioSource *w = s_audioSources[data];
    sfx->source = w;
    sfx->data = data;
    sfx->len = len;
    return sfx;
}

#pragma pack(push, 1)
struct MidiInfoStruct
{
    const char *musicName;
    bool loop;
};
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
    //data;
};
#pragma pack(pop)

static const int MIDI_MUSIC_COUNT = 57;
static const MidiInfoStruct s_MidiInfo[MIDI_MUSIC_COUNT] = {
    { "oldult01.mid", true },  { "create1.mid", false },  { "dragflit.mid", false },
    { "oldult02.mid", true },  { "oldult03.mid", true },  { "oldult04.mid", true },
    { "oldult05.mid", true },  { "oldult06.mid", true },  { "stones2.mid", true },
    { "britain1.mid", true },  { "britain2.mid", true },  { "bucsden.mid", true },
    { "jhelom.mid", false },   { "lbcastle.mid", false }, { "linelle.mid", false },
    { "magincia.mid", true },  { "minoc.mid", true },     { "ocllo.mid", true },
    { "samlethe.mid", false }, { "serpents.mid", true },  { "skarabra.mid", true },
    { "trinsic.mid", true },   { "vesper.mid", true },    { "wind.mid", true },
    { "yew.mid", true },       { "cave01.mid", false },   { "dungeon9.mid", false },
    { "forest_a.mid", false }, { "intown01.mid", false }, { "jungle_a.mid", false },
    { "mountn_a.mid", false }, { "plains_a.mid", false }, { "sailing.mid", false },
    { "swamp_a.mid", false },  { "tavern01.mid", false }, { "tavern02.mid", false },
    { "tavern03.mid", false }, { "tavern04.mid", false }, { "combat1.mid", false },
    { "combat2.mid", false },  { "combat3.mid", false },  { "approach.mid", false },
    { "death.mid", false },    { "victory.mid", false },  { "btcastle.mid", false },
    { "nujelm.mid", true },    { "dungeon2.mid", false }, { "cove.mid", true },
    { "moonglow.mid", true },  { "oldult02.mid", false }, { "serpents.mid", true },
    { "oldult04.mid", false }, { "dragflit.mid", false }, { "create1.mid", false },
    { "approach.mid", false }, { "combat3.mid", false },  { "jungle_a.mid", false }
};

CSoundManager g_SoundManager;

static uint8_t *CreateWaveFile(CIndexSound &is)
{
    SOUND_DEBUG_TRACE;
    size_t dataSize = is.DataSize - sizeof(SOUND_BLOCK);
    auto waveSound = (uint8_t *)malloc(dataSize + sizeof(WaveHeader));
    auto waveHeader = reinterpret_cast<WaveHeader *>(waveSound);

    strcpy(waveHeader->chunkId, "RIFF");
    strcpy(waveHeader->format, "WAVE");
    strcpy(waveHeader->subChunkId, "fmt ");
    strcpy(waveHeader->dataChunkId, "data");

    waveHeader->chunkSize = uint32_t(dataSize + sizeof(WaveHeader));
    waveHeader->subChunkSize = 16;
    waveHeader->audioFormat = 1;
    waveHeader->numChannels = 1;
    waveHeader->sampleRate = 22050;
    waveHeader->bitsPerSample = 16;
    waveHeader->bytesPerSecond = 88200;
    waveHeader->blockAlign = 4;
    waveHeader->dataSize = uint32_t(dataSize);

    is.Delay = uint32_t((dataSize - 16) / 88.2f);
    auto sndDataPtr = reinterpret_cast<uint8_t *>(is.Address + sizeof(SOUND_BLOCK));
    memcpy(waveSound + sizeof(WaveHeader), sndDataPtr + 16, dataSize - 16);

    return waveSound;
}

CSoundManager::CSoundManager()
{
}

CSoundManager::~CSoundManager()
{
}

bool CSoundManager::Init()
{
    SOUND_DEBUG_TRACE;
    Info(Client, "initializing sound system");
    s_backend.init();
    s_backend.setGlobalVolume(0.30f);
    Info(Client, "sound init successfull");

    return true;
}

void CSoundManager::Free()
{
    SOUND_DEBUG_TRACE;
    StopMusic();
    s_backend.deinit();
}

void CSoundManager::PauseSound()
{
    SOUND_DEBUG_TRACE;
    s_backend.setPauseAll(true);
    g_Game.AdjustSoundEffects(g_Ticks + 100000);
}

void CSoundManager::ResumeSound()
{
    SOUND_DEBUG_TRACE;
    s_backend.setPauseAll(false);
}

bool CSoundManager::UpdateSoundEffect(SoundHandle stream, float volume)
{
    if (stream && volume > 0)
    {
        s_backend.setVolume(stream->h, VOLUME_FACTOR * volume);
        return true;
    }
    FreeSound(stream);
    return false;
}

float CSoundManager::GetVolumeValue(int distance, bool music)
{
    SOUND_DEBUG_TRACE;
    uint16_t clientConfigVolume =
        music ? g_ConfigManager.GetMusicVolume() : g_ConfigManager.GetSoundVolume();

    float volume = s_backend.getGlobalVolume();
    if (volume == 0 || clientConfigVolume == 0)
    {
        return 0;
    }
    float clientsVolumeValue = (255.f / float(clientConfigVolume));
    volume /= clientsVolumeValue;
    if (distance > g_ConfigManager.UpdateRange || distance < 1)
    {
        return volume;
    }

    float soundValuePerDistance = volume / g_ConfigManager.UpdateRange;
    return volume - (soundValuePerDistance * distance);
}

SoundHandle CSoundManager::LoadSoundEffect(CIndexSound &is)
{
    SOUND_DEBUG_TRACE;

    if (is.m_WaveFile == nullptr)
    {
        is.m_WaveFile = CreateWaveFile(is);
    }
    size_t waveFileSize = is.DataSize - sizeof(SOUND_BLOCK) + sizeof(WaveHeader);
    auto stream = getSound(is.m_WaveFile, waveFileSize - 16);
    if (stream == SOUND_NULL)
    {
        Warning(Client, "creating sound voice: %s", GetErrorDescription());
        free(is.m_WaveFile);
        is.m_WaveFile = nullptr;
    }

    return stream;
}

void CSoundManager::PlaySoundEffect(SoundHandle stream, float volume)
{
    SOUND_DEBUG_TRACE;
    if (stream == SOUND_NULL || (!g_GameWindow.IsActive() && !g_ConfigManager.BackgroundSound))
    {
        return;
    }

    if (stream->source)
    {
        stream->h = s_backend.play(*stream->source, VOLUME_FACTOR * volume);
    }
    else
    {
        Error(Client, "trying to play unallocated sound");
    }
}

void CSoundManager::FreeSound(SoundHandle stream)
{
    SOUND_DEBUG_TRACE;
    if (stream == SOUND_NULL)
    {
        return;
    }

    if (stream->source)
    {
        s_backend.stop(stream->h);
        delete stream;
    }
    else
    {
        Warning(Client, "trying to free unallocated sound");
    }
    stream = SOUND_NULL;
}

void CSoundManager::SetMusicVolume(float volume)
{
    SOUND_DEBUG_TRACE;
    float v = VOLUME_FACTOR * volume / 255.0f;
    s_backend.setVolume(s_Music[0], v);
    s_backend.setVolume(s_Music[1], v);
}

bool CSoundManager::IsPlayingNormalMusic()
{
    SOUND_DEBUG_TRACE;
    return s_backend.isValidVoiceHandle(s_Music[0]);
}

void CSoundManager::PlayMP3(const string &fileName, int index, bool loop, bool warmode)
{
    SOUND_DEBUG_TRACE;
    int cur = warmode ? 1 : 0;
    int old = warmode ? 0 : 1;
    if (s_Music[cur])
    {
        s_backend.stop(s_Music[cur]);
    }

    s_MusicStream[cur].load(fileName.c_str());
    s_MusicStream[cur].setLooping(loop);
    s_Music[cur] = s_backend.play(s_MusicStream[cur], 0, 0);
    s_backend.fadeVolume(s_Music[cur], VOLUME_FACTOR, 2);
    s_backend.fadeVolume(s_Music[old], 0, 2);

    if (!warmode)
    {
        CurrentMusicIndex = index;
    }
}

void CSoundManager::StopWarMusic()
{
    SOUND_DEBUG_TRACE;
    s_backend.stop(s_Music[1]);
    if (s_Music[0] != 0 && !IsPlayingNormalMusic())
    {
        s_backend.fadeVolume(s_Music[0], VOLUME_FACTOR, 2);
    }
}

void CSoundManager::StopMusic()
{
    SOUND_DEBUG_TRACE;
    s_backend.stop(s_Music[0]);
    s_backend.stop(s_Music[1]);
    s_backend.stopAudioSource(s_MusicStream[0]);
    s_backend.stopAudioSource(s_MusicStream[1]);
    //s_backend.stopAudioSource(s_MusicMidi[0]);
    //s_backend.stopAudioSource(s_MusicMidi[1]);
    s_Music[0] = 0;
    s_Music[1] = 0;
}

void CSoundManager::PlayMidi(int index, bool warmode)
{
    SOUND_DEBUG_TRACE;
    if (index < 0 || index >= MIDI_MUSIC_COUNT)
    {
        Warning(Client, "music id is out of range: %i", index);
        return;
    }

    char musicPath[100] = { 0 };
    MidiInfoStruct midiInfo = s_MidiInfo[index];
    sprintf_s(musicPath, "music/%s", midiInfo.musicName);
    /*
    static bool tsfLoaded = false;
    if (!tsfLoaded)
    {
        tsfLoaded = true;
        auto path = ToString(g_App.ExeFilePath("uo_4mb_2.sf2"));
        s_Sf2.load(path.c_str());
    }

    s_MusicSource[0] = &s_MusicMidi[0];
    s_MusicSource[1] = &s_MusicMidi[1];

    int cur = warmode ? 1 : 0;
    int old = warmode ? 0 : 1;
    if (s_Music[cur])
    {
        s_backend.stop(s_Music[cur]);
    }

    s_MusicMidi[cur].load(musicPath, s_Sf2);
    s_MusicMidi[cur].setLooping(midiInfo.loop);
    s_Music[cur] = s_backend.play(s_MusicMidi[cur], VOLUME_FACTOR, 0, 0);

    if (!warmode)
    {
        CurrentMusicIndex = index;
    }
    */
}
