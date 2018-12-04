// ASS - Audio Stupidly Simple

#pragma once

#define DR_MP3_NO_STDIO
#define DR_MP3_FLOAT_OUTPUT
#include "dr_mp3.h"

#define DR_WAV_NO_STDIO
#include "dr_wav.h"

#define TSF_NO_STDIO
#include "tsf.h"
#define TML_NO_STDIO
#include "tml.h"

/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_H
#define SOLOUD_H

#include <stdlib.h> // rand
#include <math.h> // sin

#ifdef SOLOUD_NO_ASSERTS
#define SOLOUD_ASSERT(x)
#else
#ifdef _MSC_VER
#include <stdio.h> // for sprintf in asserts
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // only needed for OutputDebugStringA, should be solved somehow.
#define SOLOUD_ASSERT(x) if (!(x)) { char temp[200]; sprintf(temp, "%s(%d): assert(%s) failed.\n", __FILE__, __LINE__, #x); OutputDebugStringA(temp); __debugbreak(); }
#else
#include <assert.h> // assert
#define SOLOUD_ASSERT(x) assert(x)
#endif
#endif

#ifdef WITH_SDL
#undef WITH_SDL2
#undef WITH_SDL1
#define WITH_SDL1
#define WITH_SDL2
#endif

#ifdef WITH_SDL_STATIC
#undef WITH_SDL1_STATIC
#define WITH_SDL1_STATIC
#endif

#ifndef M_PI
#define M_PI 3.14159265359
#endif

#if defined(_WIN32)||defined(_WIN64)
#define WINDOWS_VERSION
#endif

#if !defined(DISABLE_SIMD)
#if defined(__x86_64__) || defined( _M_X64 ) || defined( __i386 ) || defined( _M_IX86 )
#define SOLOUD_SSE_INTRINSICS
#endif
#endif

#define SOLOUD_VERSION 201811

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// Configuration defines

// Maximum number of filters per stream
#define FILTERS_PER_STREAM 8

// Number of samples to process on one go
#define SAMPLE_GRANULARITY 512

// Maximum number of concurrent voices (hard limit is 4095)
#define VOICE_COUNT 1024

// Use linear resampler
#define RESAMPLER_LINEAR

// 1)mono, 2)stereo 4)quad 6)5.1 8)7.1
#define MAX_CHANNELS 8

//
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

// Typedefs have to be made before the includes, as the
// includes depend on them.
namespace SoLoud
{
	class Soloud;
	typedef void (*mutexCallFunction)(void *aMutexPtr);
	typedef void (*soloudCallFunction)(Soloud *aSoloud);
	typedef unsigned int result;
	typedef unsigned int handle;
	typedef double time;
};

namespace SoLoud
{
	// Class that handles aligned allocations to support vectorized operations
	class AlignedFloatBuffer
	{
	public:
		float *mData; // aligned pointer
		unsigned char *mBasePtr; // raw allocated pointer (for delete)
		int mFloats; // size of buffer (w/out padding)

		// ctor
		AlignedFloatBuffer();
		// Allocate and align buffer
		result init(unsigned int aFloats);
		// Clear data to zero.
		void clear();
		// dtor
		~AlignedFloatBuffer();
	};

	// Lightweight class that handles small aligned buffer to support vectorized operations
	class TinyAlignedFloatBuffer
	{
	public:
		float *mData; // aligned pointer
		unsigned char mActualData[sizeof(float) * 16 + 16];

		// ctor
		TinyAlignedFloatBuffer();
	};
};

// including file: soloud_filter.h
// BEGIN file: #include "soloud_filter.h"
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_FILTER_H
#define SOLOUD_FILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class Fader;

	class FilterInstance
	{
	public:
		unsigned int mNumParams;
		unsigned int mParamChanged;
		float *mParam;
		Fader *mParamFader;


		FilterInstance();
		virtual result initParams(int aNumParams);
		virtual void updateParams(time aTime);
		virtual void filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, time aTime);
		virtual void filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		virtual float getFilterParameter(unsigned int aAttributeId);
		virtual void setFilterParameter(unsigned int aAttributeId, float aValue);
		virtual void fadeFilterParameter(unsigned int aAttributeId, float aTo, time aTime, time aStartTime);
		virtual void oscillateFilterParameter(unsigned int aAttributeId, float aFrom, float aTo, time aTime, time aStartTime);
		virtual ~FilterInstance();
	};

	class Filter
	{
	public:
		Filter();
		virtual FilterInstance *createInstance() = 0;
		virtual ~Filter();
	};
};

#endif

// END file: #include "soloud_filter.h"

// including file: soloud_fader.h
// BEGIN file: #include "soloud_fader.h"
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_FADER_H
#define SOLOUD_FADER_H

// #include "soloud.h"

namespace SoLoud
{
	// Helper class to process faders
	class Fader
	{
	public:
		// Value to fade from
		float mFrom;
		// Value to fade to
		float mTo;
		// Delta between from and to
		float mDelta;
		// Total time to fade
		time mTime;
		// Time fading started
		time mStartTime;
		// Time fading will end
		time mEndTime;
		// Current value. Used in case time rolls over.
		float mCurrent;
		// Active flag; 0 means disabled, 1 is active, 2 is LFO, -1 means was active, but stopped
		int mActive;
		// Ctor
		Fader();
		// Set up LFO
		void setLFO(float aFrom, float aTo, time aTime, time aStartTime);
		// Set up fader
		void set(float aFrom, float aTo, time aTime, time aStartTime);
		// Get the current fading value
		float get(time aCurrentTime);
	};
};

#endif

// END file: #include "soloud_fader.h"

// including file: soloud_audiosource.h
// BEGIN file: #include "soloud_audiosource.h"
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_AUDIOSOURCE_H
#define SOLOUD_AUDIOSOURCE_H

// #include "soloud.h"
// #include "soloud_fader.h"
// #include "soloud_filter.h"

namespace SoLoud
{
	class AudioSource;
	class AudioSourceInstance;
	class AudioSourceInstance3dData;

	class AudioCollider
	{
	public:
		// Calculate volume multiplier. Assumed to return value between 0 and 1.
		virtual float collide(Soloud *aSoloud, AudioSourceInstance3dData *aAudioInstance3dData,	int aUserData) = 0;
	};

	class AudioAttenuator
	{
	public:
		virtual float attenuate(float aDistance, float aMinDistance, float aMaxDistance, float aRolloffFactor) = 0;
	};

	class AudioSourceInstance3dData
	{
	public:
		// ctor
		AudioSourceInstance3dData();
		// Set settings from audiosource
		void init(AudioSource &aSource);
		// 3d position
		float m3dPosition[3];
		// 3d velocity
		float m3dVelocity[3];
		// 3d cone direction
		/*
		float m3dConeDirection[3];
		// 3d cone inner angle
		float m3dConeInnerAngle;
		// 3d cone outer angle
		float m3dConeOuterAngle;
		// 3d cone outer volume multiplier
		float m3dConeOuterVolume;
		*/
		// 3d min distance
		float m3dMinDistance;
		// 3d max distance
		float m3dMaxDistance;
		// 3d attenuation rolloff factor
		float m3dAttenuationRolloff;
		// 3d attenuation model
		unsigned int m3dAttenuationModel;
		// 3d doppler factor
		float m3dDopplerFactor;
		// Pointer to a custom audio collider object
		AudioCollider *mCollider;
		// Pointer to a custom audio attenuator object
		AudioAttenuator *mAttenuator;
		// User data related to audio collider
		int mColliderData;

