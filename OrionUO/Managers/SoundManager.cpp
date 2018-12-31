// MIT License
// Copyright (C) August 2016 Hotride

#if 0
#define SOUND_DEBUG_TRACE DEBUG_TRACE_FUNCTION
#else
#define SOUND_DEBUG_TRACE
#endif

#if !USE_BASS
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

#else
HSTREAM s_Music = 0;
HSTREAM s_WarMusic = 0;
#define VOLUME_FACTOR 1.0f

struct BASS_ErrorDescription
{
    int errorCode;
    char desc[64];
};

static BASS_ErrorDescription BASS_ErrorTable[38] = {
    { -2, "unspecified error" },
    { BASS_OK, "OK" },
    { BASS_ERROR_MEM, "memory error" },
    { BASS_ERROR_FILEOPEN, "can't open the file" },
    { BASS_ERROR_DRIVER, "can't find a free/valid driver" },
    { BASS_ERROR_BUFLOST, "the sample buffer was lost" },
    { BASS_ERROR_HANDLE, "invalid handle" },
    { BASS_ERROR_FORMAT, "unsupported sample format" },
    { BASS_ERROR_POSITION, "invalid position" },
    { BASS_ERROR_INIT, "BASS_Init has not been successfully called" },
    { BASS_ERROR_START, "BASS_Start has not been successfully called" },
    { BASS_ERROR_SSL, "SSL/HTTPS support isn't available" },
    { BASS_ERROR_ALREADY, "already initialized/paused/whatever" },
    { BASS_ERROR_NOCHAN, "can't get a free channel" },
    { BASS_ERROR_ILLTYPE, "an illegal type was specified" },
    { BASS_ERROR_ILLPARAM, "an illegal parameter was specified" },
    { BASS_ERROR_NO3D, "no 3D support" },
    { BASS_ERROR_NOEAX, "no EAX support" },
    { BASS_ERROR_DEVICE, "illegal device number" },
    { BASS_ERROR_NOPLAY, "not playing" },
    { BASS_ERROR_FREQ, "illegal sample rate" },
    { BASS_ERROR_NOTFILE, "the stream is not a file stream" },
    { BASS_ERROR_NOHW, "no hardware voices available" },
    { BASS_ERROR_EMPTY, "the MOD music has no sequence data" },
    { BASS_ERROR_NONET, "no internet connection could be opened" },
    { BASS_ERROR_CREATE, "couldn't create the file" },
    { BASS_ERROR_NOFX, "effects are not available" },
    { BASS_ERROR_NOTAVAIL, "requested data is not available" },
    { BASS_ERROR_DECODE, "the channel is/isn't a \"decoding channel\"" },
    { BASS_ERROR_DX, "a sufficient DirectX version is not installed" },
    { BASS_ERROR_TIMEOUT, "connection timedout" },
    { BASS_ERROR_FILEFORM, "unsupported file format" },
    { BASS_ERROR_SPEAKER, "unavailable speaker" },
    { BASS_ERROR_VERSION, "invalid BASS version (used by add-ons)" },
    { BASS_ERROR_CODEC, "codec is not available/supported" },
    { BASS_ERROR_ENDED, "the channel/file has ended" },
    { BASS_ERROR_BUSY, "the device is busy" },
    { BASS_ERROR_UNKNOWN, "some other mystery problem" },
};

static inline const char *GetErrorDescription()
{
    int currentErrorCode = BASS_ErrorGetCode();
    for (int i = 0; i < 38; i++)
    {
        if (BASS_ErrorTable[i].errorCode == currentErrorCode)
        {
            return BASS_ErrorTable[i].desc;
        }
    }
    return BASS_ErrorTable[0].desc;
}
#endif

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
    LOG("Initializing bass sound system.\n");
    // initialize default output device
#if USE_BASS
    auto hwnd = (HWND)g_OrionWindow.Handle;
    if (!BASS_Init(-1, 48000, BASS_DEVICE_3D, hwnd, nullptr))
    {
        LOG("Can't initialize device: %s\n", GetErrorDescription());
        return false;
    }

    BASS_SetConfig(BASS_CONFIG_SRC, 3); // interpolation method
    if (!BASS_SetConfig(BASS_CONFIG_3DALGORITHM, BASS_3DALG_FULL))
    {
        LOG("Error setting 3d sound: %s\n", GetErrorDescription());
    }
    auto path = g_App.ExeFilePath("uo_4mb_2.sf2");
    if (!BASS_SetConfigPtr(BASS_CONFIG_MIDI_DEFFONT, CStringFromPath(path)))
    {
        LOG("Could not load soundfont file for midi");
    }
