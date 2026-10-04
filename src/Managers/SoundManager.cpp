// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "SoundManager.h"
#include "ConfigManager.h"
#include <xuocore/uodata.h>
#if defined(XUO_EMSC)
#include <xuocore/vfs.h> // vfs_stream_file_ensure (music data lane)
#endif
#include <common/utils.h>
#include "../CrossUO.h"
#include "../GameWindow.h"
#include <unordered_map>

#ifndef ASS_IMPLEMENTATION
#define ASS_IMPLEMENTATION
#define WITH_SDL2_STATIC
#if defined(XUO_EMSC)
// web: compile in soloud's null backend - it provides the whole mixer state
// machine WITHOUT a device and WITHOUT a mixing thread (see soloud_null.cpp:
// postinit only), so the game loop can drive the mix() output itself and
// pump it into the SharedArrayBuffer ring the page's ScriptProcessorNode
// pulls (CSoundManager::Update + shell.html). SDL2 audio can not run here:
// the AudioContext lives on the browser main thread (autoplay policy + the
// worker crash on Module.SDL2.audioContext)
#define WITH_NULL
#endif
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>
namespace ass
{
#include <external/ass.h>
};
#endif // ASS_IMPLEMENTATION

#if defined(XUO_EMSC)
#include <emscripten.h>
#endif

#if 0
#define SOUND_DEBUG_TRACE DEBUG_TRACE_FUNCTION
#else
#define SOUND_DEBUG_TRACE
#endif

using namespace ass::SoLoud;
static Soloud s_backend;

//static SoundFont s_Sf2;
//static Midi s_MusicMidi[2];
static WavStream s_MusicStream[2];
//static AudioSource *s_MusicSource[] = { &s_MusicStream[0], &s_MusicStream[1] };
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
    // web: the sound pack materializes as a table-only skeleton - the index
    // bakes Address into the ORIGINAL archive layout, far past the end of the
    // small skeleton mapping, so a raw read lands in unrelated heap memory
    // (the pcm bytes change from run to run). Fetch the real block through
    // the punctual layer like every other streamed reader
    // (CUopMappedFile::GetData). Native keeps the raw pointer: the mapping
    // is the whole file there and the trap resolves ranges on fault.
    std::vector<uint8_t> fetched;
    const uint8_t *blockData = reinterpret_cast<const uint8_t *>(is.Address);
    size_t blockSize = is.DataSize;
#if defined(XUO_EMSC)
    if (is.UopBlock != nullptr)
    {
        fetched = g_FileManager.m_SoundLegacyMUL.GetData(is.UopBlock);
        if (fetched.size() < sizeof(SOUND_BLOCK))
        {
            Warning(Client, "sound block fetch failed: %u", is.DataSize);
            return nullptr;
        }
        blockData = fetched.data();
        blockSize = fetched.size();
    }
    else if (is.UopHash != 0 && vfs_asset_read(is.UopHash, fetched) &&
             fetched.size() >= sizeof(SOUND_BLOCK))
    {
        // planned index (no table entry): the whole block by hash
        blockData = fetched.data();
        blockSize = fetched.size();
    }
    else if (is.UopBlock == nullptr && is.UopHash != 0)
    {
        Warning(Client, "sound block fetch failed: %u", is.DataSize);
        return nullptr;
    }
#endif
    size_t dataSize = blockSize - sizeof(SOUND_BLOCK);
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
    auto sndDataPtr = blockData + sizeof(SOUND_BLOCK);
    memcpy(waveSound + sizeof(WaveHeader), sndDataPtr + 16, dataSize - 16);

    return waveSound;
}

CSoundManager::CSoundManager()
{
}

CSoundManager::~CSoundManager()
{
}

#if defined(XUO_EMSC)
// --- web output backend ----------------------------------------------------
// The browser owns the audio device: its AudioContext lives on the page
// (shell.html) and is created suspended - autoplay policy only allows the
// page's user gesture to resume it. SoLoud keeps running on the null driver
// (full mixer state machine, no device, no thread) and the game loop pumps
// its mix() output into a SharedArrayBuffer ring that the page's
// ScriptProcessorNode pulls (4096-frame quantum). Ring layout (words are
// int32, then interleaved stereo float32; monotonic frame counters keep the
// fill math wrap-safe - see shell.html for the JS side):
//   [0] writeIdx (this thread) [1] readIdx (page) [2] capacity
//   [3] sampleRate (page)      [4] unlocked (page) [5] context running (page)
//   [6] underrun callbacks     [7] magic 'XUA1'
#define XUO_WEB_RING_FRAMES 16384 // ~340ms at 48kHz: absorbs frame hitches
#define XUO_WEB_MIX_CHUNK 4096    // == soloud's scratch size (init buffer)

static float s_web_mixBuffer[XUO_WEB_MIX_CHUNK * 2];

static void web_audio_init()
{
    // the context exists (suspended) from page load, so the hardware rate is
    // known before any gesture exists: mix exactly at it, nothing resamples
    const unsigned int rate = MAIN_THREAD_EM_ASM_INT(
    {
        if (window.__xuoAudio && window.__xuoAudio.ctx)
            return window.__xuoAudio.ctx.sampleRate | 0;
        return 48000;
    });
    s_backend.init(Soloud::CLIP_ROUNDOFF, Soloud::NULLDRIVER, rate, XUO_WEB_MIX_CHUNK, 2);
    // create the ring here (the mixer writes from the wasm heap) and hand
    // the SharedArrayBuffer to the page. No cmd field: emscripten's pthread
    // message pump on the main thread ignores it, the shell's own listener
    // picks it up (shell.html hookPThread)
    EM_ASM(
    {
        if (globalThis.__xuoRing)
            return;
        const frames = $0;
        const sab = new SharedArrayBuffer(8 * 4 + frames * 2 * 4);
        const words = new Int32Array(sab, 0, 8);
        words[2] = frames;
        words[7] = 0x58554131; // 'XUA1'
        globalThis.__xuoRing = ({
            words: words,
            samples: new Float32Array(sab, 32, frames * 2),
            cap: frames
        });
        postMessage({ xuoAudioRing: sab });
    },
    XUO_WEB_RING_FRAMES);
    Info(Client, "web audio: soloud manual mix at %u Hz into a %d frame sab ring", rate, XUO_WEB_RING_FRAMES);
}
#endif // XUO_EMSC