		// Doppler sample rate multiplier
		float mDopplerValue;
		// Overall 3d volume
		float m3dVolume;
		// Channel volume
		float mChannelVolume[MAX_CHANNELS];
		// Copy of flags
		unsigned int mFlags;
		// Latest handle for this voice
		handle mHandle;
	};

	// Base class for audio instances
	class AudioSourceInstance
	{
	public:
		enum FLAGS
		{
			// This audio instance loops (if supported)
			LOOPING = 1,
			// This audio instance is protected - won't get stopped if we run out of voices
			PROTECTED = 2,
			// This audio instance is paused
			PAUSED = 4,
			// This audio instance is affected by 3d processing
			PROCESS_3D = 8,
			// This audio instance has listener-relative 3d coordinates
			LISTENER_RELATIVE = 16,
			// Currently inaudible
			INAUDIBLE = 32,
			// If inaudible, should be killed (default = don't kill kill)
			INAUDIBLE_KILL = 64,
			// If inaudible, should still be ticked (default = pause)
			INAUDIBLE_TICK = 128
		};
		// Ctor
		AudioSourceInstance();
		// Dtor
		virtual ~AudioSourceInstance();
		// Play index; used to identify instances from handles
		unsigned int mPlayIndex;
		// Loop count
		unsigned int mLoopCount;
		// Flags; see AudioSourceInstance::FLAGS
		unsigned int mFlags;
		// Pan value, for getPan()
		float mPan;
		// Volume for each channel (panning)
		float mChannelVolume[MAX_CHANNELS];
		// Set volume
		float mSetVolume;
		// Overall volume overall = set * 3d
		float mOverallVolume;
		// Base samplerate; samplerate = base samplerate * relative play speed
		float mBaseSamplerate;
		// Samplerate; samplerate = base samplerate * relative play speed
		float mSamplerate;
		// Number of channels this audio source produces
		unsigned int mChannels;
		// Relative play speed; samplerate = base samplerate * relative play speed
		float mSetRelativePlaySpeed;
		// Overall relative plays peed; overall = set * 3d
		float mOverallRelativePlaySpeed;
		// How long this stream has played, in seconds.
		time mStreamTime;
		// Position of this stream, in seconds.
		time mStreamPosition;
		// Fader for the audio panning
		Fader mPanFader;
		// Fader for the audio volume
		Fader mVolumeFader;
		// Fader for the relative play speed
		Fader mRelativePlaySpeedFader;
		// Fader used to schedule pausing of the stream
		Fader mPauseScheduler;
		// Fader used to schedule stopping of the stream
		Fader mStopScheduler;
		// Affected by some fader
		int mActiveFader;
		// Current channel volumes, used to ramp the volume changes to avoid clicks
		float mCurrentChannelVolume[MAX_CHANNELS];
		// ID of the sound source that generated this instance
		unsigned int mAudioSourceID;
		// Handle of the bus this audio instance is playing on. 0 for root.
		unsigned int mBusHandle;
		// Filter pointer
		FilterInstance *mFilter[FILTERS_PER_STREAM];
		// Initialize instance. Mostly internal use.
		void init(AudioSource &aSource, int aPlayIndex);
		// Buffers for the resampler
		AlignedFloatBuffer *mResampleData[2];
		// Sub-sample playhead; 16.16 fixed point
		unsigned int mSrcOffset;
		// Samples left over from earlier pass
		unsigned int mLeftoverSamples;
		// Number of samples to delay streaming
		unsigned int mDelaySamples;
		// When looping, start playing from this time
		time mLoopPoint;

		// Get N samples from the stream to the buffer. Report samples written.
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize) = 0;
		// Has the stream ended?
		virtual bool hasEnded() = 0;
		// Seek to certain place in the stream. Base implementation is generic "tape" seek (and slow).
		virtual result seek(time aSeconds, float *mScratch, unsigned int mScratchSize);
		// Rewind stream. Base implementation returns NOT_IMPLEMENTED, meaning it can't rewind.
		virtual result rewind();
		// Get information. Returns 0 by default.
		virtual float getInfo(unsigned int aInfoKey);
	};

	class Soloud;

	// Base class for audio sources
	class AudioSource
	{
	public:
		enum FLAGS
		{
			// The instances from this audio source should loop
			SHOULD_LOOP = 1,
			// Only one instance of this audio source should play at the same time
			SINGLE_INSTANCE = 2,
			// Visualization data gathering enabled. Only for busses.
			VISUALIZATION_DATA = 4,
			// Audio instances created from this source are affected by 3d processing
			PROCESS_3D = 8,
			// Audio instances created from this source have listener-relative 3d coordinates
			LISTENER_RELATIVE = 16,
			// Delay start of sound by the distance from listener
			DISTANCE_DELAY = 32,
			// If inaudible, should be killed (default)
			INAUDIBLE_KILL = 64,
			// If inaudible, should still be ticked (default = pause)
			INAUDIBLE_TICK = 128
		};
		enum ATTENUATION_MODELS
		{
			// No attenuation
			NO_ATTENUATION = 0,
			// Inverse distance attenuation model
			INVERSE_DISTANCE = 1,
			// Linear distance attenuation model
			LINEAR_DISTANCE = 2,
			// Exponential distance attenuation model
			EXPONENTIAL_DISTANCE = 3
		};

		// Flags. See AudioSource::FLAGS
		unsigned int mFlags;
		// Base sample rate, used to initialize instances
		float mBaseSamplerate;
		// Default volume for created instances
		float mVolume;
		// Number of channels this audio source produces
		unsigned int mChannels;
		// Sound source ID. Assigned by SoLoud the first time it's played.
		unsigned int mAudioSourceID;
		// 3d min distance
		float m3dMinDistance;
		// 3d max distance
		float m3dMaxDistance;
		// 3d attenuation rolloff factor
		float m3dAttenuationRolloff;
		// 3d attenuation model
		unsigned int m3dAttenuationModel;
		// 3d doppler factor
		float m3dDopplerFactor;
		// Filter pointer
		Filter *mFilter[FILTERS_PER_STREAM];
		// Pointer to the Soloud object. Needed to stop all instances in dtor.
		Soloud *mSoloud;
		// Pointer to a custom audio collider object
		AudioCollider *mCollider;
		// Pointer to custom attenuator object
		AudioAttenuator *mAttenuator;
		// User data related to audio collider
		int mColliderData;
		// When looping, start playing from this time
		time mLoopPoint;

		// CTor
		AudioSource();
		// Set default volume for instances
		void setVolume(float aVolume);
		// Set the looping of the instances created from this audio source
		void setLooping(bool aLoop);
		// Set whether only one instance of this sound should ever be playing at the same time
		void setSingleInstance(bool aSingleInstance);

		// Set the minimum and maximum distances for 3d audio source (closer to min distance = max vol)
		void set3dMinMaxDistance(float aMinDistance, float aMaxDistance);
		// Set attenuation model and rolloff factor for 3d audio source
		void set3dAttenuation(unsigned int aAttenuationModel, float aAttenuationRolloffFactor);
		// Set doppler factor to reduce or enhance doppler effect, default = 1.0
		void set3dDopplerFactor(float aDopplerFactor);
		// Set the coordinates for this audio source to be relative to listener's coordinates.
		void set3dListenerRelative(bool aListenerRelative);
		// Enable delaying the start of the sound based on the distance.
		void set3dDistanceDelay(bool aDistanceDelay);

		// Set a custom 3d audio collider. Set to NULL to disable.
		void set3dCollider(AudioCollider *aCollider, int aUserData = 0);
		// Set a custom attenuator. Set to NULL to disable.
		void set3dAttenuator(AudioAttenuator *aAttenuator);

		// Set behavior for inaudible sounds
		void setInaudibleBehavior(bool aMustTick, bool aKill);

		// Set time to jump to when looping
		void setLoopPoint(time aLoopPoint);
		// Get current loop point value
		time getLoopPoint();

		// Set filter. Set to NULL to clear the filter.
		virtual void setFilter(unsigned int aFilterId, Filter *aFilter);
		// DTor
		virtual ~AudioSource();
		// Create instance from the audio source. Called from within Soloud class.
		virtual AudioSourceInstance *createInstance() = 0;
		// Stop all instances of this audio source
		void stop();
	};
};