#else
    s_backend.init();
    s_backend.setGlobalVolume(0.30f);
#endif
    LOG("Sound init successfull.\n");

    return true;
}

void CSoundManager::Free()
{
    SOUND_DEBUG_TRACE;
    StopMusic();
#if USE_BASS
    BASS_Free();
#else
    s_backend.deinit();
#endif
}

void CSoundManager::PauseSound()
{
    SOUND_DEBUG_TRACE;
#if USE_BASS
    BASS_Pause();
#else
    s_backend.setPauseAll(true);
#endif
    g_Orion.AdjustSoundEffects(g_Ticks + 100000);
}

void CSoundManager::ResumeSound()
{
    SOUND_DEBUG_TRACE;
#if USE_BASS
    BASS_Start();
#else
    s_backend.setPauseAll(false);
#endif
}

bool CSoundManager::UpdateSoundEffect(SoundHandle stream, float volume)
{
#if USE_BASS
    if (volume > 0)
    {
        BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, volume);
        return true;
    }
#else
    if (stream && volume > 0)
    {
        s_backend.setVolume(stream->h, VOLUME_FACTOR * volume);
        return true;
    }
#endif
    FreeSound(stream);
    return false;
}

float CSoundManager::GetVolumeValue(int distance, bool music)
{
    SOUND_DEBUG_TRACE;
    uint16_t clientConfigVolume =
        music ? g_ConfigManager.GetMusicVolume() : g_ConfigManager.GetSoundVolume();

#if USE_BASS
    float volume = BASS_GetVolume();
#else
    float volume = s_backend.getGlobalVolume();
#endif

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
#if 0
        static int sid = 0;
        char fname[64];
        snprintf(fname, 64, "sound%02d.wav", sid);
        FILE *fp = fopen(fname, "wb");
        size_t waveFileSize = is.DataSize - sizeof(SOUND_BLOCK) + sizeof(WaveHeader);
        fwrite(is.m_WaveFile, 1, waveFileSize, fp);
        fclose(fp);
        sid++;
#endif
    }
    size_t waveFileSize = is.DataSize - sizeof(SOUND_BLOCK) + sizeof(WaveHeader);
#if USE_BASS
    auto stream = BASS_StreamCreateFile(
        true,
        is.m_WaveFile,
        0,
        waveFileSize - 16,
        BASS_SAMPLE_FLOAT | BASS_SAMPLE_3D | BASS_SAMPLE_SOFTWARE);
#else
    auto stream = getSound(is.m_WaveFile, waveFileSize - 16);
#endif

    if (stream == SOUND_NULL)
    {
        LOG("Error creating sound voice: %s\n", GetErrorDescription());
        free(is.m_WaveFile);
        is.m_WaveFile = nullptr;
    }

    return stream;
}

void CSoundManager::PlaySoundEffect(SoundHandle stream, float volume)
{
    SOUND_DEBUG_TRACE;
    if (stream == SOUND_NULL || (!g_OrionWindow.IsActive() && !g_ConfigManager.BackgroundSound))
    {
        return;
    }

#if USE_BASS
    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, volume);
    if (!BASS_ChannelPlay(stream, false))
    {
        LOG("Bass sound play error: %s\n", GetErrorDescription());
    }
#else
    if (stream->source)
    {
        stream->h = s_backend.play(*stream->source, VOLUME_FACTOR * volume);
    }
    else
    {
        LOG("Trying to play unallocated sound");
    }
#endif
}

void CSoundManager::FreeSound(SoundHandle stream)
{
    SOUND_DEBUG_TRACE;
#if USE_BASS
    BASS_StreamFree(stream);
#else
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
        LOG("Trying to free unallocated sound");
    }
#endif

    stream = SOUND_NULL;
}

