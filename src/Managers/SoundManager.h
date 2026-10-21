// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

struct CIndexSound;

struct SoundInfo;
typedef SoundInfo *SoundHandle;
#define SOUND_NULL nullptr

class CSoundManager
{
public:
    int CurrentMusicIndex = -1;

    CSoundManager();
    ~CSoundManager();

    bool Init();
    void Free();
    // web only: per-frame pump of SoLoud's manual mix() into the page's
    // AudioContext (no-op on native builds - the backend thread mixes there)
    void Update() const;
    void ResumeSound();
    void PauseSound();
    void FreeSound(SoundHandle stream);
    bool IsPlayingNormalMusic();
    float GetVolumeValue(int distance = -1, bool music = false);

    SoundHandle LoadSoundEffect(CIndexSound &is);
    bool UpdateSoundEffect(SoundHandle stream, float volume);
    void PlaySoundEffect(SoundHandle stream, float volume);
    void PlayMidi(int index, bool warmode);
    void PlayMP3(const char *fileName, int index, bool loop, bool warmode = false);
    void StopMusic();
    void StopWarMusic();
    void SetMusicVolume(float volume);
};

extern CSoundManager g_SoundManager;