#endif

// END file: #include "soloud_audiosource.h"

// including file: soloud_bus.h
// BEGIN file: #include "soloud_bus.h"
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_BUS_H
#define SOLOUD_BUS_H

// #include "soloud.h"

namespace SoLoud
{
	class Bus;

	class BusInstance : public AudioSourceInstance
	{
		Bus *mParent;
		unsigned int mScratchSize;
		AlignedFloatBuffer mScratch;
	public:
		// Approximate volume for channels.
		float mVisualizationChannelVolume[MAX_CHANNELS];
		// Mono-mixed wave data for visualization and for visualization FFT input
		float mVisualizationWaveData[256];

		BusInstance(Bus *aParent);
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual bool hasEnded();
		virtual ~BusInstance();
	};

	class Bus : public AudioSource
	{
	public:
		Bus();
		virtual BusInstance *createInstance();
		// Set filter. Set to NULL to clear the filter.
		virtual void setFilter(unsigned int aFilterId, Filter *aFilter);
		// Play sound through the bus
		handle play(AudioSource &aSound, float aVolume = 1.0f, float aPan = 0.0f, bool aPaused = 0);
		// Play sound through the bus, delayed in relation to other sounds called via this function.
		handle playClocked(time aSoundTime, AudioSource &aSound, float aVolume = 1.0f, float aPan = 0.0f);
		// Start playing a 3d audio source through the bus
		handle play3d(AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX = 0.0f, float aVelY = 0.0f, float aVelZ = 0.0f, float aVolume = 1.0f, bool aPaused = 0);
		// Start playing a 3d audio source through the bus, delayed in relation to other sounds called via this function.
		handle play3dClocked(time aSoundTime, AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX = 0.0f, float aVelY = 0.0f, float aVelZ = 0.0f, float aVolume = 1.0f);
		// Set number of channels for the bus (default 2)
		result setChannels(unsigned int aChannels);
		// Enable or disable visualization data gathering
		void setVisualizationEnable(bool aEnable);

		// Calculate and get 256 floats of FFT data for visualization. Visualization has to be enabled before use.
		float *calcFFT();

		// Get 256 floats of wave data for visualization. Visualization has to be enabled before use.
		float *getWave();

		// Get approximate volume for output channel for visualization. Visualization has to be enabled before use.
		float getApproximateVolume(unsigned int aChannel);
	public:
		BusInstance *mInstance;
		unsigned int mChannelHandle;
		// FFT output data
		float mFFTData[256];
		// Snapshot of wave data for visualization
		float mWaveData[256];
		// Internal: find the bus' channel
		void findBusHandle();
	};
};

#endif

// END file: #include "soloud_bus.h"

// including file: soloud_queue.h
// BEGIN file: #include "soloud_queue.h"
/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_QUEUE_H
#define SOLOUD_QUEUE_H

// #include "soloud.h"

#define SOLOUD_QUEUE_MAX 32

namespace SoLoud
{
	class Queue;

	class QueueInstance : public AudioSourceInstance
	{
		Queue *mParent;
	public:
		QueueInstance(Queue *aParent);
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual bool hasEnded();
		virtual ~QueueInstance();
	};

	class Queue : public AudioSource
	{
	public:
		Queue();
		virtual QueueInstance *createInstance();
		// Play sound through the queue
		result play(AudioSource &aSound);
        // Number of audio sources queued for replay
        unsigned int getQueueCount();
		// Is this audio source currently playing?
		bool isCurrentlyPlaying(AudioSource &aSound);
		// Set params by reading them from an audio source
		result setParamsFromAudioSource(AudioSource &aSound);
		// Set params manually
		result setParams(float aSamplerate, unsigned int aChannels = 2);

	public:
	    unsigned int mReadIndex, mWriteIndex, mCount;
	    AudioSourceInstance *mSource[SOLOUD_QUEUE_MAX];
		QueueInstance *mInstance;
		handle mQueueHandle;
		void findQueueHandle();

	};
};

#endif

// END file: #include "soloud_queue.h"

// including file: soloud_error.h
// BEGIN file: #include "soloud_error.h"
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/
#ifndef SOLOUD_ERROR_H
#define SOLOUD_ERROR_H

namespace SoLoud
{
	enum SOLOUD_ERRORS
	{
		SO_NO_ERROR       = 0, // No error
		INVALID_PARAMETER = 1, // Some parameter is invalid
		FILE_NOT_FOUND    = 2, // File not found
		FILE_LOAD_FAILED  = 3, // File found, but could not be loaded
		DLL_NOT_FOUND     = 4, // DLL not found, or wrong DLL
		OUT_OF_MEMORY     = 5, // Out of memory
		NOT_IMPLEMENTED   = 6, // Feature not implemented
		UNKNOWN_ERROR     = 7  // Other error
	};
};
#endif

// END file: #include "soloud_error.h"


namespace SoLoud
{

	// Soloud core class.
	class Soloud
	{
	public:
		// Back-end data; content is up to the back-end implementation.
		void * mBackendData;
		// Pointer for the audio thread mutex.
		void * mAudioThreadMutex;
		// Flag for when we're inside the mutex, used for debugging.
		bool mInsideAudioThreadMutex;
		// Called by SoLoud to shut down the back-end. If NULL, not called. Should be set by back-end.
		soloudCallFunction mBackendCleanupFunc;

		// CTor
		Soloud();
		// DTor
		~Soloud();

		enum BACKENDS
		{
			AUTO = 0,
			SDL1,
			SDL2,
			PORTAUDIO,
			WINMM,
			XAUDIO2,
			WASAPI,
			ALSA,
			OSS,
			OPENAL,
			COREAUDIO,
			OPENSLES,
			VITA_HOMEBREW,
			NULLDRIVER,
			BACKEND_MAX,
		};

		enum FLAGS
		{
			// Use round-off clipper
			CLIP_ROUNDOFF = 1,
			ENABLE_VISUALIZATION = 2,
			LEFT_HANDED_3D = 4
		};

		// Initialize SoLoud. Must be called before SoLoud can be used.
		result init(unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aBackend = Soloud::AUTO, unsigned int aSamplerate = Soloud::AUTO, unsigned int aBufferSize = Soloud::AUTO, unsigned int aChannels = 2);

		// Deinitialize SoLoud. Must be called before shutting down.
		void deinit();

		// Query SoLoud version number (should equal to SOLOUD_VERSION macro)
		unsigned int getVersion() const;

		// Translate error number to an asciiz string
		const char * getErrorString(result aErrorCode) const;

		// Returns current backend ID (BACKENDS enum)
		unsigned int getBackendId();
		// Returns current backend string. May be NULL.
		const char * getBackendString();
		// Returns current backend channel count (1 mono, 2 stereo, etc)
		unsigned int getBackendChannels();
		// Returns current backend sample rate
		unsigned int getBackendSamplerate();
		// Returns current backend buffer size
		unsigned int getBackendBufferSize();