void CSoundManager::SetMusicVolume(float volume)
{
    SOUND_DEBUG_TRACE;
#if USE_BASS
    if (s_Music != 0 && IsPlayingNormalMusic())
    {
        BASS_ChannelSetAttribute(s_Music, BASS_ATTRIB_VOL, volume);
    }

    if (s_WarMusic != 0 && BASS_ChannelIsActive(s_WarMusic))
    {
        BASS_ChannelSetAttribute(s_WarMusic, BASS_ATTRIB_VOL, volume);
    }
#else
    float v = VOLUME_FACTOR * volume / 255.0f;
    s_backend.setVolume(s_Music[0], v);
    s_backend.setVolume(s_Music[1], v);
#endif
}

bool CSoundManager::IsPlayingNormalMusic()
{
    SOUND_DEBUG_TRACE;

#if USE_BASS
    return BASS_ChannelIsActive(s_Music);
#else
    return s_backend.isValidVoiceHandle(s_Music[0]);
#endif
}

void CSoundManager::PlayMP3(const std::string &fileName, int index, bool loop, bool warmode)
{
    SOUND_DEBUG_TRACE;

#if USE_BASS
    if (warmode && s_WarMusic != 0)
    {
        return;
    }

    if (warmode)
    {
        StopWarMusic();
    }
    else
    {
        StopMusic();
    }

    HSTREAM streamHandle =
        BASS_StreamCreateFile(FALSE, fileName.c_str(), 0, 0, loop ? BASS_SAMPLE_LOOP : 0);
    BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_VOL, GetVolumeValue(-1, true));
    BASS_ChannelPlay(streamHandle, 0);

    if (warmode)
    {
        s_WarMusic = streamHandle;
    }
    else
    {
        s_Music = streamHandle;
        CurrentMusicIndex = index;
    }
#else
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
#endif
}

void CSoundManager::StopWarMusic()
{
    SOUND_DEBUG_TRACE;

#if USE_BASS
    BASS_ChannelStop(s_WarMusic);
    s_WarMusic = 0;
    if (s_Music != 0 && !IsPlayingNormalMusic())
    {
        BASS_ChannelPlay(s_Music, 1);
    }
#else
    s_backend.stop(s_Music[1]);
    if (s_Music[0] != 0 && !IsPlayingNormalMusic())
    {
        s_backend.fadeVolume(s_Music[0], VOLUME_FACTOR, 2);
    }
#endif
}

void CSoundManager::StopMusic()
{
    SOUND_DEBUG_TRACE;

#if USE_BASS
    BASS_ChannelStop(s_Music);
    BASS_ChannelStop(s_WarMusic);
    s_Music = 0;
    s_WarMusic = 0;
#else
    s_backend.stop(s_Music[0]);
    s_backend.stop(s_Music[1]);
    s_backend.stopAudioSource(s_MusicStream[0]);
    s_backend.stopAudioSource(s_MusicStream[1]);
    //s_backend.stopAudioSource(s_MusicMidi[0]);
    //s_backend.stopAudioSource(s_MusicMidi[1]);
    s_Music[0] = 0;
    s_Music[1] = 0;
#endif
}

void CSoundManager::PlayMidi(int index, bool warmode)
{
    SOUND_DEBUG_TRACE;
    if (index < 0 || index >= MIDI_MUSIC_COUNT)
    {
        LOG("Music ID is out of range: %i\n", index);
        return;
    }

    char musicPath[100] = { 0 };
    MidiInfoStruct midiInfo = s_MidiInfo[index];
    sprintf_s(musicPath, "music/%s", midiInfo.musicName);

#if USE_BASS
    if (warmode && s_WarMusic != 0)
    {
        return;
    }

    if (warmode)
    {
        BASS_ChannelStop(s_WarMusic);
    }
    else
    {
        StopMusic();
    }

    wstring midiName = ToWString(musicPath);
    HSTREAM streamHandle =
        BASS_MIDI_StreamCreateFile(FALSE, midiName.c_str(), 0, 0, BASS_MIDI_DECAYEND, 0);
    float volume = GetVolumeValue(-1, true);
    BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_VOL, volume);
    BASS_ChannelPlay(streamHandle, midiInfo.loop);
    if (warmode)
    {
        s_WarMusic = streamHandle;
    }
    else
    {
        CurrentMusicIndex = index;
        s_Music = streamHandle;
    }
#else
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
#endif
}