bool CSoundManager::Init()
{
    SOUND_DEBUG_TRACE;
    Info(Client, "initializing sound system");
#if defined(XUO_EMSC)
    // web output backend: SoLoud stays on the null driver (no device, no
    // thread) and CSoundManager::Update pumps its mix() into the page's
    // AudioContext through a SharedArrayBuffer ring
    web_audio_init();
#else
    s_backend.init();
#endif
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

void CSoundManager::Update()
{
#if defined(XUO_EMSC)
    // pump SoLoud's mixer into the web audio ring - only while the page has
    // unlocked the context AND it is running: before the unlock gesture the
    // ring could not drain, and mixing would burn every queued voice down
    // silently (a hidden tab stops rAF, the pump stops with it)
    if (!EM_ASM_INT(
    {
        const r = globalThis.__xuoRing;
        if (!r)
            return 0;
        return (Atomics.load(r.words, 4) === 1 && Atomics.load(r.words, 5) === 1) ? 1 : 0;
    }))
    {
        return;
    }
    // sample the free space once: the page only drains the ring, so it can
    // not shrink under us while we fill it (overruns are impossible)
    int due = EM_ASM_INT(
    {
        const r = globalThis.__xuoRing;
        return r.cap - ((Atomics.load(r.words, 0) - Atomics.load(r.words, 1)) | 0);
    });
    if (due <= 0)
    {
        return;
    }
    if (due > XUO_WEB_RING_FRAMES)
    {
        due = XUO_WEB_RING_FRAMES;
    }
    do
    {
        const int chunk = due < XUO_WEB_MIX_CHUNK ? due : XUO_WEB_MIX_CHUNK;
        s_backend.mix(s_web_mixBuffer, chunk);
        EM_ASM(
        {
            const r = globalThis.__xuoRing;
            const frames = $1;
            const src = HEAPF32.subarray($0 >> 2, ($0 >> 2) + frames * 2);
            const off = (Atomics.load(r.words, 0) % r.cap) * 2;
            const first = Math.min(frames * 2, r.cap * 2 - off);
            r.samples.set(src.subarray(0, first), off);
            if (frames * 2 > first)
            {
                r.samples.set(src.subarray(first), 0);
            }
            Atomics.add(r.words, 0, frames);
        },
        reinterpret_cast<intptr_t>(s_web_mixBuffer), chunk);
        due -= chunk;
    } while (due > 0);
#endif
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
    if (volume <= 0.001f || clientConfigVolume == 0)
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

void CSoundManager::PlayMP3(const char *fileName, int index, bool loop, bool warmode)
{
    SOUND_DEBUG_TRACE;
    int cur = warmode ? 1 : 0;
    int old = warmode ? 0 : 1;
#if defined(XUO_EMSC)
    // music DATA lane (web): complete the mp3 through the stream-file
    // registry (bounded, pool-driven), then hand the finished bytes to
    // SoLoud as memory. Two reasons to never let WavStream touch the file:
    // the lane materializes at the plan's case-exact install path (MEMFS
    // is case-sensitive) while the music index spells the config's
    // lowercase name, and streaming would re-open the file mid-mix. The
    // registry is keyed by the bare plan name (the caller passes an
    // install path)
    bool musicLoaded = false;
    if (fileName != nullptr)
    {
        const astr_t lower = str_lower(astr_t(fileName));
        const auto at = lower.find("music/");
        if (at != astr_t::npos)
        {
            const char *key = lower.c_str() + at;
            if (vfs_stream_file_ensure(key))
            {
                vfs_stream_file_stats_entry entry[32] = {};
                uint64_t total = 0;
                const int entries = vfs_stream_file_stats(entry, 32);
                for (int i = 0; i < entries; i++)
                {
                    if (strcmp(entry[i].name, key) == 0 && entry[i].phase == 2)
                    {
                        total = entry[i].inflated_total;
                        break;
                    }
                }
                if (total > 0 && total <= (64u << 20))
                {
                    std::vector<uint8_t> bytes;
                    bytes.resize(size_t(total));
                    bool complete = true;
                    for (uint64_t off = 0; off < total;)
                    {
                        const uint32_t chunk =
                            uint32_t((total - off) > (1u << 20) ? (1u << 20) : (total - off));
                        const uint8_t *window = vfs_stream_file_read(key, size_t(off), chunk);
                        if (window == nullptr)
                        {
                            complete = false;
                            break;
                        }
                        memcpy(bytes.data() + off, window, chunk);
                        off += chunk;
                    }
                    if (complete)
                    {
                        musicLoaded =
                            s_MusicStream[cur].loadMem(bytes.data(), int(bytes.size()), true, false) == 0;
                    }
                }
            }
        }
    }
#else
    const bool musicLoaded = false;
#endif
    if (s_Music[cur])
    {
        s_backend.stop(s_Music[cur]);
    }

    if (!musicLoaded)
    {
        s_MusicStream[cur].load(fileName);
    }
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
        auto path = str_from(FilePath("uo_4mb_2.sf2"));
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