		// Set speaker position in 3d space
		result setSpeakerPosition(unsigned int aChannel, float aX, float aY, float aZ);
		// Get speaker position in 3d space
		result getSpeakerPosition(unsigned int aChannel, float &aX, float &aY, float &aZ);

		// Start playing a sound. Returns voice handle, which can be ignored or used to alter the playing sound's parameters. Negative volume means to use default.
		handle play(AudioSource &aSound, float aVolume = -1.0f, float aPan = 0.0f, bool aPaused = 0, unsigned int aBus = 0);
		// Start playing a sound delayed in relation to other sounds called via this function. Negative volume means to use default.
		handle playClocked(time aSoundTime, AudioSource &aSound, float aVolume = -1.0f, float aPan = 0.0f, unsigned int aBus = 0);
		// Start playing a 3d audio source
		handle play3d(AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX = 0.0f, float aVelY = 0.0f, float aVelZ = 0.0f, float aVolume = 1.0f, bool aPaused = 0, unsigned int aBus = 0);
		// Start playing a 3d audio source, delayed in relation to other sounds called via this function.
		handle play3dClocked(time aSoundTime, AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX = 0.0f, float aVelY = 0.0f, float aVelZ = 0.0f, float aVolume = 1.0f, unsigned int aBus = 0);
		// Start playing a sound without any panning. It will be played at full volume.
		handle playBackground(AudioSource &aSound, float aVolume = -1.0f, bool aPaused = 0, unsigned int aBus = 0);

		// Seek the audio stream to certain point in time. Some streams can't seek backwards. Relative play speed affects time.
		result seek(handle aVoiceHandle, time aSeconds);
		// Stop the sound.
		void stop(handle aVoiceHandle);
		// Stop all voices.
		void stopAll();
		// Stop all voices that play this sound source
		void stopAudioSource(AudioSource &aSound);
		// Count voices that play this audio source
		int countAudioSource(AudioSource &aSound);

