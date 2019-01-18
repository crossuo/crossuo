// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../IndexObject.h"

class CSoundManager
{
public:
    int CurrentMusicIndex = -1;

    CSoundManager();
    ~CSoundManager();

    bool Init();
    void Free();
    void ResumeSound();
    void PauseSound();
    void FreeSound(SoundHandle stream);
    bool IsPlayingNormalMusic();
    float GetVolumeValue(int distance = -1, bool music = false);

    SoundHandle LoadSoundEffect(CIndexSound &is);
    bool UpdateSoundEffect(SoundHandle stream, float volume);
    void PlaySoundEffect(SoundHandle stream, float volume);
    void PlayMidi(int index, bool warmode);
    void PlayMP3(const string &fileName, int index, bool loop, bool warmode = false);
    void StopMusic();
    void StopWarMusic();
    void SetMusicVolume(float volume);
};

extern CSoundManager g_SoundManager;
