// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#pragma pack(push, 1)
struct MidiInfoStruct
{
    const char *musicName;
    bool loop;
};
struct WaveHeader
{
    char chunkId[4];
    unsigned long chunkSize;
    char format[4];
    char subChunkId[4];
    unsigned long subChunkSize;
    unsigned short audioFormat;
    unsigned short numChannels;
    unsigned long sampleRate;
    unsigned long bytesPerSecond;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
    char dataChunkId[4];
    unsigned long dataSize;
    //data;
};
#pragma pack(pop)

class CSoundManager
{
public:
    int CurrentMusicIndex = -1;

private:
    static const int MIDI_MUSIC_COUNT = 57;
    static const MidiInfoStruct MidiInfo[MIDI_MUSIC_COUNT];
    HSTREAM m_Music{};
    HSTREAM m_WarMusic{};

    void TraceMusicError(DWORD error);

public:
    CSoundManager();
    ~CSoundManager();

    bool Init();
    void Free();
    void ResumeSound();
    void PauseSound();
    bool FreeStream(HSTREAM hSteam);
    bool IsPlayingNormalMusic();
    float GetVolumeValue(int distance = -1, bool music = false);

    HSTREAM LoadSoundEffect(CIndexSound &is);
    vector<uint8_t> CreateWaveFile(CIndexSound &is);

    void PlaySoundEffect(HSTREAM stream, float volume);
    void PlayMidi(int index, bool warmode);
    void PlayMP3(const os_path &fileName, int index, bool loop, bool warmode = false);
    void StopMusic();
    void StopWarMusic();
    void SetMusicVolume(float volume);
};

extern CSoundManager g_SoundManager;