		// Set a live filter parameter. Use 0 for the global filters.
		void setFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId, float aValue);
		// Get a live filter parameter. Use 0 for the global filters.
		float getFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId);
		// Fade a live filter parameter. Use 0 for the global filters.
		void fadeFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId, float aTo, time aTime);
		// Oscillate a live filter parameter. Use 0 for the global filters.
		void oscillateFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId, float aFrom, float aTo, time aTime);

		// Get current play time, in seconds.
		time getStreamTime(handle aVoiceHandle);
		// Get current sample position, in seconds.
		time getStreamPosition(handle aVoiceHandle);
		// Get current pause state.
		bool getPause(handle aVoiceHandle);
		// Get current volume.
		float getVolume(handle aVoiceHandle);
		// Get current overall volume (set volume * 3d volume)
		float getOverallVolume(handle aVoiceHandle);
		// Get current pan.
		float getPan(handle aVoiceHandle);
		// Get current sample rate.
		float getSamplerate(handle aVoiceHandle);
		// Get current voice protection state.
		bool getProtectVoice(handle aVoiceHandle);
		// Get the current number of busy voices.
		unsigned int getActiveVoiceCount();
		// Get the current number of voices in SoLoud
		unsigned int getVoiceCount();
		// Check if the handle is still valid, or if the sound has stopped.
		bool isValidVoiceHandle(handle aVoiceHandle);
		// Get current relative play speed.
		float getRelativePlaySpeed(handle aVoiceHandle);
		// Get current post-clip scaler value.
		float getPostClipScaler() const;
		// Get current global volume
		float getGlobalVolume() const;
		// Get current maximum active voice setting
		unsigned int getMaxActiveVoiceCount() const;
		// Query whether a voice is set to loop.
		bool getLooping(handle aVoiceHandle);
		// Get voice loop point value
		time getLoopPoint(handle aVoiceHandle);

		// Set voice loop point value
		void setLoopPoint(handle aVoiceHandle, time aLoopPoint);
		// Set voice's loop state
		void setLooping(handle aVoiceHandle, bool aLooping);
		// Set current maximum active voice setting
		result setMaxActiveVoiceCount(unsigned int aVoiceCount);
		// Set behavior for inaudible sounds
		void setInaudibleBehavior(handle aVoiceHandle, bool aMustTick, bool aKill);
		// Set the global volume
		void setGlobalVolume(float aVolume);
		// Set the post clip scaler value
		void setPostClipScaler(float aScaler);
		// Set the pause state
		void setPause(handle aVoiceHandle, bool aPause);
		// Pause all voices
		void setPauseAll(bool aPause);
		// Set the relative play speed
		result setRelativePlaySpeed(handle aVoiceHandle, float aSpeed);
		// Set the voice protection state
		void setProtectVoice(handle aVoiceHandle, bool aProtect);
		// Set the sample rate
		void setSamplerate(handle aVoiceHandle, float aSamplerate);
		// Set panning value; -1 is left, 0 is center, 1 is right
		void setPan(handle aVoiceHandle, float aPan);
		// Set absolute left/right volumes
		void setPanAbsolute(handle aVoiceHandle, float aLVolume, float aRVolume, float aLBVolume = 0, float aRBVolume = 0, float aCVolume = 0, float aSVolume = 0);
		// Set overall volume
		void setVolume(handle aVoiceHandle, float aVolume);
		// Set delay, in samples, before starting to play samples. Calling this on a live sound will cause glitches.
		void setDelaySamples(handle aVoiceHandle, unsigned int aSamples);

		// Set up volume fader
		void fadeVolume(handle aVoiceHandle, float aTo, time aTime);
		// Set up panning fader
		void fadePan(handle aVoiceHandle, float aTo, time aTime);
		// Set up relative play speed fader
		void fadeRelativePlaySpeed(handle aVoiceHandle, float aTo, time aTime);
		// Set up global volume fader
		void fadeGlobalVolume(float aTo, time aTime);
		// Schedule a stream to pause
		void schedulePause(handle aVoiceHandle, time aTime);
		// Schedule a stream to stop
		void scheduleStop(handle aVoiceHandle, time aTime);

		// Set up volume oscillator
		void oscillateVolume(handle aVoiceHandle, float aFrom, float aTo, time aTime);
		// Set up panning oscillator
		void oscillatePan(handle aVoiceHandle, float aFrom, float aTo, time aTime);
		// Set up relative play speed oscillator
		void oscillateRelativePlaySpeed(handle aVoiceHandle, float aFrom, float aTo, time aTime);
		// Set up global volume oscillator
		void oscillateGlobalVolume(float aFrom, float aTo, time aTime);

		// Set global filters. Set to NULL to clear the filter.
		void setGlobalFilter(unsigned int aFilterId, Filter *aFilter);

		// Enable or disable visualization data gathering
		void setVisualizationEnable(bool aEnable);

		// Calculate and get 256 floats of FFT data for visualization. Visualization has to be enabled before use.
		float *calcFFT();

		// Get 256 floats of wave data for visualization. Visualization has to be enabled before use.
		float *getWave();

		// Get approximate output volume for a channel for visualization. Visualization has to be enabled before use.
		float getApproximateVolume(unsigned int aChannel);

		// Get current loop count. Returns 0 if handle is not valid. (All audio sources may not update loop count)
		unsigned int getLoopCount(handle aVoiceHandle);

		// Get audiosource-specific information from a voice.
		float getInfo(handle aVoiceHandle, unsigned int aInfoKey);

		// Create a voice group. Returns 0 if unable (out of voice groups / out of memory)
		handle createVoiceGroup();
		// Destroy a voice group.
		result destroyVoiceGroup(handle aVoiceGroupHandle);
		// Add a voice handle to a voice group
		result addVoiceToGroup(handle aVoiceGroupHandle, handle aVoiceHandle);
		// Is this handle a valid voice group?
		bool isVoiceGroup(handle aVoiceGroupHandle);
		// Is this voice group empty?
		bool isVoiceGroupEmpty(handle aVoiceGroupHandle);

		// Perform 3d audio parameter update
		void update3dAudio();

		// Set the speed of sound constant for doppler
		result set3dSoundSpeed(float aSpeed);
		// Get the current speed of sound constant for doppler
		float get3dSoundSpeed();
		// Set 3d listener parameters
		void set3dListenerParameters(float aPosX, float aPosY, float aPosZ, float aAtX, float aAtY, float aAtZ, float aUpX, float aUpY, float aUpZ, float aVelocityX = 0.0f, float aVelocityY = 0.0f, float aVelocityZ = 0.0f);
		// Set 3d listener position
		void set3dListenerPosition(float aPosX, float aPosY, float aPosZ);
		// Set 3d listener "at" vector
		void set3dListenerAt(float aAtX, float aAtY, float aAtZ);
		// set 3d listener "up" vector
		void set3dListenerUp(float aUpX, float aUpY, float aUpZ);
		// Set 3d listener velocity
		void set3dListenerVelocity(float aVelocityX, float aVelocityY, float aVelocityZ);

		// Set 3d audio source parameters
		void set3dSourceParameters(handle aVoiceHandle, float aPosX, float aPosY, float aPosZ, float aVelocityX = 0.0f, float aVelocityY = 0.0f, float aVelocityZ = 0.0f);
		// Set 3d audio source position
		void set3dSourcePosition(handle aVoiceHandle, float aPosX, float aPosY, float aPosZ);
		// Set 3d audio source velocity
		void set3dSourceVelocity(handle aVoiceHandle, float aVelocityX, float aVelocityY, float aVelocityZ);
		// Set 3d audio source min/max distance (distance < min means max volume)
		void set3dSourceMinMaxDistance(handle aVoiceHandle, float aMinDistance, float aMaxDistance);
		// Set 3d audio source attenuation parameters
		void set3dSourceAttenuation(handle aVoiceHandle, unsigned int aAttenuationModel, float aAttenuationRolloffFactor);
		// Set 3d audio source doppler factor to reduce or enhance doppler effect. Default = 1.0
		void set3dSourceDopplerFactor(handle aVoiceHandle, float aDopplerFactor);

		// Rest of the stuff is used internally.

		// Returns mixed float samples in buffer. Called by the back-end, or user with null driver.
		void mix(float *aBuffer, unsigned int aSamples);
		// Returns mixed 16-bit signed integer samples in buffer. Called by the back-end, or user with null driver.
		void mixSigned16(short *aBuffer, unsigned int aSamples);
	public:
		// Mix N samples * M channels. Called by other mix_ functions.
		void mix_internal(unsigned int aSamples);

		// Handle rest of initialization (called from backend)
		void postinit(unsigned int aSamplerate, unsigned int aBufferSize, unsigned int aFlags, unsigned int aChannels);

		// Update list of active voices
		void calcActiveVoices();
		// Map resample buffers to active voices
		void mapResampleBuffers();
		// Perform mixing for a specific bus
		void mixBus(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize, float *aScratch, unsigned int aBus, float aSamplerate, unsigned int aChannels);
		// Max. number of active voices. Busses and tickable inaudibles also count against this.
		unsigned int mMaxActiveVoices;
		// Highest voice in use so far
		unsigned int mHighestVoice;
		// Scratch buffer, used for resampling.
		AlignedFloatBuffer mScratch;
		// Current size of the scratch, in samples.
		unsigned int mScratchSize;
		// Amount of scratch needed.
		unsigned int mScratchNeeded;
		// Output scratch buffer, used in mix_().
		AlignedFloatBuffer mOutputScratch;
		// Resampler buffers, two per active voice.
		AlignedFloatBuffer *mResampleData;
		// Owners of the resample data
		AudioSourceInstance **mResampleDataOwner;
		// Audio voices.
		AudioSourceInstance *mVoice[VOICE_COUNT];
		// Output sample rate (not float)
		unsigned int mSamplerate;
		// Output channel count
		unsigned int mChannels;
		// Current backend ID
		unsigned int mBackendID;
		// Current backend string
		const char * mBackendString;
		// Maximum size of output buffer; used to calculate needed scratch.
		unsigned int mBufferSize;
		// Flags; see Soloud::FLAGS
		unsigned int mFlags;
		// Global volume. Applied before clipping.
		float mGlobalVolume;
		// Post-clip scaler. Applied after clipping.
		float mPostClipScaler;
		// Current play index. Used to create audio handles.
		unsigned int mPlayIndex;
		// Current sound source index. Used to create sound source IDs.
		unsigned int mAudioSourceID;
		// Fader for the global volume.
		Fader mGlobalVolumeFader;
		// Global stream time, for the global volume fader.
		time mStreamTime;
		// Last time seen by the playClocked call
		time mLastClockedTime;
		// Global filter
		Filter *mFilter[FILTERS_PER_STREAM];
		// Global filter instance
		FilterInstance *mFilterInstance[FILTERS_PER_STREAM];
		// Find a free voice, stopping the oldest if no free voice is found.
		int findFreeVoice();
		// Converts handle to voice, if the handle is valid. Returns -1 if not.
		int getVoiceFromHandle(handle aVoiceHandle) const;
		// Converts voice + playindex into handle
		handle getHandleFromVoice(unsigned int aVoice) const;
		// Stop voice (not handle).
		void stopVoice(unsigned int aVoice);
		// Set voice (not handle) pan.
		void setVoicePan(unsigned int aVoice, float aPan);
		// Set voice (not handle) relative play speed.
		result setVoiceRelativePlaySpeed(unsigned int aVoice, float aSpeed);
		// Set voice (not handle) volume.
		void setVoiceVolume(unsigned int aVoice, float aVolume);
		// Set voice (not handle) pause state.
		void setVoicePause(unsigned int aVoice, int aPause);
		// Update overall volume from set and 3d volumes
		void updateVoiceVolume(unsigned int aVoice);
		// Update overall relative play speed from set and 3d speeds
		void updateVoiceRelativePlaySpeed(unsigned int aVoice);
		// Perform 3d audio calculation for array of voices
		void update3dVoices(unsigned int *aVoiceList, unsigned int aVoiceCount);
		// Clip the samples in the buffer
		void clip(AlignedFloatBuffer &aBuffer, AlignedFloatBuffer &aDestBuffer, unsigned int aSamples, float aVolume0, float aVolume1);
		// Approximate volume for channels.
		float mVisualizationChannelVolume[MAX_CHANNELS];
		// Mono-mixed wave data for visualization and for visualization FFT input
		float mVisualizationWaveData[256];
		// FFT output data
		float mFFTData[256];
		// Snapshot of wave data for visualization
		float mWaveData[256];

		// 3d listener position
		float m3dPosition[3];
		// 3d listener look-at
		float m3dAt[3];
		// 3d listener up
		float m3dUp[3];
		// 3d listener velocity
		float m3dVelocity[3];
		// 3d speed of sound (for doppler)
		float m3dSoundSpeed;

		// 3d position of speakers
		float m3dSpeakerPosition[3 * MAX_CHANNELS];

		// Data related to 3d processing, separate from AudioSource so we can do 3d calculations without audio mutex.
		AudioSourceInstance3dData m3dData[VOICE_COUNT];

		// For each voice group, first int is number of ints alocated.
		unsigned int **mVoiceGroup;
		unsigned int mVoiceGroupCount;

		// List of currently active voices
		unsigned int mActiveVoice[VOICE_COUNT];
		// Number of currently active voices
		unsigned int mActiveVoiceCount;
		// Active voices list needs to be recalculated
		bool mActiveVoiceDirty;

		// Remove all non-active voices from group
		void trimVoiceGroup(handle aVoiceGroupHandle);
		// Get pointer to the zero-terminated array of voice handles in a voice group
		handle * voiceGroupHandleToArray(handle aVoiceGroupHandle) const;

		// Lock audio thread mutex.
		void lockAudioMutex();
		// Unlock audio thread mutex.
		void unlockAudioMutex();
	};
};

#endif


// file: soloud_fftfilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_FFTFILTER_H
#define SOLOUD_FFTFILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class FFTFilter;

	class FFTFilterInstance : public FilterInstance
	{
		float *mTemp;
		float *mInputBuffer;
		float *mMixBuffer;
		unsigned int mOffset[MAX_CHANNELS];
		FFTFilter *mParent;
	public:
		virtual void fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		virtual void filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		virtual ~FFTFilterInstance();
		FFTFilterInstance(FFTFilter *aParent);
		FFTFilterInstance();
	};

	class FFTFilter : public Filter
	{
	public:
		virtual FilterInstance *createInstance();
		FFTFilter();
	};
}

#endif


// file: soloud/include/soloud_robotizefilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_ROBOTIZEFILTER_H
#define SOLOUD_ROBOTIZEFILTER_H

// #include "soloud.h"
// #include "soloud_fftfilter.h"

namespace SoLoud
{
	class RobotizeFilter;

	class RobotizeFilterInstance : public FFTFilterInstance
	{
		enum FILTERATTRIBUTE
		{
			WET = 0
		};
		RobotizeFilter *mParent;
	public:
		virtual void fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		RobotizeFilterInstance(RobotizeFilter *aParent);
	};

	class RobotizeFilter : public FFTFilter
	{
	public:
		enum FILTERATTRIBUTE
		{
			WET = 0
		};
		float mBoost;
		virtual FilterInstance *createInstance();
		RobotizeFilter();
	};
}

#endif
// file: soloud/include/soloud_waveshaperfilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_WAVESHAPERFILTER_H
#define SOLOUD_WAVESHAPERFILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class WaveShaperFilter;

	class WaveShaperFilterInstance : public FilterInstance
	{
		WaveShaperFilter *mParent;
	public:
		virtual void filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		virtual ~WaveShaperFilterInstance();
		WaveShaperFilterInstance(WaveShaperFilter *aParent);
	};

	class WaveShaperFilter : public Filter
	{
	public:
		float mAmount, mWet;
		virtual WaveShaperFilterInstance *createInstance();
		result setParams(float aAmount, float aWet = 1.0f);
		WaveShaperFilter();
		virtual ~WaveShaperFilter();
	};
}

#endif
// file: soloud/include/soloud_dcremovalfilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_DCREMOVAL_H
#define SOLOUD_DCREMOVAL_H

// #include "soloud.h"

namespace SoLoud
{
	class DCRemovalFilter;

	class DCRemovalFilterInstance : public FilterInstance
	{
		float *mBuffer;
		float *mTotals;
		int mBufferLength;
		DCRemovalFilter *mParent;
		int mOffset;

	public:
		virtual void filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, time aTime);
		virtual ~DCRemovalFilterInstance();
		DCRemovalFilterInstance(DCRemovalFilter *aParent);
	};

	class DCRemovalFilter : public Filter
	{
	public:
		float mLength;
		virtual FilterInstance *createInstance();
		DCRemovalFilter();
		result setParams(float aLength = 0.1f);
	};
}

#endif
// file: soloud/include/soloud_flangerfilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_FLANGERFILTER_H
#define SOLOUD_FLANGERFILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class FlangerFilter;

	class FlangerFilterInstance : public FilterInstance
	{
		float *mBuffer;
		unsigned int mBufferLength;
		FlangerFilter *mParent;
		unsigned int mOffset;
		double mIndex;

	public:
		virtual void filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, time aTime);
		virtual ~FlangerFilterInstance();
		FlangerFilterInstance(FlangerFilter *aParent);
	};

	class FlangerFilter : public Filter
	{
	public:
		enum FILTERPARAMS
		{
			WET,
			DELAY,
			FREQ
		};
		float mDelay;
		float mFreq;
		virtual FilterInstance *createInstance();
		FlangerFilter();
		result setParams(float aDelay, float aFreq);
	};
}

#endif
// file: soloud/include/soloud_biquadresonantfilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_BQRFILTER_H
#define SOLOUD_BQRFILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class BiquadResonantFilter;

	struct BQRStateData
	{
		float mY1, mY2, mX1, mX2;
	};

	class BiquadResonantFilterInstance : public FilterInstance
	{
		enum FILTERATTRIBUTE
		{
			WET = 0,
			SAMPLERATE = 1,
			FREQUENCY = 2,
			RESONANCE = 3
		};

		int mActive;
		BQRStateData mState[2];
		float mA0, mA1, mA2, mB1, mB2;
		int mDirty;
		int mFilterType;

		BiquadResonantFilter *mParent;
		void calcBQRParams();
	public:
		virtual void filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		virtual ~BiquadResonantFilterInstance();
		BiquadResonantFilterInstance(BiquadResonantFilter *aParent);
	};

	class BiquadResonantFilter : public Filter
	{
	public:
		enum FILTERTYPE
		{
			NONE = 0,
			LOWPASS = 1,
			HIGHPASS = 2,
			BANDPASS = 3
		};
		enum FILTERATTRIBUTE
		{
			WET = 0,
			SAMPLERATE = 1,
			FREQUENCY = 2,
			RESONANCE = 3
		};
		int mFilterType;
		float mSampleRate;
		float mFrequency;
		float mResonance;
		virtual BiquadResonantFilterInstance *createInstance();
		BiquadResonantFilter();
		result setParams(int aType, float aSampleRate, float aFrequency, float aResonance);
		virtual ~BiquadResonantFilter();
	};
}

#endif
// file: soloud/include/soloud_fft.h
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_FFT_H
#define SOLOUD_FFT_H

// #include "soloud.h"

namespace SoLoud
{
	namespace FFT
	{
		// Perform 1024 unit FFT. Buffer must have 1024 floats, and will be overwritten
		void fft1024(float *aBuffer);

		// Perform 256 unit FFT. Buffer must have 256 floats, and will be overwritten
		void fft256(float *aBuffer);

		// Perform 256 unit IFFT. Buffer must have 256 floats, and will be overwritten
		void ifft256(float *aBuffer);

		// Generic (slower) power of two FFT. Buffer is overwritten.
		void fft(float *aBuffer, unsigned int aBufferLength);

		// Generic (slower) power of two IFFT. Buffer is overwritten.
		void ifft(float *aBuffer, unsigned int aBufferLength);
	};
};

#endif
// file: soloud/include/soloud_echofilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_ECHOFILTER_H
#define SOLOUD_ECHOFILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class EchoFilter;

	class EchoFilterInstance : public FilterInstance
	{
		float *mBuffer;
		int mBufferLength;
		EchoFilter *mParent;
		int mOffset;

	public:
		virtual void filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, time aTime);
		virtual ~EchoFilterInstance();
		EchoFilterInstance(EchoFilter *aParent);
	};

	class EchoFilter : public Filter
	{
	public:
		float mDelay;
		float mDecay;
		float mFilter;
		virtual FilterInstance *createInstance();
		EchoFilter();
		result setParams(float aDelay, float aDecay = 0.7f, float aFilter = 0.0f);
	};
}

#endif
// file: soloud/include/soloud_wav.h
/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_WAV_H
#define SOLOUD_WAV_H

// #include "soloud.h"

struct stb_vorbis;

namespace SoLoud
{
	class Wav;
	class File;
	class MemoryFile;

	class WavInstance : public AudioSourceInstance
	{
		Wav *mParent;
		unsigned int mOffset;
	public:
		WavInstance(Wav *aParent);
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual result rewind();
		virtual bool hasEnded();
	};

	class Wav : public AudioSource
	{
		result loadwav(MemoryFile *aReader);
		result loadmp3(MemoryFile *aReader);
		result testAndLoadFile(MemoryFile *aReader);
	public:
		float *mData;
		unsigned int mSampleCount;

		Wav();
		virtual ~Wav();
		result load(const char *aFilename);
		result loadMem(unsigned char *aMem, unsigned int aLength, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File *aFile);
		result loadRawWave8(unsigned char *aMem, unsigned int aLength, float aSamplerate = 44100.0f, unsigned int aChannels = 1);
		result loadRawWave16(short *aMem, unsigned int aLength, float aSamplerate = 44100.0f, unsigned int aChannels = 1);
		result loadRawWave(float *aMem, unsigned int aLength, float aSamplerate = 44100.0f, unsigned int aChannels = 1, bool aCopy = false, bool aTakeOwnership = true);

		virtual AudioSourceInstance *createInstance();
		time getLength();
	};
};

#endif
// file: soloud/include/soloud_internal.h
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_INTERNAL_H
#define SOLOUD_INTERNAL_H

// #include "soloud.h"

namespace SoLoud
{
	// SDL1 back-end initialization call
	result sdl1_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// SDL2 back-end initialization call
	result sdl2_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// SDL1 "non-dynamic" back-end initialization call
	result sdl1static_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// SDL2 "non-dynamic" back-end initialization call
	result sdl2static_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// OpenAL back-end initialization call
	result openal_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// Core Audio driver back-end initialization call
	result coreaudio_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// OpenSL ES back-end initialization call
	result opensles_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// PortAudio back-end initialization call
	result portaudio_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// WinMM back-end initialization call
	result winmm_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 4096, unsigned int aChannels = 2);

	// XAudio2 back-end initialization call
	result xaudio2_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// WASAPI back-end initialization call
	result wasapi_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 4096, unsigned int aChannels = 2);

	// OSS back-end initialization call
	result oss_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// PS Vita homebrew back-end initialization call
	result vita_homebrew_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// ALSA back-end initialization call
	result alsa_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// null driver back-end initialization call
	result null_init(SoLoud::Soloud *aSoloud, unsigned int aFlags = Soloud::CLIP_ROUNDOFF, unsigned int aSamplerate = 44100, unsigned int aBuffer = 2048, unsigned int aChannels = 2);

	// Deinterlace samples in a buffer. From 12121212 to 11112222
	void deinterlace_samples_float(const float *aSourceBuffer, float *aDestBuffer, unsigned int aSamples, unsigned int aChannels);

	// Interlace samples in a buffer. From 11112222 to 12121212
	void interlace_samples_float(const float *aSourceBuffer, float *aDestBuffer, unsigned int aSamples, unsigned int aChannels);

	// Convert to 16-bit and interlace samples in a buffer. From 11112222 to 12121212
	void interlace_samples_s16(const float *aSourceBuffer, short *aDestBuffer, unsigned int aSamples, unsigned int aChannels);
};

#define FOR_ALL_VOICES_PRE \
		handle *h_ = NULL; \
		handle th_[2] = { aVoiceHandle, 0 }; \
		lockAudioMutex(); \
		h_ = voiceGroupHandleToArray(aVoiceHandle); \
		if (h_ == NULL) h_ = th_; \
		while (*h_) \
		{ \
			int ch = getVoiceFromHandle(*h_); \
			if (ch != -1)  \
			{

#define FOR_ALL_VOICES_POST \
			} \
			h_++; \
		} \
		unlockAudioMutex();

#define FOR_ALL_VOICES_PRE_3D \
		handle *h_ = NULL; \
		handle th_[2] = { aVoiceHandle, 0 }; \
		h_ = voiceGroupHandleToArray(aVoiceHandle); \
		if (h_ == NULL) h_ = th_; \
				while (*h_) \
						{ \
			int ch = (*h_ & 0xfff) - 1; \
			if (ch != -1 && m3dData[ch].mHandle == *h_)  \
						{

#define FOR_ALL_VOICES_POST_3D \
						} \
			h_++; \
						}

#endif
// file: soloud/include/soloud_midi.h
#ifndef SOLOUD_TSF_H
#define SOLOUD_TSF_H

// #include "soloud.h"

namespace SoLoud
{
	class Midi;
	class File;

	class MidiInstance : public AudioSourceInstance
	{
		Midi *mParent;
		void *mTrack;
		double mMsec;
		int mPlaying;

	  int tick(float *stream, int SampleCount);
	public:
		MidiInstance(Midi *aParent);
		virtual ~MidiInstance();
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual void seek(float aSeconds, float *mScratch, int mScratchSize);
  	virtual unsigned int rewind();
		virtual bool hasEnded();
	};

	class SoundFont
	{
		friend class MidiInstance;
	protected:
		void *mHandle;
		char *mData;
		unsigned int mDataLen;
	public:
		SoundFont();
		virtual ~SoundFont();
		result load(const char* aFilename);
		result loadMem(unsigned char *aMem, unsigned int aLength, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File *aFile);
	};

	class Midi : public AudioSource
	{
		friend class SoundFont;
		friend class MidiInstance;
	protected:
		SoundFont *mSoundFont;
		void *mHandle;
		char *mData;
		unsigned int mDataLen;
	public:
		Midi();
		virtual ~Midi();
		result load(const char* aFilename, SoundFont &sf);
		result loadMem(unsigned char *aMem, unsigned int aLength, SoundFont &sf, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File *aFile, SoundFont &sf);
		virtual AudioSourceInstance *createInstance();
	};
};

#endif
// file: soloud/include/soloud_file.h
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_FILE_H
#define SOLOUD_FILE_H

#include <stdio.h>
// #include "soloud.h"

typedef void* Soloud_Filehack;

namespace SoLoud
{
	class File
	{
	public:
		virtual ~File() {}
		unsigned int read8();
		unsigned int read16();
		unsigned int read32();
		virtual int eof() = 0;
		virtual unsigned int read(unsigned char *aDst, unsigned int aBytes) = 0;
		virtual unsigned int length() = 0;
		virtual void seek(int aOffset) = 0;
		virtual unsigned int pos() = 0;
		virtual FILE * getFilePtr() { return 0; }
		virtual unsigned char * getMemPtr() { return 0; }
	};

	class DiskFile : public File
	{
	public:
		FILE *mFileHandle;

		virtual int eof();
		virtual unsigned int read(unsigned char *aDst, unsigned int aBytes);
		virtual unsigned int length();
		virtual void seek(int aOffset);
		virtual unsigned int pos();
		virtual ~DiskFile();
		DiskFile();
		DiskFile(FILE *fp);
		result open(const char *aFilename);
		virtual FILE * getFilePtr();
	};

	class MemoryFile : public File
	{
	public:
		unsigned char *mDataPtr;
		unsigned int mDataLength;
		unsigned int mOffset;
		bool mDataOwned;

		virtual int eof();
		virtual unsigned int read(unsigned char *aDst, unsigned int aBytes);
		virtual unsigned int length();
		virtual void seek(int aOffset);
		virtual unsigned int pos();
		virtual unsigned char * getMemPtr();
		virtual ~MemoryFile();
		MemoryFile();
		result openMem(unsigned char *aData, unsigned int aDataLength, bool aCopy=false, bool aTakeOwnership=true);
		result openToMem(const char *aFilename);
		result openFileToMem(File *aFile);
	};
};

#endif
// file: soloud/include/soloud_lofifilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_LOFIFILTER_H
#define SOLOUD_LOFIFILTER_H

// #include "soloud.h"

namespace SoLoud
{
	class LofiFilter;

	struct LofiChannelData
	{
		float mSample;
		float mSamplesToSkip;
	};

	class LofiFilterInstance : public FilterInstance
	{
		enum FILTERPARAMS
		{
			WET,
			SAMPLERATE,
			BITDEPTH
		};
		LofiChannelData mChannelData[2];

		LofiFilter *mParent;
	public:
		virtual void filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		virtual ~LofiFilterInstance();
		LofiFilterInstance(LofiFilter *aParent);
	};

	class LofiFilter : public Filter
	{
	public:
		enum FILTERPARAMS
		{
			WET,
			SAMPLERATE,
			BITDEPTH
		};
		float mSampleRate;
		float mBitdepth;
		virtual LofiFilterInstance *createInstance();
		LofiFilter();
		result setParams(float aSampleRate, float aBitdepth);
		virtual ~LofiFilter();
	};
}

#endif
// file: soloud/include/soloud_bassboostfilter.h
/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_BASSBOOSTFILTER_H
#define SOLOUD_BASSBOOSTFILTER_H

// #include "soloud.h"
// #include "soloud_fftfilter.h"

namespace SoLoud
{
	class BassboostFilter;

	class BassboostFilterInstance : public FFTFilterInstance
	{
		enum FILTERATTRIBUTE
		{
			WET = 0,
			BOOST = 1
		};
		BassboostFilter *mParent;
	public:
		virtual void fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels);
		BassboostFilterInstance(BassboostFilter *aParent);
	};

	class BassboostFilter : public FFTFilter
	{
	public:
		enum FILTERATTRIBUTE
		{
			WET = 0,
			BOOST = 1
		};
		float mBoost;
		result setParams(float aBoost);
		virtual FilterInstance *createInstance();
		BassboostFilter();
	};
}

#endif
// file: soloud/include/soloud_thread.h
/*
SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_THREAD_H
#define SOLOUD_THREAD_H

// #include "soloud.h"

namespace SoLoud
{
	namespace Thread
	{
		typedef void (*threadFunction)(void *aParam);

        struct ThreadHandleData;
        typedef ThreadHandleData* ThreadHandle;

		void * createMutex();
		void destroyMutex(void *aHandle);
		void lockMutex(void *aHandle);
		void unlockMutex(void *aHandle);

		ThreadHandle createThread(threadFunction aThreadFunction, void *aParameter);

		void sleep(int aMSec);
        void wait(ThreadHandle aThreadHandle);
        void release(ThreadHandle aThreadHandle);

#define MAX_THREADPOOL_TASKS 1024

		class PoolTask
		{
		public:
			virtual void work() = 0;
		};

		class Pool
		{
		public:
			// Initialize and run thread pool. For thread count 0, work is done at addWork call.
			void init(int aThreadCount);
			// Ctor, sets known state
			Pool();
			// Dtor. Waits for the threads to finish. Work may be unfinished.
			~Pool();
			// Add work to work list. Object is not automatically deleted when work is done.
			void addWork(PoolTask *aTask);
			// Called from worker thread to get a new task. Returns null if no work available.
			PoolTask *getWork();
		public:
			int mThreadCount; // number of threads
			ThreadHandle *mThread; // array of thread handles
			void *mWorkMutex; // mutex to protect task array/maxtask
			PoolTask *mTaskArray[MAX_THREADPOOL_TASKS]; // pointers to tasks
			int mMaxTask; // how many tasks are pending
			int mRobin; // cyclic counter, used to pick jobs for threads
			volatile int mRunning; // running flag, used to flag threads to stop
		};
	}
}

#endif
// file: soloud/include/soloud_wavstream.h
/*
SoLoud audio engine
Copyright (c) 2013-2018 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef SOLOUD_WAVSTREAM_H
#define SOLOUD_WAVSTREAM_H

#include <stdio.h>
// #include "soloud.h"

#ifndef dr_mp3_h
struct drmp3;
#endif
#ifndef dr_wav_h
struct drwav;
#endif

namespace SoLoud
{
	class WavStream;
	class File;

	class WavStreamInstance : public AudioSourceInstance
	{
		WavStream *mParent;
		unsigned int mOffset;
		File *mFile;
		union codec
		{
			drmp3 *mMp3;
			drwav *mWav;
		} mCodec;
	public:
		WavStreamInstance(WavStream *aParent);
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual result rewind();
		virtual bool hasEnded();
		virtual ~WavStreamInstance();
	};

	enum WAVSTREAM_FILETYPE
	{
		WAVSTREAM_WAV = 0,
		WAVSTREAM_MP3 = 3
	};

	class WavStream : public AudioSource
	{
		result loadwav(File *fp);
		result loadmp3(File *fp);
	public:
		int mFiletype;
		char *mFilename;
		File *mMemFile;
		File *mStreamFile;
		unsigned int mSampleCount;

		WavStream();
		virtual ~WavStream();
		result load(const char *aFilename);
		result loadMem(unsigned char *aData, unsigned int aDataLen, bool aCopy = false, bool aTakeOwnership = true);
		result loadToMem(const char *aFilename);
		result loadFile(File *aFile);
		result loadFileToMem(File *aFile);
		virtual AudioSourceInstance *createInstance();
		time getLength();

	public:
		result parse(File *aFile);
	};
};

#endif
// file: patches/includes/soloud_midi.h
#ifndef SOLOUD_TSF_H
#define SOLOUD_TSF_H

// #include "soloud.h"

namespace SoLoud
{
	class Midi;
	class File;

	class MidiInstance : public AudioSourceInstance
	{
		Midi *mParent;
		void *mTrack;
		double mMsec;
		int mPlaying;

		int tick(float *stream, int SampleCount);
	public:
		MidiInstance(Midi *aParent);
		virtual ~MidiInstance();
		virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual void seek(float aSeconds, float *mScratch, int mScratchSize);
		virtual unsigned int rewind();
		virtual bool hasEnded();
	};

	class SoundFont
	{
		friend class MidiInstance;
	protected:
		void *mHandle;
		char *mData;
		unsigned int mDataLen;
	public:
		SoundFont();
		virtual ~SoundFont();
		result load(const char* aFilename);
		result loadMem(unsigned char *aMem, unsigned int aLength, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File *aFile);
	};

	class Midi : public AudioSource
	{
		friend class SoundFont;
		friend class MidiInstance;
	protected:
		SoundFont *mSoundFont;
		void *mHandle;
		char *mData;
		unsigned int mDataLen;
	public:
		Midi();
		virtual ~Midi();
		result load(const char* aFilename, SoundFont &sf);
		result loadMem(unsigned char *aMem, unsigned int aLength, SoundFont &sf, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File *aFile, SoundFont &sf);
		virtual AudioSourceInstance *createInstance();
	};
};

#endif
