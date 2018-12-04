// ASS - Audio Stupidly Simple

#define WITH_SDL2

#if __APPLE__
#define WITH_PORTAUDIO
#define WITH_OPENAL
#endif

#if _WIN32
//#define WITH_WASAPI
//#define WITH_WINMM
//#define WITH_XAUDIO2
#endif

#if __linux__
#define WITH_OPENAL
#define WITH_ALSA
#define WITH_OSS
#endif

#define WITH_NULL

// file: dr_impl.cpp
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_FLOAT_OUTPUT
#include "dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include "dr_wav.h"

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"
#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include "tml.h"

#include "ass.h"

// file: patch_alsa.cpp
#if defined(WITH_OPENAL)

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#undef OPENAL
#elif __linux__
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#undef OPENAL
 // AL.h defines OPENAL without anything, this breaks Backend::OPENAL
#endif

#endif


// file: soloud_alsa.cpp
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

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_ALSA)

namespace SoLoud
{
    result alsa_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
};

#else

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

namespace SoLoud
{
    struct ALSAData
    {
        float *buffer;
        short *sampleBuffer;
        snd_pcm_t *alsaDeviceHandle;
        Soloud *soloud;
        int samples;
        int channels;
        bool audioProcessingDone;
        Thread::ThreadHandle threadHandle;
    };


    static void alsaThread(void *aParam)
    {

        ALSAData *data = static_cast<ALSAData*>(aParam);
        while (!data->audioProcessingDone)
        {
            data->soloud->mix(data->buffer, data->samples);
            for (int i=0;i<data->samples*data->channels;++i)
            {
                data->sampleBuffer[i] = static_cast<short>(floor(data->buffer[i]
                                                          * static_cast<float>(0x7fff)));
            }
            if (snd_pcm_writei(data->alsaDeviceHandle, data->sampleBuffer, data->samples) == -EPIPE)
                snd_pcm_prepare(data->alsaDeviceHandle);

        }

    }

    static void alsaCleanup(Soloud *aSoloud)
    {
        if (0 == aSoloud->mBackendData)
        {
            return;
        }
        ALSAData *data = static_cast<ALSAData*>(aSoloud->mBackendData);
        data->audioProcessingDone = true;
        if (data->threadHandle)
        {
            Thread::wait(data->threadHandle);
            Thread::release(data->threadHandle);
        }
        snd_pcm_drain(data->alsaDeviceHandle);
        snd_pcm_close(data->alsaDeviceHandle);
        if (0 != data->sampleBuffer)
        {
            delete[] data->sampleBuffer;
        }
        if (0 != data->buffer)
        {
            delete[] data->buffer;
        }
        delete data;
        aSoloud->mBackendData = 0;
    }

    result alsa_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
    {
        ALSAData *data = new ALSAData;
        memset(data, 0, sizeof(ALSAData));
        aSoloud->mBackendData = data;
        aSoloud->mBackendCleanupFunc = alsaCleanup;
        data->samples = aBuffer;
        data->channels = 2;
        data->soloud = aSoloud;

        int rc;
        snd_pcm_t *handle;
        rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (rc < 0)
        {
            return UNKNOWN_ERROR;
        }

        data->alsaDeviceHandle = handle;

        snd_pcm_hw_params_t *params;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(handle, params);

        snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_channels(handle, params, 2);
        snd_pcm_hw_params_set_buffer_size(handle, params, aBuffer);

        unsigned int val = aSamplerate;
        int dir = 0;
        rc = snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
        if (rc < 0)
        {
            return UNKNOWN_ERROR;
        }

        rc = snd_pcm_hw_params(handle, params);
        if (rc < 0)
        {
            return UNKNOWN_ERROR;
        }

        snd_pcm_hw_params_get_rate(params, &val, &dir);
        aSamplerate = val;
        snd_pcm_hw_params_get_channels(params, &val);
        data->channels = val;

        data->buffer = new float[data->samples*data->channels];
        data->sampleBuffer = new short[data->samples*data->channels];
        aSoloud->postinit(aSamplerate, data->samples * data->channels, aFlags, 2);
        data->threadHandle = Thread::createThread(alsaThread, data);
        if (0 == data->threadHandle)
        {
            return UNKNOWN_ERROR;
        }
        aSoloud->mBackendString = "ALSA";
        return 0;
    }
};
#endif


// file: soloud/src/audiosource/wav/soloud_wav.cpp
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// #include "soloud.h"
// #include "soloud_wav.h"
// #include "soloud_file.h"
// #include "dr_mp3.h"
// #include "dr_wav.h"

namespace SoLoud
{
	WavInstance::WavInstance(Wav *aParent)
	{
		mParent = aParent;
		mOffset = 0;
	}

	unsigned int WavInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize)
	{
		if (mParent->mData == NULL)
			return 0;

		unsigned int dataleft = mParent->mSampleCount - mOffset;
		unsigned int copylen = dataleft;
		if (copylen > aSamplesToRead)
			copylen = aSamplesToRead;

		unsigned int i;
		for (i = 0; i < mChannels; i++)
		{
			memcpy(aBuffer + i * aBufferSize, mParent->mData + mOffset + i * mParent->mSampleCount, sizeof(float) * copylen);
		}

		mOffset += copylen;
		return copylen;
	}

	result WavInstance::rewind()
	{
		mOffset = 0;
		mStreamPosition = 0.0f;
		return 0;
	}

	bool WavInstance::hasEnded()
	{
		if (!(mFlags & AudioSourceInstance::LOOPING) && mOffset >= mParent->mSampleCount)
		{
			return 1;
		}
		return 0;
	}

	Wav::Wav()
	{
		mData = NULL;
		mSampleCount = 0;
	}

	Wav::~Wav()
	{
		stop();
		delete[] mData;
	}

#define MAKEDWORD(a,b,c,d) (((d) << 24) | ((c) << 16) | ((b) << 8) | (a))

	result Wav::loadwav(MemoryFile *aReader)
	{
		drwav decoder;

		if (!drwav_init_memory(&decoder, aReader->getMemPtr(), aReader->length()))
		{
			return FILE_LOAD_FAILED;
		}

		drwav_uint64 samples = decoder.totalSampleCount / decoder.channels;

		if (!samples)
		{
			drwav_uninit(&decoder);
			return FILE_LOAD_FAILED;
		}

		mData = new float[(unsigned int)(samples * decoder.channels)];
		mBaseSamplerate = (float)decoder.sampleRate;
		mSampleCount = (unsigned int)samples;
		mChannels = decoder.channels;

		unsigned int i, j, k;
		for (i = 0; i < mSampleCount; i += 512)
		{
			float tmp[512 * MAX_CHANNELS];
			unsigned int blockSize = (mSampleCount - i) > 512 ? 512 : mSampleCount - i;
			drwav_read_pcm_frames_f32(&decoder, blockSize, tmp);
			for (j = 0; j < blockSize; j++)
			{
				for (k = 0; k < decoder.channels; k++)
				{
					mData[k * mSampleCount + i + j] = tmp[j * decoder.channels + k];
				}
			}
		}
		drwav_uninit(&decoder);

		return SO_NO_ERROR;
	}

	result Wav::loadmp3(MemoryFile *aReader)
	{
		drmp3 decoder;

		if (!drmp3_init_memory(&decoder, aReader->getMemPtr(), aReader->length(), NULL))
		{
			return FILE_LOAD_FAILED;
		}

		drmp3_uint64 samples = drmp3_get_pcm_frame_count(&decoder);

		if (!samples)
		{
			drmp3_uninit(&decoder);
			return FILE_LOAD_FAILED;
		}

		mData = new float[(unsigned int)(samples * decoder.channels)];
		mBaseSamplerate = (float)decoder.sampleRate;
		mSampleCount = (unsigned int)samples;
		mChannels = decoder.channels;
		drmp3_seek_to_pcm_frame(&decoder, 0);

		unsigned int i, j, k;
		for (i = 0; i<mSampleCount; i += 512)
		{
			float tmp[512 * MAX_CHANNELS];
			unsigned int blockSize = (mSampleCount - i) > 512 ? 512 : mSampleCount - i;
			drmp3_read_pcm_frames_f32(&decoder, blockSize, tmp);
			for (j = 0; j < blockSize; j++)
			{
				for (k = 0; k < decoder.channels; k++)
				{
					mData[k * mSampleCount + i + j] = tmp[j * decoder.channels + k];
				}
			}
		}
		drmp3_uninit(&decoder);

		return SO_NO_ERROR;
	}

	result Wav::testAndLoadFile(MemoryFile *aReader)
	{
		delete[] mData;
		mData = 0;
		mSampleCount = 0;
		mChannels = 1;
		int tag = aReader->read32();
		if (tag == MAKEDWORD('R','I','F','F'))
		{
			return loadwav(aReader);
		}
		else if (loadmp3(aReader) == SO_NO_ERROR)
		{
			return SO_NO_ERROR;
		}

		return FILE_LOAD_FAILED;
	}

	result Wav::load(const char *aFilename)
	{
		if (aFilename == 0)
			return INVALID_PARAMETER;
		stop();
		DiskFile dr;
		int res = dr.open(aFilename);
		if (res == SO_NO_ERROR)
			return loadFile(&dr);
		return FILE_LOAD_FAILED;
	}

	result Wav::loadMem(unsigned char *aMem, unsigned int aLength, bool aCopy, bool aTakeOwnership)
	{
		if (aMem == NULL || aLength == 0)
			return INVALID_PARAMETER;
		stop();

		MemoryFile dr;
		dr.openMem(aMem, aLength, aCopy, aTakeOwnership);
		return testAndLoadFile(&dr);
	}

	result Wav::loadFile(File *aFile)
	{
		if (!aFile)
			return INVALID_PARAMETER;
		stop();

		MemoryFile mr;
		result res = mr.openFileToMem(aFile);

		if (res != SO_NO_ERROR)
		{
			return res;
		}
		return testAndLoadFile(&mr);
	}

	AudioSourceInstance *Wav::createInstance()
	{
		return new WavInstance(this);
	}

	double Wav::getLength()
	{
		if (mBaseSamplerate == 0)
			return 0;
		return mSampleCount / mBaseSamplerate;
	}

	result Wav::loadRawWave8(unsigned char *aMem, unsigned int aLength, float aSamplerate, unsigned int aChannels)
	{
		if (aMem == 0 || aLength == 0 || aSamplerate <= 0 || aChannels < 1)
			return INVALID_PARAMETER;
		stop();
		delete[] mData;
		mData = new float[aLength];
		mSampleCount = aLength / aChannels;
		mChannels = aChannels;
		mBaseSamplerate = aSamplerate;
		unsigned int i;
		for (i = 0; i < aLength; i++)
			mData[i] = ((signed)aMem[i] - 128) / (float)0x80;
		return SO_NO_ERROR;
	}

	result Wav::loadRawWave16(short *aMem, unsigned int aLength, float aSamplerate, unsigned int aChannels)
	{
		if (aMem == 0 || aLength == 0 || aSamplerate <= 0 || aChannels < 1)
			return INVALID_PARAMETER;
		stop();
		delete[] mData;
		mData = new float[aLength];
		mSampleCount = aLength / aChannels;
		mChannels = aChannels;
		mBaseSamplerate = aSamplerate;
		unsigned int i;
		for (i = 0; i < aLength; i++)
			mData[i] = ((signed short)aMem[i]) / (float)0x8000;
		return SO_NO_ERROR;
	}

	result Wav::loadRawWave(float *aMem, unsigned int aLength, float aSamplerate, unsigned int aChannels, bool aCopy, bool aTakeOwndership)
	{
		if (aMem == 0 || aLength == 0 || aSamplerate <= 0 || aChannels < 1)
			return INVALID_PARAMETER;
		stop();
		delete[] mData;
		if (aCopy == true || aTakeOwndership == false)
		{
			mData = new float[aLength];
			memcpy(mData, aMem, sizeof(float) * aLength);
		}
		else
		{
			mData = aMem;
		}
		mSampleCount = aLength / aChannels;
		mChannels = aChannels;
		mBaseSamplerate = aSamplerate;
		return SO_NO_ERROR;
	}
};
// file: soloud/src/audiosource/wav/soloud_wavstream.cpp
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// #include "soloud.h"
// #include "dr_mp3.h"
// #include "dr_wav.h"
// #include "soloud_wavstream.h"
// #include "soloud_file.h"

namespace SoLoud
{
	size_t drmp3_read_func(void* pUserData, void* pBufferOut, size_t bytesToRead)
	{
		File *fp = (File*)pUserData;
		return fp->read((unsigned char*)pBufferOut, (unsigned int)bytesToRead);
	}

	size_t drwav_read_func(void* pUserData, void* pBufferOut, size_t bytesToRead)
	{
		File *fp = (File*)pUserData;
		return fp->read((unsigned char*)pBufferOut, (unsigned int)bytesToRead);
	}

	drmp3_bool32 drmp3_seek_func(void* pUserData, int offset, drmp3_seek_origin origin)
	{
		File *fp = (File*)pUserData;
		if (origin != drmp3_seek_origin_start)
			offset += fp->pos();
		fp->seek(offset);
		return 1;
	}

	drmp3_bool32 drwav_seek_func(void* pUserData, int offset, drwav_seek_origin origin)
	{
		File *fp = (File*)pUserData;
		if (origin != drwav_seek_origin_start)
			offset += fp->pos();
		fp->seek(offset);
		return 1;
	}

	WavStreamInstance::WavStreamInstance(WavStream *aParent)
	{
		mParent = aParent;
		mOffset = 0;
		mCodec.mMp3 = 0;
		mCodec.mWav = 0;
		mFile = 0;
		if (aParent->mMemFile)
		{
			MemoryFile *mf = new MemoryFile();
			mFile = mf;
			mf->openMem(aParent->mMemFile->getMemPtr(), aParent->mMemFile->length(), false, false);
		}
		else
		if (aParent->mFilename)
		{
			DiskFile *df = new DiskFile;
			mFile = df;
			df->open(aParent->mFilename);
		}
		else
		if (aParent->mStreamFile)
		{
			mFile = aParent->mStreamFile;
			mFile->seek(0); // stb_vorbis assumes file offset to be at start of ogg
		}
		else
		{
			return;
		}

		if (mFile)
		{
			if (mParent->mFiletype == WAVSTREAM_WAV)
			{
				mCodec.mWav = drwav_open(drwav_read_func, drwav_seek_func, (void*)mFile);
				if (!mCodec.mWav)
				{
					if (mFile != mParent->mStreamFile)
						delete mFile;
					mFile = 0;
				}
			}
			else
			if (mParent->mFiletype == WAVSTREAM_MP3)
			{
				mCodec.mMp3 = new drmp3;
				if (!drmp3_init(mCodec.mMp3, drmp3_read_func, drmp3_seek_func, (void*)mFile, NULL))
				{
					delete mCodec.mMp3;
					mCodec.mMp3 = 0;
					if (mFile != mParent->mStreamFile)
						delete mFile;
					mFile = 0;
				}
			}
			else
			{
				if (mFile != mParent->mStreamFile)
					delete mFile;
				mFile = NULL;
				return;
			}
		}
	}

	WavStreamInstance::~WavStreamInstance()
	{
		switch (mParent->mFiletype)
		{
		case WAVSTREAM_MP3:
			if (mCodec.mMp3)
			{
				drmp3_uninit(mCodec.mMp3);
				delete mCodec.mMp3;
			}
			break;
		case WAVSTREAM_WAV:
			if (mCodec.mWav)
			{
				drwav_close(mCodec.mWav);
			}
		}
		if (mFile != mParent->mStreamFile)
		{
			delete mFile;
		}
	}

	unsigned int WavStreamInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize)
	{
		unsigned int offset = 0;
		if (mFile == NULL)
			return 0;
		switch (mParent->mFiletype)
		{
		case WAVSTREAM_MP3:
			{
				unsigned int i, j, k;

				for (i = 0; i < aSamplesToRead; i += 512)
				{
					float tmp[512 * MAX_CHANNELS];
					unsigned int blockSize = (aSamplesToRead - i) > 512 ? 512 : aSamplesToRead - i;
					offset += (unsigned int)drmp3_read_pcm_frames_f32(mCodec.mMp3, blockSize, tmp);

					for (j = 0; j < blockSize; j++)
					{
						for (k = 0; k < mChannels; k++)
						{
							aBuffer[k * aSamplesToRead + i + j] = tmp[j * mCodec.mMp3->channels + k];
						}
					}
				}
				mOffset += offset;
				return offset;
			}
		break;
		case WAVSTREAM_WAV:
			{
				unsigned int i, j, k;

				for (i = 0; i < aSamplesToRead; i += 512)
				{
					float tmp[512 * MAX_CHANNELS];
					unsigned int blockSize = (aSamplesToRead - i) > 512 ? 512 : aSamplesToRead - i;
					offset += (unsigned int)drwav_read_pcm_frames_f32(mCodec.mWav, blockSize, tmp);

					for (j = 0; j < blockSize; j++)
					{
						for (k = 0; k < mChannels; k++)
						{
							aBuffer[k * aSamplesToRead + i + j] = tmp[j * mCodec.mWav->channels + k];
						}
					}
				}
				mOffset += offset;
				return offset;
			}
			break;
		}
		return aSamplesToRead;
	}

	result WavStreamInstance::rewind()
	{
		switch (mParent->mFiletype)
		{
		case WAVSTREAM_MP3:
			if (mCodec.mMp3)
			{
				drmp3_seek_to_pcm_frame(mCodec.mMp3, 0);
			}
			break;
		case WAVSTREAM_WAV:
			if (mCodec.mWav)
			{
				drwav_seek_to_pcm_frame(mCodec.mWav, 0);
			}
			break;
		}
		mOffset = 0;
		mStreamPosition = 0.0f;
		return 0;
	}

	bool WavStreamInstance::hasEnded()
	{
		if (mOffset >= mParent->mSampleCount)
		{
			return 1;
		}
		return 0;
	}

	WavStream::WavStream()
	{
		mFilename = 0;
		mSampleCount = 0;
		mFiletype = WAVSTREAM_WAV;
		mMemFile = 0;
		mStreamFile = 0;
	}

	WavStream::~WavStream()
	{
		stop();
		delete[] mFilename;
		delete mMemFile;
	}

#define MAKEDWORD(a,b,c,d) (((d) << 24) | ((c) << 16) | ((b) << 8) | (a))

	result WavStream::loadwav(File * fp)
	{
		fp->seek(0);
		drwav* decoder = drwav_open(drwav_read_func, drwav_seek_func, (void*)fp);

		if (decoder == NULL)
			return FILE_LOAD_FAILED;

		mChannels = decoder->channels;
		if (mChannels > MAX_CHANNELS)
		{
			mChannels = MAX_CHANNELS;
		}

		mBaseSamplerate = (float)decoder->sampleRate;
		mSampleCount = (unsigned int)decoder->totalSampleCount;
		mFiletype = WAVSTREAM_WAV;
		drwav_close(decoder);

		return SO_NO_ERROR;
	}

	result WavStream::loadmp3(File * fp)
	{
		fp->seek(0);
		drmp3 decoder;
		if (!drmp3_init(&decoder, drmp3_read_func, drmp3_seek_func, (void*)fp, NULL))
			return FILE_LOAD_FAILED;


		mChannels = decoder.channels;
		if (mChannels > MAX_CHANNELS)
		{
			mChannels = MAX_CHANNELS;
		}

		drmp3_uint64 samples = drmp3_get_pcm_frame_count(&decoder);

		mBaseSamplerate = (float)decoder.sampleRate;
		mSampleCount = (unsigned int)samples;
		mFiletype = WAVSTREAM_MP3;
		drmp3_uninit(&decoder);

		return SO_NO_ERROR;
	}

	result WavStream::load(const char *aFilename)
	{
		delete[] mFilename;
		delete mMemFile;
		mMemFile = 0;
		mFilename = 0;
		mSampleCount = 0;
		DiskFile fp;
		int res = fp.open(aFilename);
		if (res != SO_NO_ERROR)
			return res;

		int len = (int)strlen(aFilename);
		mFilename = new char[len+1];
		memcpy(mFilename, aFilename, len);
		mFilename[len] = 0;

		res = parse(&fp);

		if (res != SO_NO_ERROR)
		{
			delete[] mFilename;
			mFilename = 0;
			return res;
		}

		return 0;
	}

	result WavStream::loadMem(unsigned char *aData, unsigned int aDataLen, bool aCopy, bool aTakeOwnership)
	{
		delete[] mFilename;
		delete mMemFile;
		mStreamFile = 0;
		mMemFile = 0;
		mFilename = 0;
		mSampleCount = 0;

		if (aData == NULL || aDataLen == 0)
			return INVALID_PARAMETER;

		MemoryFile *mf = new MemoryFile();
		int res = mf->openMem(aData, aDataLen, aCopy, aTakeOwnership);
		if (res != SO_NO_ERROR)
		{
			delete mf;
			return res;
		}

		res = parse(mf);

		if (res != SO_NO_ERROR)
		{
			delete mf;
			return res;
		}

		mMemFile = mf;

		return 0;
	}

	result WavStream::loadToMem(const char *aFilename)
	{
		DiskFile df;
		int res = df.open(aFilename);
		if (res == SO_NO_ERROR)
		{
			res = loadFileToMem(&df);
		}
		return res;
	}

	result WavStream::loadFile(File *aFile)
	{
		delete[] mFilename;
		delete mMemFile;
		mStreamFile = 0;
		mMemFile = 0;
		mFilename = 0;
		mSampleCount = 0;

		int res = parse(aFile);

		if (res != SO_NO_ERROR)
		{
			return res;
		}

		mStreamFile = aFile;

		return 0;
	}

	result WavStream::loadFileToMem(File *aFile)
	{
		delete[] mFilename;
		delete mMemFile;
		mStreamFile = 0;
		mMemFile = 0;
		mFilename = 0;
		mSampleCount = 0;

		MemoryFile *mf = new MemoryFile();
		int res = mf->openFileToMem(aFile);
		if (res != SO_NO_ERROR)
		{
			delete mf;
			return res;
		}

		res = parse(mf);

		if (res != SO_NO_ERROR)
		{
			delete mf;
			return res;
		}

		mMemFile = mf;

		return res;
	}


	result WavStream::parse(File *aFile)
	{
		int tag = aFile->read32();
		int res = SO_NO_ERROR;
		if (tag == MAKEDWORD('R', 'I', 'F', 'F'))
		{
			res = loadwav(aFile);
		}
		else
		if (loadmp3(aFile) == SO_NO_ERROR)
		{
			res = SO_NO_ERROR;
		}
		else
		{
			res = FILE_LOAD_FAILED;
		}
		return res;
	}

	AudioSourceInstance *WavStream::createInstance()
	{
		return new WavStreamInstance(this);
	}

	double WavStream::getLength()
	{
		if (mBaseSamplerate == 0)
			return 0;
		return mSampleCount / mBaseSamplerate;
	}
};
// file: soloud/src/backend/coreaudio/soloud_coreaudio.cpp
/*
SoLoud audio engine
Copyright (c) 2015 Jari Komppa

Core Audio backend for Mac OS X
Copyright (c) 2015 Petri Häkkinen

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

#include <stdlib.h>
#include <math.h>

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_COREAUDIO)

namespace SoLoud
{
	result coreaudio_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
}
#else

#include <AudioToolbox/AudioToolbox.h>

#define NUM_BUFFERS 2

static AudioQueueRef audioQueue = 0;

namespace SoLoud
{
	void soloud_coreaudio_deinit(SoLoud::Soloud *aSoloud)
	{
		AudioQueueStop(audioQueue, true);
		AudioQueueDispose(audioQueue, false);
	}

	static void coreaudio_mutex_lock(void *mutex)
	{
		Thread::lockMutex(mutex);
	}

	static void coreaudio_mutex_unlock(void *mutex)
	{
		Thread::unlockMutex(mutex);
	}

	static void coreaudio_fill_buffer(void *context, AudioQueueRef queue, AudioQueueBufferRef buffer)
	{
		SoLoud::Soloud *aSoloud = (SoLoud::Soloud*)context;
		aSoloud->mixSigned16((short*)buffer->mAudioData, buffer->mAudioDataByteSize / 4);
		AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
	}

	result coreaudio_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
	{
		aSoloud->postinit(aSamplerate, aBuffer, aFlags, 2);
		aSoloud->mBackendCleanupFunc = soloud_coreaudio_deinit;

		AudioStreamBasicDescription audioFormat;
		audioFormat.mSampleRate = aSamplerate;
		audioFormat.mFormatID = kAudioFormatLinearPCM;
		audioFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		audioFormat.mBytesPerPacket = 4;
		audioFormat.mFramesPerPacket = 1;
		audioFormat.mBytesPerFrame = 4;
		audioFormat.mChannelsPerFrame = 2;
		audioFormat.mBitsPerChannel = 16;
		audioFormat.mReserved = 0;

		// create the audio queue
		OSStatus result = AudioQueueNewOutput(&audioFormat, coreaudio_fill_buffer, aSoloud, NULL, NULL, 0, &audioQueue);
		if(result)
		{
			//printf("AudioQueueNewOutput failed (%d)\n", result);
			return UNKNOWN_ERROR;
		}

		// allocate and prime audio buffers
		for(int i = 0; i < NUM_BUFFERS; ++i)
		{
			AudioQueueBufferRef buffer;
			result = AudioQueueAllocateBuffer(audioQueue, aBuffer * 4, &buffer);
			if(result)
			{
				//printf("AudioQueueAllocateBuffer failed (%d)\n", result);
				return UNKNOWN_ERROR;
			}
			buffer->mAudioDataByteSize = aBuffer * 4;
			memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
			AudioQueueEnqueueBuffer(audioQueue, buffer, 0, NULL);
		}

		// start playback
		result = AudioQueueStart(audioQueue, NULL);
		if(result)
		{
			//printf("AudioQueueStart failed (%d)\n", result);
			return UNKNOWN_ERROR;
		}

        aSoloud->mBackendString = "CoreAudio";
		return 0;
	}
};
#endif
// file: soloud/src/backend/null/soloud_null.cpp
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

// #include "soloud.h"

#if !defined(WITH_NULL)

namespace SoLoud
{
    result null_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
};

#else

namespace SoLoud
{
    static void nullCleanup(Soloud *aSoloud)
    {
    }

    result null_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
    {
		if (aChannels == 0 || aChannels == 3 || aChannels == 5 || aChannels == 7 || aChannels > MAX_CHANNELS || aBuffer < SAMPLE_GRANULARITY)
			return INVALID_PARAMETER;
        aSoloud->mBackendData = 0;
        aSoloud->mBackendCleanupFunc = nullCleanup;

        aSoloud->postinit(aSamplerate, aBuffer, aFlags, aChannels);
        aSoloud->mBackendString = "null driver";
        return SO_NO_ERROR;
    }
};
#endif
// file: soloud/src/backend/openal/soloud_openal_dll.c
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
#if defined(WITH_OPENAL)

#include <stdlib.h>
#include <math.h>

#ifdef __APPLE__
// #include "OpenAL/al.h"
// #include "OpenAL/alc.h"
#else
// #include "AL/al.h"
// #include "AL/alc.h"
// #include "AL/alext.h"
#endif

#if defined(_MSC_VER)
#define WINDOWS_VERSION
#endif

#ifdef SOLOUD_STATIC_OPENAL

extern "C"
{

// statically linked OpenAL
int dll_al_found() { return 1; }
ALCdevice* dll_alc_OpenDevice(const ALCchar *devicename) { return alcOpenDevice(devicename); }
void dll_alc_CloseDevice(ALCdevice *device) { alcCloseDevice(device); }
ALCcontext* dll_alc_CreateContext(ALCdevice *device, const ALCint* attrlist) { return alcCreateContext(device, attrlist); }
void dll_alc_DestroyContext(ALCcontext *context) { return alcDestroyContext(context); }
ALCboolean dll_alc_MakeContextCurrent(ALCcontext *context) { return alcMakeContextCurrent(context); }
void dll_al_GetSourcei(ALuint source, ALenum param, ALint *value) { alGetSourcei(source, param, value); }
void dll_al_SourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers) { alSourceQueueBuffers(source, nb, buffers); }
void dll_al_SourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers) { alSourceUnqueueBuffers(source, nb, buffers); }
void dll_al_BufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq) { alBufferData(buffer, format, data, size, freq); }
void dll_al_SourcePlay(ALuint source) { alSourcePlay(source); }
void dll_al_SourceStop(ALuint source) { alSourceStop(source); }
void dll_al_GenBuffers(ALsizei n, ALuint *buffers) { alGenBuffers(n, buffers); }
void dll_al_DeleteBuffers(ALsizei n, ALuint *buffers) { alDeleteBuffers(n, buffers); }
void dll_al_GenSources(ALsizei n, ALuint *sources) { alGenSources(n, sources); }
void dll_al_DeleteSources(ALsizei n, ALuint *sources) { alDeleteSources(n, sources); }

}

#else

typedef ALCdevice* (*alc_OpenDevice)(const ALCchar *devicename);
typedef void (*alc_CloseDevice)(ALCdevice *device);
typedef ALCcontext* (*alc_CreateContext)(ALCdevice *device, const ALCint* attrlist);
typedef void (*alc_DestroyContext)(ALCcontext *context);
typedef ALCboolean (*alc_MakeContextCurrent)(ALCcontext *context);
typedef void (*al_GetSourcei)(ALuint source, ALenum param, ALint *value);
typedef void (*al_SourceQueueBuffers)(ALuint source, ALsizei nb, const ALuint *buffers);
typedef void (*al_SourceUnqueueBuffers)(ALuint source, ALsizei nb, ALuint *buffers);
typedef void (*al_BufferData)(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq);
typedef void (*al_SourcePlay)(ALuint source);
typedef void (*al_SourceStop)(ALuint source);
typedef void (*al_GenBuffers)(ALsizei n, ALuint *buffers);
typedef void (*al_DeleteBuffers)(ALsizei n, ALuint *buffers);
typedef void (*al_GenSources)(ALsizei n, ALuint *sources);
typedef void (*al_DeleteSources)(ALsizei n, ALuint *sources);

static alc_OpenDevice dAlcOpenDevice;
static alc_CloseDevice dAlcCloseDevice;
static alc_CreateContext dAlcCreateContext;
static alc_DestroyContext dAlcDestroyContext;
static alc_MakeContextCurrent dAlcMakeContextCurrent;
static al_GetSourcei dAlGetSourcei;
static al_SourceQueueBuffers dAlSourceQueueBuffers;
static al_SourceUnqueueBuffers dAlSourceUnqueueBuffers;
static al_BufferData dAlBufferData;
static al_SourcePlay dAlSourcePlay;
static al_SourceStop dAlSourceStop;
static al_GenBuffers dAlGenBuffers;
static al_DeleteBuffers dAlDeleteBuffers;
static al_GenSources dAlGenSources;
static al_DeleteSources dAlDeleteSources;

#ifdef WINDOWS_VERSION
#include <windows.h>

static HMODULE oal_openDll()
{
	HMODULE x = LoadLibraryA("soft_oal.dll");
	if (x == 0) x = LoadLibraryA("OpenAL32.dll");
	return x;
}

static void* oal_getDllProc(HMODULE aDllHandle, const char *aProcName)
{
    return GetProcAddress(aDllHandle, aProcName);
}

#else
#include <dlfcn.h> // dll functions

typedef void* HMODULE;

static HMODULE oal_openDll()
{
    return dlopen("libopenal.so", RTLD_LAZY);
}

static void* oal_getDllProc(HMODULE aLibrary, const char *aProcName)
{
    return dlsym(aLibrary, aProcName);
}

#endif

extern "C"
{

static int oal_load_dll()
{
#ifdef WINDOWS_VERSION
	HMODULE dll = NULL;
#else
	void * dll = NULL;
#endif

	if (dAlcOpenDevice != NULL)
	{
		return 1;
	}

    dll = oal_openDll();

    if (dll)
    {
        dAlcOpenDevice = (alc_OpenDevice)oal_getDllProc(dll, "alcOpenDevice");
        dAlcCloseDevice = (alc_CloseDevice)oal_getDllProc(dll, "alcCloseDevice");
        dAlcCreateContext = (alc_CreateContext)oal_getDllProc(dll, "alcCreateContext");
        dAlcDestroyContext = (alc_DestroyContext)oal_getDllProc(dll, "alcDestroyContext");
        dAlcMakeContextCurrent = (alc_MakeContextCurrent)oal_getDllProc(dll, "alcMakeContextCurrent");
        dAlGetSourcei = (al_GetSourcei)oal_getDllProc(dll, "alGetSourcei");
        dAlSourceQueueBuffers = (al_SourceQueueBuffers)oal_getDllProc(dll, "alSourceQueueBuffers");
        dAlSourceUnqueueBuffers = (al_SourceUnqueueBuffers)oal_getDllProc(dll, "alSourceUnqueueBuffers");
        dAlBufferData = (al_BufferData)oal_getDllProc(dll, "alBufferData");
        dAlSourcePlay = (al_SourcePlay)oal_getDllProc(dll, "alSourcePlay");
        dAlSourceStop = (al_SourceStop)oal_getDllProc(dll, "alSourceStop");
        dAlGenBuffers = (al_GenBuffers)oal_getDllProc(dll, "alGenBuffers");
        dAlDeleteBuffers = (al_GenBuffers)oal_getDllProc(dll, "alDeleteBuffers");
        dAlGenSources = (al_GenSources)oal_getDllProc(dll, "alGenSources");
        dAlDeleteSources = (al_GenSources)oal_getDllProc(dll, "alDeleteSources");

        if (dAlcOpenDevice &&
        	dAlcCloseDevice &&
			dAlcCreateContext &&
			dAlcDestroyContext &&
			dAlcMakeContextCurrent &&
            dAlGetSourcei &&
			dAlSourceQueueBuffers &&
            dAlSourceUnqueueBuffers &&
			dAlBufferData &&
			dAlSourcePlay &&
			dAlSourceStop &&
            dAlGenBuffers &&
            dAlDeleteBuffers &&
			dAlGenSources &&
			dAlDeleteSources)
        {
            return 1;
        }
	}
	dAlcOpenDevice = 0;
	return 0;
}

int dll_al_found()
{
	return oal_load_dll();
}

ALCdevice* dll_alc_OpenDevice(const ALCchar *devicename)
{
	if (oal_load_dll())
		return dAlcOpenDevice(devicename);
	return NULL;
}

void dll_alc_CloseDevice(ALCdevice *device)
{
	if (oal_load_dll())
		dAlcCloseDevice(device);
}

ALCcontext* dll_alc_CreateContext(ALCdevice *device, const ALCint* attrlist)
{
	if (oal_load_dll())
		return dAlcCreateContext(device, attrlist);
	return NULL;
}

void dll_alc_DestroyContext(ALCcontext *context)
{
	if (oal_load_dll())
		dAlcDestroyContext(context);
}

ALCboolean dll_alc_MakeContextCurrent(ALCcontext *context)
{
	if (oal_load_dll())
		return dAlcMakeContextCurrent(context);
	return 0;
}

void dll_al_GetSourcei(ALuint source, ALenum param, ALint *value)
{
	if (oal_load_dll())
		dAlGetSourcei(source, param, value);
}

void dll_al_SourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers)
{
	if (oal_load_dll())
		dAlSourceQueueBuffers(source, nb, buffers);
}

void dll_al_SourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers)
{
	if (oal_load_dll())
		dAlSourceUnqueueBuffers(source, nb, buffers);
}

void dll_al_BufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq)
{
	if (oal_load_dll())
		dAlBufferData(buffer, format, data, size, freq);
}

void dll_al_SourcePlay(ALuint source)
{
	if (oal_load_dll())
		dAlSourcePlay(source);
}

void dll_al_SourceStop(ALuint source)
{
	if (oal_load_dll())
		dAlSourceStop(source);
}

void dll_al_GenBuffers(ALsizei n, ALuint *buffers)
{
	if (oal_load_dll())
		dAlGenBuffers(n, buffers);
}

void dll_al_DeleteBuffers(ALsizei n, ALuint *buffers)
{
	if (oal_load_dll())
		dAlDeleteBuffers(n, buffers);
}

void dll_al_GenSources(ALsizei n, ALuint *sources)
{
	if (oal_load_dll())
		dAlGenSources(n, sources);
}

void dll_al_DeleteSources(ALsizei n, ALuint *sources)
{
	if (oal_load_dll())
		dAlDeleteSources(n, sources);
}

}

#endif
#endif
// file: soloud/src/backend/openal/soloud_openal.cpp
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
#include <stdlib.h>
#include <math.h>

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_OPENAL)

namespace SoLoud
{
	result openal_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
}
#else

#ifdef __APPLE__
// #include "OpenAL/al.h"
// #include "OpenAL/alc.h"
#else
// #include "AL/al.h"
// #include "AL/alc.h"
// #include "AL/alext.h"
#endif

#define NUM_BUFFERS 2

static ALCdevice* device = NULL;
static ALCcontext* context = NULL;
static ALenum format = 0;
static ALuint buffers[NUM_BUFFERS];
static ALuint source = 0;
static int frequency = 0;
static volatile int threadrun = 0;
static int buffersize = 0;
static short* bufferdata = 0;

extern "C"
{
	int dll_al_found();
	ALCdevice* dll_alc_OpenDevice(const ALCchar *devicename);
	void dll_alc_CloseDevice(ALCdevice *device);
	ALCcontext* dll_alc_CreateContext(ALCdevice *device, const ALCint* attrlist);
	void dll_alc_DestroyContext(ALCcontext *context);
	ALCboolean dll_alc_MakeContextCurrent(ALCcontext *context);
	void dll_al_GetSourcei(ALuint source, ALenum param, ALint *value);
	void dll_al_SourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers);
	void dll_al_SourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers);
	void dll_al_BufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq);
	void dll_al_SourcePlay(ALuint source);
	void dll_al_SourceStop(ALuint source);
	void dll_al_GenBuffers(ALsizei n, ALuint *buffers);
	void dll_al_DeleteBuffers(ALsizei n, ALuint *buffers);
	void dll_al_GenSources(ALsizei n, ALuint *sources);
	void dll_al_DeleteSources(ALsizei n, ALuint *sources);
}

namespace SoLoud
{
	void soloud_openal_deinit(SoLoud::Soloud *aSoloud)
	{
		threadrun++;
		while (threadrun == 1)
		{
			Thread::sleep(10);
		}

		dll_al_SourceStop(source);
		dll_al_DeleteSources(1, &source);
		dll_al_DeleteBuffers(NUM_BUFFERS, buffers);

		dll_alc_MakeContextCurrent(NULL);
		dll_alc_DestroyContext(context);
		dll_alc_CloseDevice(device);

		free(bufferdata);

		device = NULL;
		context = NULL;
		format = 0;
		source = 0;
		frequency = 0;
		threadrun = 0;
		buffersize = 0;
		bufferdata = 0;
	}

	static void openal_mutex_lock(void * mutex)
	{
		Thread::lockMutex(mutex);
	}

	static void openal_mutex_unlock(void * mutex)
	{
		Thread::unlockMutex(mutex);
	}

	static void openal_iterate(SoLoud::Soloud *aSoloud)
	{
		ALuint buffer = 0;
		ALint buffersProcessed = 0;
		ALint state;
		dll_al_GetSourcei(source, AL_BUFFERS_PROCESSED, &buffersProcessed);

		while (buffersProcessed--)
		{
			aSoloud->mixSigned16(bufferdata,buffersize);

			dll_al_SourceUnqueueBuffers(source, 1, &buffer);

			dll_al_BufferData(buffer, format, bufferdata, buffersize*4, frequency);

			dll_al_SourceQueueBuffers(source, 1, &buffer);
		}

		dll_al_GetSourcei(source, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
			dll_al_SourcePlay(source);
	}

	static void openal_thread(void *aParam)
	{
		Soloud *soloud = (Soloud *)aParam;
		while (threadrun == 0)
		{
			openal_iterate(soloud);
			Thread::sleep(1);
		}
		threadrun++;
	}

	result openal_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
	{
		if (!dll_al_found())
			return DLL_NOT_FOUND;

		aSoloud->postinit(aSamplerate,aBuffer,aFlags,2);
		aSoloud->mBackendCleanupFunc = soloud_openal_deinit;

		device = dll_alc_OpenDevice(NULL);
		context = dll_alc_CreateContext(device, NULL);
		dll_alc_MakeContextCurrent(context);
		format = AL_FORMAT_STEREO16;
		dll_al_GenBuffers(NUM_BUFFERS, buffers);
		dll_al_GenSources(1, &source);
		buffersize = aBuffer;
		bufferdata = (short*)malloc(buffersize*2*2);

		if (!bufferdata)
			return OUT_OF_MEMORY;

		frequency = aSamplerate;

		int i;
		for (i = 0; i < buffersize*2; i++)
			bufferdata[i] = 0;
		for (i = 0; i < NUM_BUFFERS; i++)
		{
			dll_al_BufferData(buffers[i], format, bufferdata, buffersize, frequency);
			dll_al_SourceQueueBuffers(source, 1, &buffers[i]);
		}

		dll_al_SourcePlay(source);

		Thread::createThread(openal_thread, (void*)aSoloud);

        aSoloud->mBackendString = "OpenAL";
		return 0;
	}
};
#endif
// file: soloud/src/backend/oss/soloud_oss.cpp
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

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_OSS)

namespace SoLoud
{
    result oss_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
};

#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <string.h>

namespace SoLoud
{
    static const int OSS_DEVICE_COUNT = 4;
    static const char *OSS_DEVICES[OSS_DEVICE_COUNT] =
    {
        "/dev/dsp",
        "/dev/dsp0.0",
        "/dev/dsp1.0",
        "/dev/dsp2.0"
    };

    struct OSSData
    {
        float *buffer;
        short *sampleBuffer;
        int ossDeviceHandle;
        Soloud *soloud;
        int samples;
        int channels;
        bool audioProcessingDone;
        Thread::ThreadHandle threadHandle;
    };

    static void ossThread(void *aParam)
    {
        OSSData *data = static_cast<OSSData*>(aParam);
        while (!data->audioProcessingDone)
        {
            data->soloud->mix(data->buffer, data->samples);
            for (int i=0;i<data->samples*data->channels;++i)
            {
                data->sampleBuffer[i] = static_cast<short>(floor(data->buffer[i]
                                                                 * static_cast<float>(0x7fff)));
            }
            write(data->ossDeviceHandle, data->sampleBuffer,
                  sizeof(short)*data->samples*data->channels);
        }
    }

    static void ossCleanup(Soloud *aSoloud)
    {
        if (0 == aSoloud->mBackendData)
        {
            return;
        }
        OSSData *data = static_cast<OSSData*>(aSoloud->mBackendData);
        data->audioProcessingDone = true;
        if (data->threadHandle)
        {
            Thread::wait(data->threadHandle);
            Thread::release(data->threadHandle);
        }
        ioctl(data->ossDeviceHandle, SNDCTL_DSP_RESET, 0);
        if (0 != data->sampleBuffer)
        {
            delete[] data->sampleBuffer;
        }
        if (0 != data->buffer)
        {
            delete[] data->buffer;
        }
        close(data->ossDeviceHandle);
        delete data;
        aSoloud->mBackendData = 0;
    }

    result oss_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
    {
        OSSData *data = new OSSData;
        memset(data, 0, sizeof(OSSData));
        aSoloud->mBackendData = data;
        aSoloud->mBackendCleanupFunc = ossCleanup;
        data->samples = aBuffer;
        data->soloud = aSoloud;
        bool deviceIsOpen = false;
        for (int i=0;i<OSS_DEVICE_COUNT;++i)
        {
            data->ossDeviceHandle = open(OSS_DEVICES[i], O_WRONLY, 0);
            if (-1 != data->ossDeviceHandle)
            {
                deviceIsOpen = true;
                break;
            }
        }
        if (!deviceIsOpen)
        {
            return UNKNOWN_ERROR;
        }
        int flags = fcntl(data->ossDeviceHandle, F_GETFL);
        flags &= ~O_NONBLOCK;
        if (-1 == fcntl(data->ossDeviceHandle, F_SETFL, flags))
        {
            return UNKNOWN_ERROR;
        }
        int format = AFMT_S16_NE;
        if (-1 == ioctl(data->ossDeviceHandle, SNDCTL_DSP_SETFMT, &format))
        {
            return UNKNOWN_ERROR;
        }
        if (format != AFMT_S16_NE)
        {
            return UNKNOWN_ERROR;
        }
        int channels = 2;
        data->channels = channels;
        if (-1 == ioctl(data->ossDeviceHandle, SNDCTL_DSP_CHANNELS, &data->channels))
        {
            return UNKNOWN_ERROR;
        }
        if (channels != data->channels)
        {
            return UNKNOWN_ERROR;
        }
        int speed = aSamplerate;
        if (-1 == ioctl(data->ossDeviceHandle, SNDCTL_DSP_SPEED, &speed))
        {
            return UNKNOWN_ERROR;
        }
        if (speed != aSamplerate)
        {
            return UNKNOWN_ERROR;
        }
        data->buffer = new float[data->samples*data->channels];
        data->sampleBuffer = new short[data->samples*data->channels];
        aSoloud->postinit(aSamplerate, data->samples * data->channels, aFlags, 2);
        data->threadHandle = Thread::createThread(ossThread, data);
        if (0 == data->threadHandle)
        {
            return UNKNOWN_ERROR;
        }
        aSoloud->mBackendString = "OSS";
        return 0;
    }
};
#endif
// file: soloud/src/backend/sdl/soloud_sdl2.cpp
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
#include <stdlib.h>

// #include "soloud.h"

#if !defined(WITH_SDL2)

namespace SoLoud
{
	result sdl2_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
}

#else

#if defined(_MSC_VER)
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <math.h>


extern "C"
{
	int dll_SDL2_found();

	Uint32 dll_SDL2_WasInit(Uint32 flags);
	int dll_SDL2_InitSubSystem(Uint32 flags);
	SDL_AudioDeviceID dll_SDL2_OpenAudioDevice(const char*          device,
											   int                  iscapture,
											   const SDL_AudioSpec* desired,
											   SDL_AudioSpec*       obtained,
											   int                  allowed_changes);
	void dll_SDL2_CloseAudioDevice(SDL_AudioDeviceID dev);
	void dll_SDL2_PauseAudioDevice(SDL_AudioDeviceID dev,
								   int               pause_on);
};


namespace SoLoud
{
	static SDL_AudioSpec gActiveAudioSpec;
	static SDL_AudioDeviceID gAudioDeviceID;

	void soloud_sdl2_audiomixer(void *userdata, Uint8 *stream, int len)
	{
		short *buf = (short*)stream;
		SoLoud::Soloud *soloud = (SoLoud::Soloud *)userdata;
		if (gActiveAudioSpec.format == AUDIO_F32)
		{
			int samples = len / (gActiveAudioSpec.channels * sizeof(float));
			soloud->mix((float *)buf, samples);
		}
		else // assume s16 if not float
		{
			int samples = len / (gActiveAudioSpec.channels * sizeof(short));
			soloud->mixSigned16(buf, samples);
		}
	}

	static void soloud_sdl2_deinit(SoLoud::Soloud *aSoloud)
	{
		dll_SDL2_CloseAudioDevice(gAudioDeviceID);
	}

	result sdl2_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
	{
		if (!dll_SDL2_found())
			return DLL_NOT_FOUND;

		if (!dll_SDL2_WasInit(SDL_INIT_AUDIO))
		{
			if (dll_SDL2_InitSubSystem(SDL_INIT_AUDIO) < 0)
			{
				return UNKNOWN_ERROR;
			}
		}

		SDL_AudioSpec as;
		as.freq = aSamplerate;
		as.format = AUDIO_F32;
		as.channels = aChannels;
		as.samples = aBuffer;
		as.callback = soloud_sdl2_audiomixer;
		as.userdata = (void*)aSoloud;

		gAudioDeviceID = dll_SDL2_OpenAudioDevice(NULL, 0, &as, &gActiveAudioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~(SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE));
		if (gAudioDeviceID == 0)
		{
			as.format = AUDIO_S16;
			gAudioDeviceID = dll_SDL2_OpenAudioDevice(NULL, 0, &as, &gActiveAudioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~(SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE));
			if (gAudioDeviceID == 0)
			{
				return UNKNOWN_ERROR;
			}
		}

		aSoloud->postinit(gActiveAudioSpec.freq, gActiveAudioSpec.samples, aFlags, gActiveAudioSpec.channels);

		aSoloud->mBackendCleanupFunc = soloud_sdl2_deinit;

		dll_SDL2_PauseAudioDevice(gAudioDeviceID, 0);
        aSoloud->mBackendString = "SDL2 (dynamic)";
		return 0;
	}

};
#endif
// file: soloud/src/backend/sdl/soloud_sdl2_dll.c
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
#if defined(WITH_SDL2)

#include <stdlib.h>
#if defined(_MSC_VER)
#define WINDOWS_VERSION
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <math.h>


typedef Uint32            (*SDL2_WasInit_t)(Uint32 flags);
typedef int               (*SDL2_InitSubSystem_t)(Uint32 flags);
typedef SDL_AudioDeviceID (*SDL2_OpenAudioDevice_t)(const char*          device,
												    int                  iscapture,
												    const SDL_AudioSpec* desired,
												    SDL_AudioSpec*       obtained,
												    int                  allowed_changes);
typedef void              (*SDL2_CloseAudioDevice_t)(SDL_AudioDeviceID dev);
typedef void              (*SDL2_PauseAudioDevice_t)(SDL_AudioDeviceID dev,
												     int               pause_on);

static SDL2_WasInit_t SDL2_WasInit = NULL;
static SDL2_InitSubSystem_t SDL2_InitSubSystem = NULL;
static SDL2_OpenAudioDevice_t SDL2_OpenAudioDevice = NULL;
static SDL2_CloseAudioDevice_t SDL2_CloseAudioDevice = NULL;
static SDL2_PauseAudioDevice_t SDL2_PauseAudioDevice = NULL;

#ifdef WINDOWS_VERSION
#include <windows.h>

static HMODULE sdl2_openDll()
{
	HMODULE res = LoadLibraryA("SDL2.dll");
    return res;
}

static void* sdl2_getDllProc(HMODULE aDllHandle, const char *aProcName)
{
    return GetProcAddress(aDllHandle, aProcName);
}

#else
#include <dlfcn.h> // dll functions

static void * sdl2_openDll()
{
	void * res;
	res = dlopen("/Library/Frameworks/SDL2.framework/SDL2", RTLD_LAZY);
	if (!res) res = dlopen("SDL2.so", RTLD_LAZY);
	if (!res) res = dlopen("libSDL2.so", RTLD_LAZY);
    return res;
}

static void* sdl2_getDllProc(void * aLibrary, const char *aProcName)
{
    return dlsym(aLibrary, aProcName);
}

#endif

static int sdl2_load_dll()
{
#ifdef WINDOWS_VERSION
	HMODULE dll = NULL;
#else
	void * dll = NULL;
#endif

	if (SDL2_OpenAudioDevice != NULL)
	{
		return 1;
	}

    dll = sdl2_openDll();

    if (dll)
    {
		SDL2_WasInit = (SDL2_WasInit_t)sdl2_getDllProc(dll, "SDL_WasInit");
		SDL2_InitSubSystem = (SDL2_InitSubSystem_t)sdl2_getDllProc(dll, "SDL_InitSubSystem");
		SDL2_OpenAudioDevice = (SDL2_OpenAudioDevice_t)sdl2_getDllProc(dll, "SDL_OpenAudioDevice");
		SDL2_CloseAudioDevice = (SDL2_CloseAudioDevice_t)sdl2_getDllProc(dll, "SDL_CloseAudioDevice");
		SDL2_PauseAudioDevice = (SDL2_PauseAudioDevice_t)sdl2_getDllProc(dll, "SDL_PauseAudioDevice");

        if (SDL2_WasInit &&
        	SDL2_InitSubSystem &&
        	SDL2_OpenAudioDevice &&
			SDL2_CloseAudioDevice &&
			SDL2_PauseAudioDevice)
        {
        	return 1;
        }
	}
	SDL2_OpenAudioDevice = NULL;
    return 0;
}

int dll_SDL2_found()
{
	return sdl2_load_dll();
}

Uint32 dll_SDL2_WasInit(Uint32 flags)
{
	if (SDL2_WasInit)
		return SDL2_WasInit(flags);
	return 0;
}

int dll_SDL2_InitSubSystem(Uint32 flags)
{
	if (SDL2_InitSubSystem)
		return SDL2_InitSubSystem(flags);
	return -1;
}

SDL_AudioDeviceID dll_SDL2_OpenAudioDevice(const char*          device,
										   int                  iscapture,
										   const SDL_AudioSpec* desired,
										   SDL_AudioSpec*       obtained,
										   int                  allowed_changes)
{
	if (SDL2_OpenAudioDevice)
		return SDL2_OpenAudioDevice(device, iscapture, desired, obtained, allowed_changes);
	return 0;
}

void dll_SDL2_CloseAudioDevice(SDL_AudioDeviceID dev)
{
	if (SDL2_CloseAudioDevice)
		SDL2_CloseAudioDevice(dev);
}

void dll_SDL2_PauseAudioDevice(SDL_AudioDeviceID dev,
							   int               pause_on)
{
	if (SDL2_PauseAudioDevice)
		SDL2_PauseAudioDevice(dev, pause_on);
}

#endif
// file: soloud/src/backend/sdl2_static/soloud_sdl2_static.cpp
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
#include <stdlib.h>

// #include "soloud.h"

#if !defined(WITH_SDL2_STATIC)

namespace SoLoud
{
	result sdl2static_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
}

#else

#if defined(_MSC_VER)
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <math.h>

namespace SoLoud
{
	static SDL_AudioSpec gActiveAudioSpec;
	static SDL_AudioDeviceID gAudioDeviceID;

	void soloud_sdl2static_audiomixer(void *userdata, Uint8 *stream, int len)
	{
		short *buf = (short*)stream;
		SoLoud::Soloud *soloud = (SoLoud::Soloud *)userdata;
		if (gActiveAudioSpec.format == AUDIO_F32)
		{
			int samples = len / (gActiveAudioSpec.channels * sizeof(float));
			soloud->mix((float *)buf, samples);
		}
		else // assume s16 if not float
		{
			int samples = len / (gActiveAudioSpec.channels * sizeof(short));
			soloud->mixSigned16(buf, samples);
		}
	}

	static void soloud_sdl2static_deinit(SoLoud::Soloud *aSoloud)
	{
		SDL_CloseAudioDevice(gAudioDeviceID);
	}

	result sdl2static_init(SoLoud::Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
	{
		if (!SDL_WasInit(SDL_INIT_AUDIO))
		{
			if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
			{
				return UNKNOWN_ERROR;
			}
		}

		SDL_AudioSpec as;
		as.freq = aSamplerate;
		as.format = AUDIO_F32;
		as.channels = aChannels;
		as.samples = aBuffer;
		as.callback = soloud_sdl2static_audiomixer;
		as.userdata = (void*)aSoloud;

		gAudioDeviceID = SDL_OpenAudioDevice(NULL, 0, &as, &gActiveAudioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~(SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE));
		if (gAudioDeviceID == 0)
		{
			as.format = AUDIO_S16;
			gAudioDeviceID = SDL_OpenAudioDevice(NULL, 0, &as, &gActiveAudioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~(SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE));
			if (gAudioDeviceID == 0)
			{
				return UNKNOWN_ERROR;
			}
		}

		aSoloud->postinit(gActiveAudioSpec.freq, gActiveAudioSpec.samples, aFlags, gActiveAudioSpec.channels);

		aSoloud->mBackendCleanupFunc = soloud_sdl2static_deinit;

		SDL_PauseAudioDevice(gAudioDeviceID, 0);
		aSoloud->mBackendString = "SDL2 (static)";
		return 0;
	}
};
#endif
// file: soloud/src/backend/wasapi/soloud_wasapi.cpp
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

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_WASAPI)

namespace SoLoud
{
	result wasapi_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
};

#else

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#define SAFE_RELEASE(x) \
    if (0 != (x)) \
    { \
        (x)->Release(); \
        (x) = 0; \
    }

namespace SoLoud
{
    struct WASAPIData
    {
        IMMDeviceEnumerator *deviceEnumerator;
        IMMDevice *device;
        IAudioClient *audioClient;
        IAudioRenderClient *renderClient;
        HANDLE bufferEndEvent;
        HANDLE audioProcessingDoneEvent;
        Thread::ThreadHandle thread;
        Soloud *soloud;
        UINT32 bufferFrames;
        int channels;
    };

    static void wasapiSubmitBuffer(WASAPIData *aData, UINT32 aFrames)
    {
        BYTE *buffer = 0;
        if (FAILED(aData->renderClient->GetBuffer(aFrames, &buffer)))
        {
            return;
        }
        aData->soloud->mixSigned16((short *)buffer, aFrames);
        aData->renderClient->ReleaseBuffer(aFrames, 0);
    }

    static void wasapiThread(LPVOID aParam)
    {
        WASAPIData *data = static_cast<WASAPIData*>(aParam);
        wasapiSubmitBuffer(data, data->bufferFrames);
        data->audioClient->Start();
        while (WAIT_OBJECT_0 != WaitForSingleObject(data->audioProcessingDoneEvent, 0))
        {
            WaitForSingleObject(data->bufferEndEvent, INFINITE);
            UINT32 padding = 0;
            if (FAILED(data->audioClient->GetCurrentPadding(&padding)))
            {
                continue;
            }
            UINT32 frames = data->bufferFrames - padding;
            wasapiSubmitBuffer(data, frames);
        }
    }

    static void wasapiCleanup(Soloud *aSoloud)
    {
        if (0 == aSoloud->mBackendData)
        {
            return;
        }
        WASAPIData *data = static_cast<WASAPIData*>(aSoloud->mBackendData);
        SetEvent(data->audioProcessingDoneEvent);
        SetEvent(data->bufferEndEvent);
		if (data->thread)
		{
			Thread::wait(data->thread);
			Thread::release(data->thread);
		}
        CloseHandle(data->bufferEndEvent);
        CloseHandle(data->audioProcessingDoneEvent);
        if (0 != data->audioClient)
        {
            data->audioClient->Stop();
        }
        SAFE_RELEASE(data->renderClient);
        SAFE_RELEASE(data->audioClient);
        SAFE_RELEASE(data->device);
        SAFE_RELEASE(data->deviceEnumerator);
        delete data;
        aSoloud->mBackendData = 0;
        CoUninitialize();
    }

	result wasapi_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
    {
		return UNKNOWN_ERROR;
		CoInitializeEx(0, COINIT_MULTITHREADED);
        WASAPIData *data = new WASAPIData;
        ZeroMemory(data, sizeof(WASAPIData));
        aSoloud->mBackendData = data;
        aSoloud->mBackendCleanupFunc = wasapiCleanup;

        data->bufferEndEvent = CreateEvent(0, FALSE, FALSE, 0);
        if (0 == data->bufferEndEvent)
        {
            return UNKNOWN_ERROR;
        }
        data->audioProcessingDoneEvent = CreateEvent(0, FALSE, FALSE, 0);
        if (0 == data->audioProcessingDoneEvent)
        {
            return UNKNOWN_ERROR;
        }
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL,
                   __uuidof(IMMDeviceEnumerator),
                   reinterpret_cast<void**>(&data->deviceEnumerator))))
        {
            return UNKNOWN_ERROR;
        }
        if (FAILED(data->deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                                   &data->device)))
        {
            return UNKNOWN_ERROR;
        }
        if (FAILED(data->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0,
                                          reinterpret_cast<void**>(&data->audioClient))))
        {
            return UNKNOWN_ERROR;
        }
        WAVEFORMATEX format;
        ZeroMemory(&format, sizeof(WAVEFORMATEX));
        format.nChannels = aChannels;
        format.nSamplesPerSec = aSamplerate;
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.wBitsPerSample = sizeof(short)*8;
        format.nBlockAlign = (format.nChannels*format.wBitsPerSample)/8;
        format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign;
        REFERENCE_TIME dur = static_cast<REFERENCE_TIME>(static_cast<double>(aBuffer)
                                           / (static_cast<double>(aSamplerate)*(1.0/10000000.0)));
		HRESULT res = data->audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			dur, 0, &format, 0);
		if (FAILED(res))
        {
            return UNKNOWN_ERROR;
        }
        data->bufferFrames = 0;
        if (FAILED(data->audioClient->GetBufferSize(&data->bufferFrames)))
        {
            return UNKNOWN_ERROR;
        }
        if (FAILED(data->audioClient->GetService(__uuidof(IAudioRenderClient),
                                                 reinterpret_cast<void**>(&data->renderClient))))
        {
            return UNKNOWN_ERROR;
        }
        if (FAILED(data->audioClient->SetEventHandle(data->bufferEndEvent)))
        {
            return UNKNOWN_ERROR;
        }
        data->channels = format.nChannels;
        data->soloud = aSoloud;
        aSoloud->postinit(aSamplerate, data->bufferFrames * format.nChannels, aFlags, 2);
        data->thread = Thread::createThread(wasapiThread, data);
        if (0 == data->thread)
        {
            return UNKNOWN_ERROR;
        }
        aSoloud->mBackendString = "WASAPI";
        return 0;
    }
};
#endif
// file: soloud/src/backend/winmm/soloud_winmm.cpp
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

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_WINMM)

namespace SoLoud
{
	result winmm_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
};

#else

#include <windows.h>
#include <mmsystem.h>

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

namespace SoLoud
{
    static const int BUFFER_COUNT = 2;

    struct SoLoudWinMMData
    {
        AlignedFloatBuffer buffer;
        short *sampleBuffer[BUFFER_COUNT];
        WAVEHDR header[BUFFER_COUNT];
        HWAVEOUT waveOut;
        HANDLE bufferEndEvent;
        HANDLE audioProcessingDoneEvent;
        Soloud *soloud;
        int samples;
        Thread::ThreadHandle threadHandle;
    };

    static void winMMThread(LPVOID aParam)
    {
        SoLoudWinMMData *data = static_cast<SoLoudWinMMData*>(aParam);
        while (WAIT_OBJECT_0 != WaitForSingleObject(data->audioProcessingDoneEvent, 0))
        {
            for (int i=0;i<BUFFER_COUNT;++i)
            {
                if (0 != (data->header[i].dwFlags & WHDR_INQUEUE))
                {
                    continue;
                }
                short *tgtBuf = data->sampleBuffer[i];

				data->soloud->mixSigned16(tgtBuf, data->samples);

				if (MMSYSERR_NOERROR != waveOutWrite(data->waveOut, &data->header[i],
                                                     sizeof(WAVEHDR)))
                {
                    return;
                }
            }
            WaitForSingleObject(data->bufferEndEvent, INFINITE);
        }
    }

    static void winMMCleanup(Soloud *aSoloud)
    {
        if (0 == aSoloud->mBackendData)
        {
            return;
        }
        SoLoudWinMMData *data = static_cast<SoLoudWinMMData*>(aSoloud->mBackendData);
        SetEvent(data->audioProcessingDoneEvent);
        SetEvent(data->bufferEndEvent);
		if (data->threadHandle)
		{
			Thread::wait(data->threadHandle);
			Thread::release(data->threadHandle);
		}
        waveOutReset(data->waveOut);
        for (int i=0;i<BUFFER_COUNT;++i)
        {
            waveOutUnprepareHeader(data->waveOut, &data->header[i], sizeof(WAVEHDR));
            if (0 != data->sampleBuffer[i])
            {
                delete[] data->sampleBuffer[i];
            }
        }
        waveOutClose(data->waveOut);
        CloseHandle(data->audioProcessingDoneEvent);
        CloseHandle(data->bufferEndEvent);
        delete data;
        aSoloud->mBackendData = 0;
    }

	result winmm_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
    {
        SoLoudWinMMData *data = new SoLoudWinMMData;
        ZeroMemory(data, sizeof(SoLoudWinMMData));
        aSoloud->mBackendData = data;
        aSoloud->mBackendCleanupFunc = winMMCleanup;
        data->samples = aBuffer;
        data->soloud = aSoloud;
        data->bufferEndEvent = CreateEvent(0, FALSE, FALSE, 0);
        if (0 == data->bufferEndEvent)
        {
            return UNKNOWN_ERROR;
        }
        data->audioProcessingDoneEvent = CreateEvent(0, FALSE, FALSE, 0);
        if (0 == data->audioProcessingDoneEvent)
        {
            return UNKNOWN_ERROR;
        }
        WAVEFORMATEX format;
        ZeroMemory(&format, sizeof(WAVEFORMATEX));
        format.nChannels = aChannels;
        format.nSamplesPerSec = aSamplerate;
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.wBitsPerSample = sizeof(short)*8;
        format.nBlockAlign = (format.nChannels*format.wBitsPerSample)/8;
        format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign;
        if (MMSYSERR_NOERROR != waveOutOpen(&data->waveOut, WAVE_MAPPER, &format,
                            reinterpret_cast<DWORD_PTR>(data->bufferEndEvent), 0, CALLBACK_EVENT))
        {
            return UNKNOWN_ERROR;
        }
        data->buffer.init(data->samples*format.nChannels);
        for (int i=0;i<BUFFER_COUNT;++i)
        {
            data->sampleBuffer[i] = new short[data->samples*format.nChannels];
            ZeroMemory(&data->header[i], sizeof(WAVEHDR));
            data->header[i].dwBufferLength = data->samples*sizeof(short)*format.nChannels;
            data->header[i].lpData = reinterpret_cast<LPSTR>(data->sampleBuffer[i]);
            if (MMSYSERR_NOERROR != waveOutPrepareHeader(data->waveOut, &data->header[i],
                                                         sizeof(WAVEHDR)))
            {
                return UNKNOWN_ERROR;
            }
        }
        aSoloud->postinit(aSamplerate, data->samples * format.nChannels, aFlags, aChannels);
        data->threadHandle = Thread::createThread(winMMThread, data);
        if (0 == data->threadHandle)
        {
            return UNKNOWN_ERROR;
        }
        aSoloud->mBackendString = "WinMM";
        return 0;
    }
};

#endif
// file: soloud/src/backend/xaudio2/soloud_xaudio2.cpp
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

// #include "soloud.h"
// #include "soloud_thread.h"

#if !defined(WITH_XAUDIO2)

namespace SoLoud
{
	result xaudio2_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer)
	{
		return NOT_IMPLEMENTED;
	}
};

#else

#include <windows.h>

#ifdef _MSC_VER
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")
#else
// #include "backend/xaudio2/xaudio2.h"
#endif

namespace SoLoud
{
    static const int BUFFER_COUNT = 2;

    struct XAudio2Data
    {
        float *buffer[BUFFER_COUNT];
        IXAudio2 *xaudio2;
        IXAudio2MasteringVoice *masteringVoice;
        IXAudio2SourceVoice *sourceVoice;
        HANDLE bufferEndEvent;
        HANDLE audioProcessingDoneEvent;
        class VoiceCallback *voiceCb;
        Thread::ThreadHandle thread;
        Soloud *soloud;
        int samples;
        UINT32 bufferLengthBytes;
    };

    class VoiceCallback : public IXAudio2VoiceCallback
    {
    public:
        VoiceCallback(HANDLE aBufferEndEvent)
            : IXAudio2VoiceCallback(), mBufferEndEvent(aBufferEndEvent) {}
        virtual ~VoiceCallback() {}

    private:
        // Called just before this voice's processing pass begins.
        void __stdcall OnVoiceProcessingPassStart(UINT32 aBytesRequired) {}

        // Called just after this voice's processing pass ends.
        void __stdcall OnVoiceProcessingPassEnd() {}

        // Called when this voice has just finished playing a buffer stream
        // (as marked with the XAUDIO2_END_OF_STREAM flag on the last buffer).
        void __stdcall OnStreamEnd() {}

        // Called when this voice is about to start processing a new buffer.
        void __stdcall OnBufferStart(void *aBufferContext) {}

        // Called when this voice has just finished processing a buffer.
        // The buffer can now be reused or destroyed.
        void __stdcall OnBufferEnd(void *aBufferContext)
        {
            SetEvent(mBufferEndEvent);
        }

        // Called when this voice has just reached the end position of a loop.
        void __stdcall OnLoopEnd(void *aBufferContext) {}

        // Called in the event of a critical error during voice processing,
        // such as a failing xAPO or an error from the hardware XMA decoder.
        // The voice may have to be destroyed and re-created to recover from
        // the error.  The callback arguments report which buffer was being
        // processed when the error occurred, and its HRESULT code.
        void __stdcall OnVoiceError(void *aBufferContext, HRESULT aError) {}

        HANDLE mBufferEndEvent;
    };

    static void xaudio2Thread(LPVOID aParam)
    {
        XAudio2Data *data = static_cast<XAudio2Data*>(aParam);
        int bufferIndex = 0;
        while (WAIT_OBJECT_0 != WaitForSingleObject(data->audioProcessingDoneEvent, 0))
        {
            XAUDIO2_VOICE_STATE state;
            data->sourceVoice->GetState(&state);
            while (state.BuffersQueued < BUFFER_COUNT)
            {
                data->soloud->mix(data->buffer[bufferIndex], data->samples);
                XAUDIO2_BUFFER info = {0};
                info.AudioBytes = data->bufferLengthBytes;
                info.pAudioData = reinterpret_cast<const BYTE*>(data->buffer[bufferIndex]);
                data->sourceVoice->SubmitSourceBuffer(&info);
                ++bufferIndex;
                if (bufferIndex >= BUFFER_COUNT)
                {
                    bufferIndex = 0;
                }
                data->sourceVoice->GetState(&state);
            }
            WaitForSingleObject(data->bufferEndEvent, INFINITE);
        }
    }

    static void xaudio2Cleanup(Soloud *aSoloud)
    {
        if (0 == aSoloud->mBackendData)
        {
            return;
        }
        XAudio2Data *data = static_cast<XAudio2Data*>(aSoloud->mBackendData);
        SetEvent(data->audioProcessingDoneEvent);
        SetEvent(data->bufferEndEvent);
        Thread::wait(data->thread);
        Thread::release(data->thread);
        if (0 != data->sourceVoice)
        {
            data->sourceVoice->Stop();
            data->sourceVoice->FlushSourceBuffers();
        }
        if (0 != data->xaudio2)
        {
            data->xaudio2->StopEngine();
        }
        if (0 != data->sourceVoice)
        {
            data->sourceVoice->DestroyVoice();
        }
        if (0 != data->voiceCb)
        {
            delete data->voiceCb;
        }
        if (0 != data->masteringVoice)
        {
            data->masteringVoice->DestroyVoice();
        }
        if (0 != data->xaudio2)
        {
            data->xaudio2->Release();
        }
        for (int i=0;i<BUFFER_COUNT;++i)
        {
            if (0 != data->buffer[i])
            {
                delete[] data->buffer[i];
            }
        }
        CloseHandle(data->bufferEndEvent);
        CloseHandle(data->audioProcessingDoneEvent);
        delete data;
        aSoloud->mBackendData = 0;
        CoUninitialize();
    }

    result xaudio2_init(Soloud *aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
    {
        if (FAILED(CoInitializeEx(0, COINIT_MULTITHREADED)))
        {
            return UNKNOWN_ERROR;
        }
        XAudio2Data *data = new XAudio2Data;
        ZeroMemory(data, sizeof(XAudio2Data));
        aSoloud->mBackendData = data;
        aSoloud->mBackendCleanupFunc = xaudio2Cleanup;
        data->bufferEndEvent = CreateEvent(0, FALSE, FALSE, 0);
        if (0 == data->bufferEndEvent)
        {
            return UNKNOWN_ERROR;
        }
        data->audioProcessingDoneEvent = CreateEvent(0, FALSE, FALSE, 0);
        if (0 == data->audioProcessingDoneEvent)
        {
            return UNKNOWN_ERROR;
        }
        WAVEFORMATEX format;
        ZeroMemory(&format, sizeof(WAVEFORMATEX));
        format.nChannels = 2;
        format.nSamplesPerSec = aSamplerate;
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nAvgBytesPerSec = aSamplerate*sizeof(float)*format.nChannels;
        format.nBlockAlign = sizeof(float)*format.nChannels;
        format.wBitsPerSample = sizeof(float)*8;
        if (FAILED(XAudio2Create(&data->xaudio2)))
        {
            return UNKNOWN_ERROR;
        }
        if (FAILED(data->xaudio2->CreateMasteringVoice(&data->masteringVoice,
                                                       format.nChannels, aSamplerate)))
        {
            return UNKNOWN_ERROR;
        }
        data->voiceCb = new VoiceCallback(data->bufferEndEvent);
        if (FAILED(data->xaudio2->CreateSourceVoice(&data->sourceVoice,
                   &format, XAUDIO2_VOICE_NOSRC|XAUDIO2_VOICE_NOPITCH, 2.f, data->voiceCb)))
        {
            return UNKNOWN_ERROR;
        }
        data->bufferLengthBytes = aBuffer * format.nChannels * sizeof(float);
        for (int i=0;i<BUFFER_COUNT;++i)
        {
            data->buffer[i] = new float[aBuffer * format.nChannels];
        }
        data->samples = aBuffer;
        data->soloud = aSoloud;
        aSoloud->postinit(aSamplerate, aBuffer * format.nChannels, aFlags, 2);
        data->thread = Thread::createThread(xaudio2Thread, data);
        if (0 == data->thread)
        {
            return UNKNOWN_ERROR;
        }
        data->sourceVoice->Start();
        aSoloud->mBackendString = "XAudio2";
        return 0;
    }
};

#endif
// file: soloud/src/core/soloud_file.cpp
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

#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
// #include "soloud.h"
// #include "soloud_file.h"

namespace SoLoud
{
	unsigned int File::read8()
	{
		unsigned char d = 0;
		read((unsigned char*)&d, 1);
		return d;
	}

	unsigned int File::read16()
	{
		unsigned short d = 0;
		read((unsigned char*)&d, 2);
		return d;
	}

	unsigned int File::read32()
	{
		unsigned int d = 0;
		read((unsigned char*)&d, 4);
		return d;
	}

DiskFile::DiskFile(FILE *fp):
mFileHandle(fp)
{

}

	unsigned int DiskFile::read(unsigned char *aDst, unsigned int aBytes)
	{
		return (unsigned int)fread(aDst, 1, aBytes, mFileHandle);
	}

	unsigned int DiskFile::length()
	{
		if (!mFileHandle)
			return 0;
		int pos = ftell(mFileHandle);
		fseek(mFileHandle, 0, SEEK_END);
		int len = ftell(mFileHandle);
		fseek(mFileHandle, pos, SEEK_SET);
		return len;
	}

	void DiskFile::seek(int aOffset)
	{
		fseek(mFileHandle, aOffset, SEEK_SET);
	}

	unsigned int DiskFile::pos()
	{
		return ftell(mFileHandle);
	}

	FILE *DiskFile::getFilePtr()
	{
		return mFileHandle;
	}

	DiskFile::~DiskFile()
	{
		if (mFileHandle)
			fclose(mFileHandle);
	}

	DiskFile::DiskFile()
	{
		mFileHandle = 0;
	}

	result DiskFile::open(const char *aFilename)
	{
		if (!aFilename)
			return INVALID_PARAMETER;
		mFileHandle = fopen(aFilename, "rb");
		if (!mFileHandle)
			return FILE_NOT_FOUND;
		return SO_NO_ERROR;
	}

	int DiskFile::eof()
	{
		return feof(mFileHandle);
	}



	unsigned int MemoryFile::read(unsigned char *aDst, unsigned int aBytes)
	{
		if (mOffset + aBytes >= mDataLength)
			aBytes = mDataLength - mOffset;

		memcpy(aDst, mDataPtr + mOffset, aBytes);
		mOffset += aBytes;

		return aBytes;
	}

	unsigned int MemoryFile::length()
	{
		return mDataLength;
	}

	void MemoryFile::seek(int aOffset)
	{
		if (aOffset >= 0)
			mOffset = aOffset;
		else
			mOffset = mDataLength + aOffset;
		if (mOffset > mDataLength-1)
			mOffset = mDataLength-1;
	}

	unsigned int MemoryFile::pos()
	{
		return mOffset;
	}

	unsigned char * MemoryFile::getMemPtr()
	{
		return mDataPtr;
	}

	MemoryFile::~MemoryFile()
	{
		if (mDataOwned)
			delete[] mDataPtr;
	}

	MemoryFile::MemoryFile()
	{
		mDataPtr = 0;
		mDataLength = 0;
		mOffset = 0;
		mDataOwned = false;
	}

	result MemoryFile::openMem(unsigned char *aData, unsigned int aDataLength, bool aCopy, bool aTakeOwnership)
	{
		if (aData == NULL || aDataLength == 0)
			return INVALID_PARAMETER;

		if (mDataOwned)
			delete[] mDataPtr;
		mDataPtr = 0;
		mOffset = 0;

		mDataLength = aDataLength;

		if (aCopy)
		{
			mDataOwned = true;
			mDataPtr = new unsigned char[aDataLength];
			if (mDataPtr == NULL)
				return OUT_OF_MEMORY;
			memcpy(mDataPtr, aData, aDataLength);
			return SO_NO_ERROR;
		}

		mDataPtr = aData;
		mDataOwned = aTakeOwnership;
		return SO_NO_ERROR;
	}

	result MemoryFile::openToMem(const char *aFile)
	{
		if (!aFile)
			return INVALID_PARAMETER;
		if (mDataOwned)
			delete[] mDataPtr;
		mDataPtr = 0;
		mOffset = 0;

		DiskFile df;
		int res = df.open(aFile);
		if (res != SO_NO_ERROR)
			return res;

		mDataLength = df.length();
		mDataPtr = new unsigned char[mDataLength];
		if (mDataPtr == NULL)
			return OUT_OF_MEMORY;
		df.read(mDataPtr, mDataLength);
		mDataOwned = true;
		return SO_NO_ERROR;
	}

	result MemoryFile::openFileToMem(File *aFile)
	{
		if (!aFile)
			return INVALID_PARAMETER;
		if (mDataOwned)
			delete[] mDataPtr;
		mDataPtr = 0;
		mOffset = 0;

		mDataLength = aFile->length();
		mDataPtr = new unsigned char[mDataLength];
		if (mDataPtr == NULL)
			return OUT_OF_MEMORY;
		aFile->read(mDataPtr, mDataLength);
		mDataOwned = true;
		return SO_NO_ERROR;
	}

	int MemoryFile::eof()
	{
		if (mOffset >= mDataLength)
			return 1;
		return 0;
	}
}

extern "C"
{
	int Soloud_Filehack_fgetc(Soloud_Filehack *f)
	{
		SoLoud::File *fp = (SoLoud::File *)f;
		if (fp->eof())
			return EOF;
		return fp->read8();
	}

	int Soloud_Filehack_fread(void *dst, int s, int c, Soloud_Filehack *f)
	{
		SoLoud::File *fp = (SoLoud::File *)f;
		return fp->read((unsigned char*)dst, s*c) / s;

	}

	int Soloud_Filehack_fseek(Soloud_Filehack *f, int idx, int base)
	{
		SoLoud::File *fp = (SoLoud::File *)f;
		switch (base)
		{
		case SEEK_CUR:
			fp->seek(fp->pos() + idx);
			break;
		case SEEK_END:
			fp->seek(fp->length() + idx);
			break;
		default:
			fp->seek(idx);
		}
		return 0;
	}

	int Soloud_Filehack_ftell(Soloud_Filehack *f)
	{
		SoLoud::File *fp = (SoLoud::File *)f;
		return fp->pos();
	}

	int Soloud_Filehack_fclose(Soloud_Filehack *f)
	{
		SoLoud::File *fp = (SoLoud::File *)f;
		delete fp;
		return 0;
	}

	Soloud_Filehack * Soloud_Filehack_fopen(const char *aFilename, char *aMode)
	{
		SoLoud::DiskFile *df = new SoLoud::DiskFile();
		int res = df->open(aFilename);
		if (res != SoLoud::SO_NO_ERROR)
		{
			delete df;
			df = 0;
		}
		return (Soloud_Filehack*)df;
	}
}
// file: soloud/src/core/soloud_audiosource.cpp
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

// #include "soloud.h"

namespace SoLoud
{

	AudioSourceInstance3dData::AudioSourceInstance3dData()
	{
		m3dAttenuationModel = 0;
		m3dAttenuationRolloff = 1;
		m3dDopplerFactor = 1.0;
		m3dMaxDistance = 1000000.0f;
		m3dMinDistance = 0.0f;
		m3dPosition[0] = 0;
		m3dPosition[1] = 0;
		m3dPosition[2] = 0;
		m3dVelocity[0] = 0;
		m3dVelocity[1] = 0;
		m3dVelocity[2] = 0;
		mCollider = 0;
		mColliderData = 0;
		mAttenuator = 0;
	}

	void AudioSourceInstance3dData::init(AudioSource &aSource)
	{
		m3dAttenuationModel = aSource.m3dAttenuationModel;
		m3dAttenuationRolloff = aSource.m3dAttenuationRolloff;
		m3dDopplerFactor = aSource.m3dDopplerFactor;
		m3dMaxDistance = aSource.m3dMaxDistance;
		m3dMinDistance = aSource.m3dMinDistance;
		mCollider = aSource.mCollider;
		mColliderData = aSource.mColliderData;
		mAttenuator = aSource.mAttenuator;
		m3dVolume = 1.0f;
		mDopplerValue = 1.0f;
	}

	AudioSourceInstance::AudioSourceInstance()
	{
		mPlayIndex = 0;
		mFlags = 0;
		mPan = 0;
		// Default all volumes to 1.0 so sound behind N mix busses isn't super quiet.
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
			mChannelVolume[i] = 1.0f;
		mSetVolume = 1.0f;
		mBaseSamplerate = 44100.0f;
		mSamplerate = 44100.0f;
		mSetRelativePlaySpeed = 1.0f;
		mStreamTime = 0.0f;
		mStreamPosition = 0.0f;
		mAudioSourceID = 0;
		mActiveFader = 0;
		mChannels = 1;
		mBusHandle = ~0u;
		mLoopCount = 0;
		mLoopPoint = 0;
		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			mFilter[i] = NULL;
		}
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			mCurrentChannelVolume[i] = 0;
		}
		// behind pointers because we swap between the two buffers
		mResampleData[0] = 0;
		mResampleData[1] = 0;
		mSrcOffset = 0;
		mLeftoverSamples = 0;
		mDelaySamples = 0;

	}

	AudioSourceInstance::~AudioSourceInstance()
	{
		int i;
		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			delete mFilter[i];
		}
	}

	void AudioSourceInstance::init(AudioSource &aSource, int aPlayIndex)
	{
		mPlayIndex = aPlayIndex;
		mBaseSamplerate = aSource.mBaseSamplerate;
		mSamplerate = mBaseSamplerate;
		mChannels = aSource.mChannels;
		mStreamTime = 0.0f;
		mStreamPosition = 0.0f;
		mLoopPoint = aSource.mLoopPoint;

		if (aSource.mFlags & AudioSource::SHOULD_LOOP)
		{
			mFlags |= AudioSourceInstance::LOOPING;
		}
		if (aSource.mFlags & AudioSource::PROCESS_3D)
		{
			mFlags |= AudioSourceInstance::PROCESS_3D;
		}
		if (aSource.mFlags & AudioSource::LISTENER_RELATIVE)
		{
			mFlags |= AudioSourceInstance::LISTENER_RELATIVE;
		}
		if (aSource.mFlags & AudioSource::INAUDIBLE_KILL)
		{
			mFlags |= AudioSourceInstance::INAUDIBLE_KILL;
		}
		if (aSource.mFlags & AudioSource::INAUDIBLE_TICK)
		{
			mFlags |= AudioSourceInstance::INAUDIBLE_TICK;
		}
	}

	result AudioSourceInstance::rewind()
	{
		return NOT_IMPLEMENTED;
	}

	result AudioSourceInstance::seek(double aSeconds, float *mScratch, unsigned int mScratchSize)
	{
		double offset = aSeconds - mStreamPosition;
		if (offset <= 0)
		{
			if (rewind() != SO_NO_ERROR)
			{
				// can't do generic seek backwards unless we can rewind.
				return NOT_IMPLEMENTED;
			}
			offset = aSeconds;
		}
		int samples_to_discard = (int)floor(mSamplerate * offset);

		while (samples_to_discard)
		{
			int samples = mScratchSize / mChannels;
			if (samples > samples_to_discard)
				samples = samples_to_discard;
			getAudio(mScratch, samples, samples);
			samples_to_discard -= samples;
		}
		mStreamPosition = offset;
		return SO_NO_ERROR;
	}


	AudioSource::AudioSource()
	{
		int i;
		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			mFilter[i] = 0;
		}
		mFlags = 0;
		mBaseSamplerate = 44100;
		mAudioSourceID = 0;
		mSoloud = 0;
		mChannels = 1;
		m3dMinDistance = 1;
		m3dMaxDistance = 1000000.0f;
		m3dAttenuationRolloff = 1.0f;
		m3dAttenuationModel = NO_ATTENUATION;
		m3dDopplerFactor = 1.0f;
		mCollider = 0;
		mAttenuator = 0;
		mColliderData = 0;
		mVolume = 1;
		mLoopPoint = 0;
	}

	AudioSource::~AudioSource()
	{
		stop();
	}

	void AudioSource::setVolume(float aVolume)
	{
		mVolume = aVolume;
	}

	void AudioSource::setLoopPoint(time aLoopPoint)
	{
		mLoopPoint = aLoopPoint;
	}

	time AudioSource::getLoopPoint()
	{
		return mLoopPoint;
	}

	void AudioSource::setLooping(bool aLoop)
	{
		if (aLoop)
		{
			mFlags |= SHOULD_LOOP;
		}
		else
		{
			mFlags &= ~SHOULD_LOOP;
		}
	}

	void AudioSource::setSingleInstance(bool aSingleInstance)
	{
		if (aSingleInstance)
		{
			mFlags |= SINGLE_INSTANCE;
		}
		else
		{
			mFlags &= ~SINGLE_INSTANCE;
		}
	}

	void AudioSource::setFilter(unsigned int aFilterId, Filter *aFilter)
	{
		if (aFilterId >= FILTERS_PER_STREAM)
			return;
		mFilter[aFilterId] = aFilter;
	}

	void AudioSource::stop()
	{
		if (mSoloud)
		{
			mSoloud->stopAudioSource(*this);
		}
	}

	void AudioSource::set3dMinMaxDistance(float aMinDistance, float aMaxDistance)
	{
		m3dMinDistance = aMinDistance;
		m3dMaxDistance = aMaxDistance;
	}

	void AudioSource::set3dAttenuation(unsigned int aAttenuationModel, float aAttenuationRolloffFactor)
	{
		m3dAttenuationModel = aAttenuationModel;
		m3dAttenuationRolloff = aAttenuationRolloffFactor;
	}

	void AudioSource::set3dDopplerFactor(float aDopplerFactor)
	{
		m3dDopplerFactor = aDopplerFactor;
	}

	void AudioSource::set3dListenerRelative(bool aListenerRelative)
	{
		if (aListenerRelative)
		{
			mFlags |= LISTENER_RELATIVE;
		}
		else
		{
			mFlags &= ~LISTENER_RELATIVE;
		}
	}


	void AudioSource::set3dDistanceDelay(bool aDistanceDelay)
	{
		if (aDistanceDelay)
		{
			mFlags |= DISTANCE_DELAY;
		}
		else
		{
			mFlags &= ~DISTANCE_DELAY;
		}
	}

	void AudioSource::set3dCollider(AudioCollider *aCollider, int aUserData)
	{
		mCollider = aCollider;
		mColliderData = aUserData;
	}

	void AudioSource::set3dAttenuator(AudioAttenuator *aAttenuator)
	{
		mAttenuator = aAttenuator;
	}

	void AudioSource::setInaudibleBehavior(bool aMustTick, bool aKill)
	{
		mFlags &= ~(AudioSource::INAUDIBLE_KILL | AudioSource::INAUDIBLE_TICK);
		if (aMustTick)
		{
			mFlags |= AudioSource::INAUDIBLE_TICK;
		}
		if (aKill)
		{
			mFlags |= AudioSource::INAUDIBLE_KILL;
		}
	}


	float AudioSourceInstance::getInfo(unsigned int aInfoKey)
	{
	    return 0;
	}


};

// file: soloud/src/core/soloud_core_voicegroup.cpp
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

// #include "soloud.h"

// Voice group operations

namespace SoLoud
{
	// Create a voice group. Returns 0 if unable (out of voice groups / out of memory)
	handle Soloud::createVoiceGroup()
	{
		lockAudioMutex();

		unsigned int i;
		// Check if there's any deleted voice groups and re-use if found
		for (i = 0; i < mVoiceGroupCount; i++)
		{
			if (mVoiceGroup[i] == NULL)
			{
				mVoiceGroup[i] = new unsigned int[16];
				if (mVoiceGroup[i] == NULL)
				{
					unlockAudioMutex();
					return 0;
				}
				mVoiceGroup[i][0] = 16;
				mVoiceGroup[i][1] = 0;
				unlockAudioMutex();
				return 0xfffff000 | i;
			}
		}
		if (mVoiceGroupCount == 4096)
		{
			unlockAudioMutex();
			return 0;
		}
		unsigned int oldcount = mVoiceGroupCount;
		if (mVoiceGroupCount == 0)
		{
			mVoiceGroupCount = 4;
		}
		mVoiceGroupCount *= 2;
		unsigned int **vg = new unsigned int * [mVoiceGroupCount];
		if (vg == NULL)
		{
			mVoiceGroupCount = oldcount;
			unlockAudioMutex();
			return 0;
		}
		for (i = 0; i < oldcount; i++)
		{
			vg[i] = mVoiceGroup[i];
		}

		for (; i < mVoiceGroupCount; i++)
		{
			vg[i] = NULL;
		}

		delete[] mVoiceGroup;
		mVoiceGroup = vg;
		i = oldcount;
		mVoiceGroup[i] = new unsigned int[17];
		if (mVoiceGroup[i] == NULL)
		{
			unlockAudioMutex();
			return 0;
		}
		mVoiceGroup[i][0] = 16;
		mVoiceGroup[i][1] = 0;
		unlockAudioMutex();
		return 0xfffff000 | i;
	}

	// Destroy a voice group.
	result Soloud::destroyVoiceGroup(handle aVoiceGroupHandle)
	{
		if (!isVoiceGroup(aVoiceGroupHandle))
			return INVALID_PARAMETER;
		int c = aVoiceGroupHandle & 0xfff;

		lockAudioMutex();
		delete[] mVoiceGroup[c];
		mVoiceGroup[c] = NULL;
		unlockAudioMutex();
		return SO_NO_ERROR;
	}

	// Add a voice handle to a voice group
	result Soloud::addVoiceToGroup(handle aVoiceGroupHandle, handle aVoiceHandle)
	{
		if (!isVoiceGroup(aVoiceGroupHandle))
			return INVALID_PARAMETER;

		// Don't consider adding invalid voice handles as an error, since the voice may just have ended.
		if (!isValidVoiceHandle(aVoiceHandle))
			return SO_NO_ERROR;

		trimVoiceGroup(aVoiceGroupHandle);

		int c = aVoiceGroupHandle & 0xfff;
		unsigned int i;

		lockAudioMutex();

		for (i = 1; i < mVoiceGroup[c][0]; i++)
		{
			if (mVoiceGroup[c][i] == aVoiceHandle)
			{
				unlockAudioMutex();
				return SO_NO_ERROR; // already there
			}

			if (mVoiceGroup[c][i] == 0)
			{
				mVoiceGroup[c][i] = aVoiceHandle;
				mVoiceGroup[c][i + 1] = 0;

				unlockAudioMutex();
				return SO_NO_ERROR;
			}
		}

		// Full group, allocate more memory
		unsigned int * n = new unsigned int[mVoiceGroup[c][0] * 2 + 1];
		if (n == NULL)
		{
			unlockAudioMutex();
			return OUT_OF_MEMORY;
		}
		for (i = 0; i < mVoiceGroup[c][0]; i++)
			n[i] = mVoiceGroup[c][i];
		n[n[0]] = aVoiceHandle;
		n[n[0]+1] = 0;
		n[0] *= 2;
		delete[] mVoiceGroup[c];
		mVoiceGroup[c] = n;
		unlockAudioMutex();
		return SO_NO_ERROR;
	}

	// Is this handle a valid voice group?
	bool Soloud::isVoiceGroup(handle aVoiceGroupHandle)
	{
		if ((aVoiceGroupHandle & 0xfffff000) != 0xfffff000)
			return 0;
		unsigned int c = aVoiceGroupHandle & 0xfff;
		if (c >= mVoiceGroupCount)
			return 0;

		lockAudioMutex();
		bool res = mVoiceGroup[c] != NULL;
		unlockAudioMutex();

		return res;
	}

	// Is this voice group empty?
	bool Soloud::isVoiceGroupEmpty(handle aVoiceGroupHandle)
	{
		// If not a voice group, yeah, we're empty alright..
		if (!isVoiceGroup(aVoiceGroupHandle))
			return 1;
		trimVoiceGroup(aVoiceGroupHandle);
		int c = aVoiceGroupHandle & 0xfff;

		lockAudioMutex();
		bool res = mVoiceGroup[c][1] == 0;
		unlockAudioMutex();

		return res;
	}

	// Remove all non-active voices from group
	void Soloud::trimVoiceGroup(handle aVoiceGroupHandle)
	{
		if (!isVoiceGroup(aVoiceGroupHandle))
			return;
		int c = aVoiceGroupHandle & 0xfff;

		lockAudioMutex();
		// empty group
		if (mVoiceGroup[c][1] == 0)
		{
			unlockAudioMutex();
			return;
		}

		unsigned int i;
		for (i = 1; i < mVoiceGroup[c][0]; i++)
		{
			if (mVoiceGroup[c][i] == 0)
			{
				unlockAudioMutex();
				return;
			}

			unlockAudioMutex();
			while (!isValidVoiceHandle(mVoiceGroup[c][i])) // function locks mutex, so we need to unlock it before the call
			{
				lockAudioMutex();
				unsigned int j;
				for (j = i; j < mVoiceGroup[c][0] - 1; j++)
				{
					mVoiceGroup[c][j] = mVoiceGroup[c][j + 1];
					if (mVoiceGroup[c][j] == 0)
						break;
				}
				mVoiceGroup[c][mVoiceGroup[c][0] - 1] = 0;
				if (mVoiceGroup[c][i] == 0)
				{
					unlockAudioMutex();
					return;
				}
			}
			lockAudioMutex();
		}
		unlockAudioMutex();
	}

	handle *Soloud::voiceGroupHandleToArray(handle aVoiceGroupHandle) const
	{
		if ((aVoiceGroupHandle & 0xfffff000) != 0xfffff000)
			return NULL;
		unsigned int c = aVoiceGroupHandle & 0xfff;
		if (c >= mVoiceGroupCount)
			return NULL;
		if (mVoiceGroup[c] == NULL)
			return NULL;
		return mVoiceGroup[c] + 1;
	}

}
// file: soloud/src/core/soloud_core_filterops.cpp
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

// #include "soloud_internal.h"

// Core operations related to filters

namespace SoLoud
{
	void Soloud::setGlobalFilter(unsigned int aFilterId, Filter *aFilter)
	{
		if (aFilterId >= FILTERS_PER_STREAM)
			return;

		lockAudioMutex();
		delete mFilterInstance[aFilterId];
		mFilterInstance[aFilterId] = 0;

		mFilter[aFilterId] = aFilter;
		if (aFilter)
		{
			mFilterInstance[aFilterId] = mFilter[aFilterId]->createInstance();
		}
		unlockAudioMutex();
	}

	float Soloud::getFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId)
	{
		float ret = INVALID_PARAMETER;
		if (aFilterId >= FILTERS_PER_STREAM)
			return ret;

		if (aVoiceHandle == 0)
		{
			lockAudioMutex();
			if (mFilterInstance[aFilterId])
			{
				ret = mFilterInstance[aFilterId]->getFilterParameter(aAttributeId);
			}
			unlockAudioMutex();
			return ret;
		}

		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			return ret;
		}
		lockAudioMutex();
		if (mVoice[ch] &&
			mVoice[ch]->mFilter[aFilterId])
		{
			ret = mVoice[ch]->mFilter[aFilterId]->getFilterParameter(aAttributeId);
		}
		unlockAudioMutex();

		return ret;
	}

	void Soloud::setFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId, float aValue)
	{
		if (aFilterId >= FILTERS_PER_STREAM)
			return;

		if (aVoiceHandle == 0)
		{
			lockAudioMutex();
			if (mFilterInstance[aFilterId])
			{
				mFilterInstance[aFilterId]->setFilterParameter(aAttributeId, aValue);
			}
			unlockAudioMutex();
			return;
		}

		FOR_ALL_VOICES_PRE
		if (mVoice[ch] &&
			mVoice[ch]->mFilter[aFilterId])
		{
			mVoice[ch]->mFilter[aFilterId]->setFilterParameter(aAttributeId, aValue);
		}
		FOR_ALL_VOICES_POST
	}

	void Soloud::fadeFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId, float aTo, double aTime)
	{
		if (aFilterId >= FILTERS_PER_STREAM)
			return;

		if (aVoiceHandle == 0)
		{
			lockAudioMutex();
			if (mFilterInstance[aFilterId])
			{
				mFilterInstance[aFilterId]->fadeFilterParameter(aAttributeId, aTo, aTime, mStreamTime);
			}
			unlockAudioMutex();
			return;
		}

		FOR_ALL_VOICES_PRE
		if (mVoice[ch] &&
			mVoice[ch]->mFilter[aFilterId])
		{
			mVoice[ch]->mFilter[aFilterId]->fadeFilterParameter(aAttributeId, aTo, aTime, mStreamTime);
		}
		FOR_ALL_VOICES_POST
	}

	void Soloud::oscillateFilterParameter(handle aVoiceHandle, unsigned int aFilterId, unsigned int aAttributeId, float aFrom, float aTo, double aTime)
	{
		if (aFilterId >= FILTERS_PER_STREAM)
			return;

		if (aVoiceHandle == 0)
		{
			lockAudioMutex();
			if (mFilterInstance[aFilterId])
			{
				mFilterInstance[aFilterId]->oscillateFilterParameter(aAttributeId, aFrom, aTo, aTime, mStreamTime);
			}
			unlockAudioMutex();
			return;
		}

		FOR_ALL_VOICES_PRE
		if (mVoice[ch] &&
			mVoice[ch]->mFilter[aFilterId])
		{
			mVoice[ch]->mFilter[aFilterId]->oscillateFilterParameter(aAttributeId, aFrom, aTo, aTime, mStreamTime);
		}
		FOR_ALL_VOICES_POST
	}

}
// file: soloud/src/core/soloud_core_basicops.cpp
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

#include <string.h>
// #include "soloud_internal.h"

// Core "basic" operations - play, stop, etc

namespace SoLoud
{
	handle Soloud::play(AudioSource &aSound, float aVolume, float aPan, bool aPaused, unsigned int aBus)
	{
		if (aSound.mFlags & AudioSource::SINGLE_INSTANCE)
		{
			// Only one instance allowed, stop others
			aSound.stop();
		}

		// Creation of an audio instance may take significant amount of time,
		// so let's not do it inside the audio thread mutex.
		aSound.mSoloud = this;
		SoLoud::AudioSourceInstance *instance = aSound.createInstance();

		lockAudioMutex();
		int ch = findFreeVoice();
		if (ch < 0)
		{
			unlockAudioMutex();
			delete instance;
			return UNKNOWN_ERROR;
		}
		if (!aSound.mAudioSourceID)
		{
			aSound.mAudioSourceID = mAudioSourceID;
			mAudioSourceID++;
		}
		mVoice[ch] = instance;
		mVoice[ch]->mAudioSourceID = aSound.mAudioSourceID;
		mVoice[ch]->mBusHandle = aBus;
		mVoice[ch]->init(aSound, mPlayIndex);
		m3dData[ch].init(aSound);

		mPlayIndex++;

		// 20 bits, skip the last one (top bits full = voice group)
		if (mPlayIndex == 0xfffff)
		{
			mPlayIndex = 0;
		}

		if (aPaused)
		{
			mVoice[ch]->mFlags |= AudioSourceInstance::PAUSED;
		}

		setVoicePan(ch, aPan);
		if (aVolume < 0)
		{
			setVoiceVolume(ch, aSound.mVolume);
		}
		else
		{
			setVoiceVolume(ch, aVolume);
		}

		// Fix initial voice volume ramp up
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			mVoice[ch]->mCurrentChannelVolume[i] = mVoice[ch]->mChannelVolume[i] * mVoice[ch]->mOverallVolume;
		}

		setVoiceRelativePlaySpeed(ch, 1);

		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			if (aSound.mFilter[i])
			{
				mVoice[ch]->mFilter[i] = aSound.mFilter[i]->createInstance();
			}
		}

		mActiveVoiceDirty = true;

		unlockAudioMutex();

		int handle = getHandleFromVoice(ch);
		return handle;
	}

	handle Soloud::playClocked(time aSoundTime, AudioSource &aSound, float aVolume, float aPan, unsigned int aBus)
	{
		handle h = play(aSound, aVolume, aPan, 1, aBus);
		lockAudioMutex();
		time lasttime = mLastClockedTime;
		if (lasttime == 0)
			mLastClockedTime = aSoundTime;
		unlockAudioMutex();
		int samples = 0;
		if (aSoundTime > lasttime)
		{
			samples = (int)floor((aSoundTime - lasttime) * mSamplerate);
		}
		setDelaySamples(h, samples);
		setPause(h, 0);
		return h;
	}

	handle Soloud::playBackground(AudioSource &aSound, float aVolume, bool aPaused, unsigned int aBus)
	{
		handle h = play(aSound, aVolume, 0.0f, aPaused, aBus);
		setPanAbsolute(h, 1.0f, 1.0f);
		return h;
	}

	result Soloud::seek(handle aVoiceHandle, time aSeconds)
	{
		result res = SO_NO_ERROR;
		result singleres = SO_NO_ERROR;
		FOR_ALL_VOICES_PRE
			singleres = mVoice[ch]->seek(aSeconds, mScratch.mData, mScratchSize);
		if (singleres != SO_NO_ERROR)
			res = singleres;
		FOR_ALL_VOICES_POST
		return res;
	}


	void Soloud::stop(handle aVoiceHandle)
	{
		FOR_ALL_VOICES_PRE
			stopVoice(ch);
		FOR_ALL_VOICES_POST
	}

	void Soloud::stopAudioSource(AudioSource &aSound)
	{
		if (aSound.mAudioSourceID)
		{
			lockAudioMutex();

			int i;
			for (i = 0; i < (signed)mHighestVoice; i++)
			{
				if (mVoice[i] && mVoice[i]->mAudioSourceID == aSound.mAudioSourceID)
				{
					stopVoice(i);
				}
			}
			unlockAudioMutex();
		}
	}

	void Soloud::stopAll()
	{
		int i;
		lockAudioMutex();
		for (i = 0; i < (signed)mHighestVoice; i++)
		{
			stopVoice(i);
		}
		unlockAudioMutex();
	}

	int Soloud::countAudioSource(AudioSource &aSound)
	{
		int count = 0;
		if (aSound.mAudioSourceID)
		{
			lockAudioMutex();

			int i;
			for (i = 0; i < (signed)mHighestVoice; i++)
			{
				if (mVoice[i] && mVoice[i]->mAudioSourceID == aSound.mAudioSourceID)
				{
					count++;
				}
			}
			unlockAudioMutex();
		}
		return count;
	}

}
// file: soloud/src/core/soloud_core_voiceops.cpp
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

// #include "soloud.h"

// Direct voice operations (no mutexes - called from other functions)

namespace SoLoud
{
	result Soloud::setVoiceRelativePlaySpeed(unsigned int aVoice, float aSpeed)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		if (aSpeed <= 0.0f)
		{
			return INVALID_PARAMETER;
		}

		if (mVoice[aVoice])
		{
			mVoice[aVoice]->mSetRelativePlaySpeed = aSpeed;
			updateVoiceRelativePlaySpeed(aVoice);
		}

		return 0;
	}

	void Soloud::setVoicePause(unsigned int aVoice, int aPause)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		mActiveVoiceDirty = true;
		if (mVoice[aVoice])
		{
			mVoice[aVoice]->mPauseScheduler.mActive = 0;

			if (aPause)
			{
				mVoice[aVoice]->mFlags |= AudioSourceInstance::PAUSED;
			}
			else
			{
				mVoice[aVoice]->mFlags &= ~AudioSourceInstance::PAUSED;
			}
		}
	}

	void Soloud::setVoicePan(unsigned int aVoice, float aPan)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		if (mVoice[aVoice])
		{
			mVoice[aVoice]->mPan = aPan;
			float l = (float)cos((aPan + 1) * M_PI / 4);
			float r = (float)sin((aPan + 1) * M_PI / 4);
			mVoice[aVoice]->mChannelVolume[0] = l;
			mVoice[aVoice]->mChannelVolume[1] = r;
			if (mVoice[aVoice]->mChannels == 4)
			{
				mVoice[aVoice]->mChannelVolume[2] = l;
				mVoice[aVoice]->mChannelVolume[3] = r;
			}
			if (mVoice[aVoice]->mChannels == 6)
			{
				mVoice[aVoice]->mChannelVolume[2] = 1.0f / (float)sqrt(2.0f);
				mVoice[aVoice]->mChannelVolume[3] = 1;
				mVoice[aVoice]->mChannelVolume[4] = l;
				mVoice[aVoice]->mChannelVolume[5] = r;
			}
			if (mVoice[aVoice]->mChannels == 8)
			{
				mVoice[aVoice]->mChannelVolume[2] = 1.0f / (float)sqrt(2.0f);
				mVoice[aVoice]->mChannelVolume[3] = 1;
				mVoice[aVoice]->mChannelVolume[4] = l;
				mVoice[aVoice]->mChannelVolume[5] = r;
				mVoice[aVoice]->mChannelVolume[6] = l;
				mVoice[aVoice]->mChannelVolume[7] = r;
			}
		}
	}

	void Soloud::setVoiceVolume(unsigned int aVoice, float aVolume)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		mActiveVoiceDirty = true;
		if (mVoice[aVoice])
		{
			mVoice[aVoice]->mSetVolume = aVolume;
			updateVoiceVolume(aVoice);
		}
	}

	void Soloud::stopVoice(unsigned int aVoice)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		mActiveVoiceDirty = true;
		if (mVoice[aVoice])
		{
			// Delete via temporary variable to avoid recursion
			AudioSourceInstance * v = mVoice[aVoice];
			mVoice[aVoice] = 0;
			delete v;

			unsigned int i;
			for (i = 0; i < mMaxActiveVoices; i++)
			{
				if (mResampleDataOwner[i] == v)
				{
					mResampleDataOwner[i] = NULL;
				}
			}
		}
	}

	void Soloud::updateVoiceRelativePlaySpeed(unsigned int aVoice)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		mVoice[aVoice]->mOverallRelativePlaySpeed = m3dData[aVoice].mDopplerValue * mVoice[aVoice]->mSetRelativePlaySpeed;
		mVoice[aVoice]->mSamplerate = mVoice[aVoice]->mBaseSamplerate * mVoice[aVoice]->mOverallRelativePlaySpeed;
	}

	void Soloud::updateVoiceVolume(unsigned int aVoice)
	{
		SOLOUD_ASSERT(aVoice < VOICE_COUNT);
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		mVoice[aVoice]->mOverallVolume = mVoice[aVoice]->mSetVolume * m3dData[aVoice].m3dVolume;
	}
}
// file: soloud/src/core/soloud_fader.cpp
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

// #include "soloud.h"

namespace SoLoud
{
	Fader::Fader()
	{
		mCurrent = mFrom = mTo = mDelta = 0;
		mTime = mStartTime = mEndTime = 0;
		mActive = 0;
	}

	void Fader::set(float aFrom, float aTo, double aTime, double aStartTime)
	{
		mCurrent = mFrom;
		mFrom = aFrom;
		mTo = aTo;
		mTime = aTime;
		mStartTime = aStartTime;
		mDelta = aTo - aFrom;
		mEndTime = mStartTime + mTime;
		mActive = 1;
	}

	void Fader::setLFO(float aFrom, float aTo, double aTime, double aStartTime)
	{
		mActive = 2;
		mCurrent = 0;
		mFrom = aFrom;
		mTo = aTo;
		mTime = aTime;
		mDelta = (aTo - aFrom) / 2;
		if (mDelta < 0) mDelta = -mDelta;
		mStartTime = aStartTime;
		mEndTime = (float)M_PI * 2 / mTime;
	}

	float Fader::get(double aCurrentTime)
	{
		if (mActive == 2)
		{
			// LFO mode
			if (mStartTime > aCurrentTime)
			{
				// Time rolled over.
				mStartTime = aCurrentTime;
			}
			double t = aCurrentTime - mStartTime;
			return (float)(sin(t * mEndTime) * mDelta + (mFrom + mDelta));

		}
		if (mStartTime > aCurrentTime)
		{
			// Time rolled over.
			// Figure out where we were..
			float p = (mCurrent - mFrom) / mDelta; // 0..1
			mFrom = mCurrent;
			mStartTime = aCurrentTime;
			mTime = mTime * (1 - p); // time left
			mDelta = mTo - mFrom;
			mEndTime = mStartTime + mTime;
		}
		if (aCurrentTime > mEndTime)
		{
			mActive = -1;
			return mTo;
		}
		mCurrent = (float)(mFrom + mDelta * ((aCurrentTime - mStartTime) / mTime));
		return mCurrent;
	}
};
// file: soloud/src/core/soloud_filter.cpp
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

// #include "soloud.h"

namespace SoLoud
{

	Filter::Filter()
	{
	}

	Filter::~Filter()
	{
	}

	FilterInstance::FilterInstance()
	{
		mNumParams = 0;
		mParamChanged = 0;
		mParam = 0;
		mParamFader = 0;
	}

	result FilterInstance::initParams(int aNumParams)
	{
		mNumParams = aNumParams;
		delete[] mParam;
		delete[] mParamFader;
		mParam = new float[mNumParams];
		mParamFader = new Fader[mNumParams];

		if (mParam == NULL || mParamFader == NULL)
		{
			delete[] mParam;
			delete[] mParamFader;
			mParam = NULL;
			mParamFader = NULL;
			mNumParams = 0;
			return OUT_OF_MEMORY;
		}

		unsigned int i;
		for (i = 0; i < mNumParams; i++)
		{
			mParam[i] = 0;
			mParamFader[i].mActive = 0;
		}
		mParam[0] = 1; // set 'wet' to 1

		return 0;
	}

	void FilterInstance::updateParams(double aTime)
	{
		unsigned int i;
		for (i = 0; i < mNumParams; i++)
		{
			if (mParamFader[i].mActive > 0)
			{
				mParamChanged |= 1 << i;
				mParam[i] = mParamFader[i].get(aTime);
			}
		}
	}

	FilterInstance::~FilterInstance()
	{
		delete[] mParam;
		delete[] mParamFader;
	}

	void FilterInstance::setFilterParameter(unsigned int aAttributeId, float aValue)
	{
		if (aAttributeId >= mNumParams)
			return;

		mParamFader[aAttributeId].mActive = 0;
		mParam[aAttributeId] = aValue;
		mParamChanged |= 1 << aAttributeId;
	}

	void FilterInstance::fadeFilterParameter(unsigned int aAttributeId, float aTo, double aTime, double aStartTime)
	{
		if (aAttributeId >= mNumParams || aTime <= 0 || aTo == mParam[aAttributeId])
			return;

		mParamFader[aAttributeId].set(mParam[aAttributeId], aTo, aTime, aStartTime);
	}

	void FilterInstance::oscillateFilterParameter(unsigned int aAttributeId, float aFrom, float aTo, double aTime, double aStartTime)
	{
		if (aAttributeId >= mNumParams || aTime <= 0 || aFrom == aTo)
			return;

		mParamFader[aAttributeId].setLFO(aFrom, aTo, aTime, aStartTime);
	}

	float FilterInstance::getFilterParameter(unsigned int aAttributeId)
	{
		if (aAttributeId >= mNumParams)
			return 0;

		return mParam[aAttributeId];
	}

	void FilterInstance::filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, double aTime)
	{
		unsigned int i;
		for (i = 0; i < aChannels; i++)
		{
			filterChannel(aBuffer + i * aSamples, aSamples, aSamplerate, aTime, i, aChannels);
		}
	}

	void FilterInstance::filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, double aTime, unsigned int aChannel, unsigned int aChannels)
	{
	}

};

// file: soloud/src/core/soloud_core_getters.cpp
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

// #include "soloud.h"

// Getters - return information about SoLoud state

namespace SoLoud
{
	unsigned int Soloud::getVersion() const
	{
		return SOLOUD_VERSION;
	}

	float Soloud::getPostClipScaler() const
	{
		return mPostClipScaler;
	}

	float Soloud::getGlobalVolume() const
	{
		return mGlobalVolume;
	}

	handle Soloud::getHandleFromVoice(unsigned int aVoice) const
	{
		if (mVoice[aVoice] == 0)
			return 0;
		return (aVoice + 1) | (mVoice[aVoice]->mPlayIndex << 12);
	}

	int Soloud::getVoiceFromHandle(handle aVoiceHandle) const
	{
		// If this is a voice group handle, pick the first handle from the group
		handle *h = voiceGroupHandleToArray(aVoiceHandle);
		if (h != NULL) aVoiceHandle = *h;

		if (aVoiceHandle == 0)
		{
			return -1;
		}

		int ch = (aVoiceHandle & 0xfff) - 1;
		unsigned int idx = aVoiceHandle >> 12;
		if (mVoice[ch] &&
			(mVoice[ch]->mPlayIndex & 0xfffff) == idx)
		{
			return ch;
		}
		return -1;
	}

	unsigned int Soloud::getMaxActiveVoiceCount() const
	{
		return mMaxActiveVoices;
	}

	unsigned int Soloud::getActiveVoiceCount()
	{
		lockAudioMutex();
		if (mActiveVoiceDirty)
			calcActiveVoices();
		unsigned int c = mActiveVoiceCount;
		unlockAudioMutex();
		return c;
	}

	unsigned int Soloud::getVoiceCount()
	{
		lockAudioMutex();
		int i;
		int c = 0;
		for (i = 0; i < (signed)mHighestVoice; i++)
		{
			if (mVoice[i])
			{
				c++;
			}
		}
		unlockAudioMutex();
		return c;
	}

	bool Soloud::isValidVoiceHandle(handle aVoiceHandle)
	{
		// voice groups are not valid voice handles
		if ((aVoiceHandle & 0xfffff000) == 0xfffff000)
			return 0;

		lockAudioMutex();
		if (getVoiceFromHandle(aVoiceHandle) != -1)
		{
			unlockAudioMutex();
			return 1;
		}
		unlockAudioMutex();
		return 0;
	}


	time Soloud::getLoopPoint(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		time v = mVoice[ch]->mLoopPoint;
		unlockAudioMutex();
		return v;
	}

	bool Soloud::getLooping(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		bool v = (mVoice[ch]->mFlags & AudioSourceInstance::LOOPING) != 0;
		unlockAudioMutex();
		return v;
	}

	float Soloud::getInfo(handle aVoiceHandle, unsigned int mInfoKey)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		float v = mVoice[ch]->getInfo(mInfoKey);
		unlockAudioMutex();
		return v;
	}

	float Soloud::getVolume(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		float v = mVoice[ch]->mSetVolume;
		unlockAudioMutex();
		return v;
	}

	float Soloud::getOverallVolume(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		float v = mVoice[ch]->mOverallVolume;
		unlockAudioMutex();
		return v;
	}

	float Soloud::getPan(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		float v = mVoice[ch]->mPan;
		unlockAudioMutex();
		return v;
	}

	time Soloud::getStreamTime(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		double v = mVoice[ch]->mStreamTime;
		unlockAudioMutex();
		return v;
	}

	time Soloud::getStreamPosition(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		double v = mVoice[ch]->mStreamPosition;
		unlockAudioMutex();
		return v;
	}

	float Soloud::getRelativePlaySpeed(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 1;
		}
		float v = mVoice[ch]->mSetRelativePlaySpeed;
		unlockAudioMutex();
		return v;
	}

	float Soloud::getSamplerate(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		float v = mVoice[ch]->mBaseSamplerate;
		unlockAudioMutex();
		return v;
	}

	bool Soloud::getPause(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		int v = !!(mVoice[ch]->mFlags & AudioSourceInstance::PAUSED);
		unlockAudioMutex();
		return v != 0;
	}

	bool Soloud::getProtectVoice(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		int v = !!(mVoice[ch]->mFlags & AudioSourceInstance::PROTECTED);
		unlockAudioMutex();
		return v != 0;
	}

	int Soloud::findFreeVoice()
	{
		int i;
		unsigned int lowest_play_index_value = 0xffffffff;
		int lowest_play_index = -1;

		// (slowly) drag the highest active voice index down
		if (mHighestVoice > 0 && mVoice[mHighestVoice - 1] == NULL)
			mHighestVoice--;

		for (i = 0; i < VOICE_COUNT; i++)
		{
			if (mVoice[i] == NULL)
			{
				if (i+1 > (signed)mHighestVoice)
				{
					mHighestVoice = i + 1;
				}
				return i;
			}
			if (((mVoice[i]->mFlags & AudioSourceInstance::PROTECTED) == 0) &&
				mVoice[i]->mPlayIndex < lowest_play_index_value)
			{
				lowest_play_index_value = mVoice[i]->mPlayIndex;
				lowest_play_index = i;
			}
		}
		stopVoice(lowest_play_index);
		return lowest_play_index;
	}

	unsigned int Soloud::getLoopCount(handle aVoiceHandle)
	{
		lockAudioMutex();
		int ch = getVoiceFromHandle(aVoiceHandle);
		if (ch == -1)
		{
			unlockAudioMutex();
			return 0;
		}
		int v = mVoice[ch]->mLoopCount;
		unlockAudioMutex();
		return v;
	}

	// Returns current backend ID
	unsigned int Soloud::getBackendId()
	{
		return mBackendID;

	}

	// Returns current backend string
	const char * Soloud::getBackendString()
	{
		return mBackendString;
	}

	// Returns current backend channel count (1 mono, 2 stereo, etc)
	unsigned int Soloud::getBackendChannels()
	{
		return mChannels;
	}

	// Returns current backend sample rate
	unsigned int Soloud::getBackendSamplerate()
	{
		return mSamplerate;
	}

	// Returns current backend buffer size
	unsigned int Soloud::getBackendBufferSize()
	{
		return mBufferSize;
	}

	// Get speaker position in 3d space
	result Soloud::getSpeakerPosition(unsigned int aChannel, float &aX, float &aY, float &aZ)
	{
		if (aChannel >= mChannels)
			return INVALID_PARAMETER;
		aX = m3dSpeakerPosition[3 * aChannel + 0];
		aY = m3dSpeakerPosition[3 * aChannel + 1];
		aZ = m3dSpeakerPosition[3 * aChannel + 2];
		return SO_NO_ERROR;
	}


}
// file: soloud/src/core/soloud_queue.cpp
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

// #include "soloud.h"

namespace SoLoud
{
	QueueInstance::QueueInstance(Queue *aParent)
	{
		mParent = aParent;
		mFlags |= PROTECTED;
	}

	unsigned int QueueInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize)
	{
		if (mParent->mCount == 0)
		{
			return 0;
		}
		unsigned int copycount = aSamplesToRead;
		unsigned int copyofs = 0;
		while (copycount && mParent->mCount)
		{
			int readcount = mParent->mSource[mParent->mReadIndex]->getAudio(aBuffer + copyofs, copycount, aBufferSize);
			copyofs += readcount;
			copycount -= readcount;
			if (mParent->mSource[mParent->mReadIndex]->hasEnded())
			{
				delete mParent->mSource[mParent->mReadIndex];
				mParent->mSource[mParent->mReadIndex] = 0;
				mParent->mReadIndex = (mParent->mReadIndex + 1) % SOLOUD_QUEUE_MAX;
				mParent->mCount--;
			}
		}
		return copyofs;
	}

	bool QueueInstance::hasEnded()
	{
		return mParent->mCount == 0;
	}

	QueueInstance::~QueueInstance()
	{
	}

	Queue::Queue()
	{
		mQueueHandle = 0;
		mInstance = 0;
		mReadIndex = 0;
		mWriteIndex = 0;
		mCount = 0;
	}

	QueueInstance * Queue::createInstance()
	{
		if (mInstance)
		{
			stop();
			mInstance = 0;
		}
		mInstance = new QueueInstance(this);
		return mInstance;
	}

	void Queue::findQueueHandle()
	{
		// Find the channel the queue is playing on to calculate handle..
		int i;
		for (i = 0; mQueueHandle == 0 && i < (signed)mSoloud->mHighestVoice; i++)
		{
			if (mSoloud->mVoice[i] == mInstance)
			{
				mQueueHandle = mSoloud->getHandleFromVoice(i);
			}
		}
	}

	result Queue::play(AudioSource &aSound)
	{
		if (!mSoloud)
		{
			return INVALID_PARAMETER;
		}

		findQueueHandle();

		if (mQueueHandle == 0)
			return INVALID_PARAMETER;

		if (mCount >= SOLOUD_QUEUE_MAX)
			return OUT_OF_MEMORY;

		if (!aSound.mAudioSourceID)
		{
			aSound.mAudioSourceID = mSoloud->mAudioSourceID;
			mSoloud->mAudioSourceID++;
		}

		SoLoud::AudioSourceInstance *instance = aSound.createInstance();

		if (instance == 0)
		{
			return OUT_OF_MEMORY;
		}

		instance->mAudioSourceID = aSound.mAudioSourceID;

		mSoloud->lockAudioMutex();
		mSource[mWriteIndex] = instance;
		mWriteIndex = (mWriteIndex + 1) % SOLOUD_QUEUE_MAX;
		mCount++;
		mSoloud->unlockAudioMutex();

		return SO_NO_ERROR;
	}


	unsigned int Queue::getQueueCount()
	{
		unsigned int count;
		mSoloud->lockAudioMutex();
		count = mCount;
		mSoloud->unlockAudioMutex();
		return count;
	}

	bool Queue::isCurrentlyPlaying(AudioSource &aSound)
	{
		if (mCount == 0 || aSound.mAudioSourceID == 0)
			return false;
		mSoloud->lockAudioMutex();
		bool res = mSource[mReadIndex]->mAudioSourceID == aSound.mAudioSourceID;
		mSoloud->unlockAudioMutex();
		return res;
	}

	result Queue::setParamsFromAudioSource(AudioSource &aSound)
	{
		mChannels = aSound.mChannels;
		mBaseSamplerate = aSound.mBaseSamplerate;

	    return SO_NO_ERROR;
	}

	result Queue::setParams(float aSamplerate, unsigned int aChannels)
	{
	    if (aChannels < 1 || aChannels > MAX_CHANNELS)
	        return INVALID_PARAMETER;
		mChannels = aChannels;
		mBaseSamplerate = aSamplerate;
	    return SO_NO_ERROR;
	}
};
// file: soloud/src/core/soloud_bus.cpp
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

// #include "soloud.h"
// #include "soloud_fft.h"

namespace SoLoud
{
	BusInstance::BusInstance(Bus *aParent)
	{
		mParent = aParent;
		mScratchSize = 0;
		mFlags |= PROTECTED | INAUDIBLE_TICK;
	}

	unsigned int BusInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize)
	{
		int handle = mParent->mChannelHandle;
		if (handle == 0)
		{
			// Avoid reuse of scratch data if this bus hasn't played anything yet
			unsigned int i;
			for (i = 0; i < aBufferSize * mChannels; i++)
				aBuffer[i] = 0;
			return aSamplesToRead;
		}

		Soloud *s = mParent->mSoloud;
		if (s->mScratchNeeded != mScratchSize)
		{
			mScratchSize = s->mScratchNeeded;
			mScratch.init(mScratchSize * MAX_CHANNELS);
		}

		s->mixBus(aBuffer, aSamplesToRead, aBufferSize, mScratch.mData, handle, mSamplerate, mChannels);

		int i;
		if (mParent->mFlags & AudioSource::VISUALIZATION_DATA)
		{
			for (i = 0; i < MAX_CHANNELS; i++)
				mVisualizationChannelVolume[i] = 0;

			if (aSamplesToRead > 255)
			{
				for (i = 0; i < 256; i++)
				{
					int j;
					mVisualizationWaveData[i] = 0;
					for (j = 0; j < (signed)mChannels; j++)
					{
						float sample = aBuffer[i + aBufferSize * j];
						float absvol = (float)fabs(sample);
						if (absvol > mVisualizationChannelVolume[j])
							mVisualizationChannelVolume[j] = absvol;
						mVisualizationWaveData[i] += sample;
					}
				}
			}
			else
			{
				// Very unlikely failsafe branch
				for (i = 0; i < 256; i++)
				{
					int j;
					mVisualizationWaveData[i] = 0;
					for (j = 0; j < (signed)mChannels; j++)
					{
						float sample = aBuffer[(i % aSamplesToRead) + aBufferSize * j];
						float absvol = (float)fabs(sample);
						if (absvol > mVisualizationChannelVolume[j])
							mVisualizationChannelVolume[j] = absvol;
						mVisualizationWaveData[i] += sample;
					}
				}
			}
		}
		return aSamplesToRead;
	}

	bool BusInstance::hasEnded()
	{
		// Busses never stop for fear of going under 50mph.
		return 0;
	}

	BusInstance::~BusInstance()
	{
		Soloud *s = mParent->mSoloud;
		int i;
		for (i = 0; i < (signed)s->mHighestVoice; i++)
		{
			if (s->mVoice[i] && s->mVoice[i]->mBusHandle == mParent->mChannelHandle)
			{
				s->stopVoice(i);
			}
		}
	}

	Bus::Bus()
	{
		mChannelHandle = 0;
		mInstance = 0;
		mChannels = 2;
	}

	BusInstance * Bus::createInstance()
	{
		if (mChannelHandle)
		{
			stop();
			mChannelHandle = 0;
			mInstance = 0;
		}
		mInstance = new BusInstance(this);
		return mInstance;
	}

	void Bus::findBusHandle()
	{
		if (mChannelHandle == 0)
		{
			// Find the channel the bus is playing on to calculate handle..
			int i;
			for (i = 0; mChannelHandle == 0 && i < (signed)mSoloud->mHighestVoice; i++)
			{
				if (mSoloud->mVoice[i] == mInstance)
				{
					mChannelHandle = mSoloud->getHandleFromVoice(i);
				}
			}
		}
	}

	handle Bus::play(AudioSource &aSound, float aVolume, float aPan, bool aPaused)
	{
		if (!mInstance || !mSoloud)
		{
			return 0;
		}

		findBusHandle();

		if (mChannelHandle == 0)
		{
			return 0;
		}
		return mSoloud->play(aSound, aVolume, aPan, aPaused, mChannelHandle);
	}


	handle Bus::playClocked(time aSoundTime, AudioSource &aSound, float aVolume, float aPan)
	{
		if (!mInstance || !mSoloud)
		{
			return 0;
		}

		findBusHandle();

		if (mChannelHandle == 0)
		{
			return 0;
		}

		return mSoloud->playClocked(aSoundTime, aSound, aVolume, aPan, mChannelHandle);
	}

	handle Bus::play3d(AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX, float aVelY, float aVelZ, float aVolume, bool aPaused)
	{
		if (!mInstance || !mSoloud)
		{
			return 0;
		}

		findBusHandle();

		if (mChannelHandle == 0)
		{
			return 0;
		}
		return mSoloud->play3d(aSound, aPosX, aPosY, aPosZ, aVelX, aVelY, aVelZ, aVolume, aPaused, mChannelHandle);
	}

	handle Bus::play3dClocked(time aSoundTime, AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX, float aVelY, float aVelZ, float aVolume)
	{
		if (!mInstance || !mSoloud)
		{
			return 0;
		}

		findBusHandle();

		if (mChannelHandle == 0)
		{
			return 0;
		}
		return mSoloud->play3dClocked(aSoundTime, aSound, aPosX, aPosY, aPosZ, aVelX, aVelY, aVelZ, aVolume, mChannelHandle);
	}


	void Bus::setFilter(unsigned int aFilterId, Filter *aFilter)
	{
		if (aFilterId >= FILTERS_PER_STREAM)
			return;

		mFilter[aFilterId] = aFilter;

		if (mInstance)
		{
			mSoloud->lockAudioMutex();
			delete mInstance->mFilter[aFilterId];
			mInstance->mFilter[aFilterId] = 0;

			if (aFilter)
			{
				mInstance->mFilter[aFilterId] = mFilter[aFilterId]->createInstance();
			}
			mSoloud->unlockAudioMutex();
		}
	}

	result Bus::setChannels(unsigned int aChannels)
	{
		if (aChannels == 0 || aChannels == 3 || aChannels == 5 || aChannels > 7 || aChannels > MAX_CHANNELS)
			return INVALID_PARAMETER;
		mChannels = aChannels;
		return SO_NO_ERROR;
	}

	void Bus::setVisualizationEnable(bool aEnable)
	{
		if (aEnable)
		{
			mFlags |= AudioSource::VISUALIZATION_DATA;
		}
		else
		{
			mFlags &= ~AudioSource::VISUALIZATION_DATA;
		}
	}

	float * Bus::calcFFT()
	{
		if (mInstance && mSoloud)
		{
			mSoloud->lockAudioMutex();
			float temp[1024];
			int i;
			for (i = 0; i < 256; i++)
			{
				temp[i*2] = mInstance->mVisualizationWaveData[i];
				temp[i*2+1] = 0;
				temp[i+512] = 0;
				temp[i+768] = 0;
			}
			mSoloud->unlockAudioMutex();

			SoLoud::FFT::fft1024(temp);

			for (i = 0; i < 256; i++)
			{
				float real = temp[i * 2];
				float imag = temp[i * 2 + 1];
				mFFTData[i] = (float)sqrt(real*real+imag*imag);
			}
		}

		return mFFTData;
	}

	float * Bus::getWave()
	{
		if (mInstance && mSoloud)
		{
			int i;
			mSoloud->lockAudioMutex();
			for (i = 0; i < 256; i++)
				mWaveData[i] = mInstance->mVisualizationWaveData[i];
			mSoloud->unlockAudioMutex();
		}
		return mWaveData;
	}

	float Bus::getApproximateVolume(unsigned int aChannel)
	{
		if (aChannel > mChannels)
			return 0;
		float vol = 0;
		if (mInstance && mSoloud)
		{
			mSoloud->lockAudioMutex();
			vol = mInstance->mVisualizationChannelVolume[aChannel];
			mSoloud->unlockAudioMutex();
		}
		return vol;
	}
};
// file: soloud/src/core/soloud_thread.cpp
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

#if defined(_WIN32)||defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

// #include "soloud.h"
// #include "soloud_thread.h"

namespace SoLoud
{
	namespace Thread
	{
#ifdef WINDOWS_VERSION
        struct ThreadHandleData
        {
            HANDLE thread;
        };

		void * createMutex()
		{
			CRITICAL_SECTION * cs = new CRITICAL_SECTION;
			InitializeCriticalSectionAndSpinCount(cs, 100);
			return (void*)cs;
		}

		void destroyMutex(void *aHandle)
		{
			CRITICAL_SECTION *cs = (CRITICAL_SECTION*)aHandle;
			DeleteCriticalSection(cs);
			delete cs;
		}

		void lockMutex(void *aHandle)
		{
			CRITICAL_SECTION *cs = (CRITICAL_SECTION*)aHandle;
			if (cs)
			{
				EnterCriticalSection(cs);
			}
		}

		void unlockMutex(void *aHandle)
		{
			CRITICAL_SECTION *cs = (CRITICAL_SECTION*)aHandle;
			if (cs)
			{
				LeaveCriticalSection(cs);
			}
		}

		struct soloud_thread_data
		{
			threadFunction mFunc;
			void *mParam;
		};

		static DWORD WINAPI threadfunc(LPVOID d)
		{
			soloud_thread_data *p = (soloud_thread_data *)d;
			p->mFunc(p->mParam);
			delete p;
			return 0;
		}

        ThreadHandle createThread(threadFunction aThreadFunction, void *aParameter)
		{
			soloud_thread_data *d = new soloud_thread_data;
			d->mFunc = aThreadFunction;
			d->mParam = aParameter;
			HANDLE h = CreateThread(NULL,0,threadfunc,d,0,NULL);
            if (0 == h)
            {
                return 0;
            }
            ThreadHandleData *threadHandle = new ThreadHandleData;
            threadHandle->thread = h;
            return threadHandle;
		}

		void sleep(int aMSec)
		{
			Sleep(aMSec);
		}

        void wait(ThreadHandle aThreadHandle)
        {
            WaitForSingleObject(aThreadHandle->thread, INFINITE);
        }

        void release(ThreadHandle aThreadHandle)
        {
            CloseHandle(aThreadHandle->thread);
            delete aThreadHandle;
        }

#else // pthreads
        struct ThreadHandleData
        {
            pthread_t thread;
        };

		void * createMutex()
		{
			pthread_mutex_t *mutex;
			mutex = new pthread_mutex_t;

			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);

			pthread_mutex_init(mutex, &attr);

			return (void*)mutex;
		}

		void destroyMutex(void *aHandle)
		{
			pthread_mutex_t *mutex = (pthread_mutex_t*)aHandle;

			if (mutex)
			{
				pthread_mutex_destroy(mutex);
				delete mutex;
			}
		}

		void lockMutex(void *aHandle)
		{
			pthread_mutex_t *mutex = (pthread_mutex_t*)aHandle;
			if (mutex)
			{
				pthread_mutex_lock(mutex);
			}
		}

		void unlockMutex(void *aHandle)
		{
			pthread_mutex_t *mutex = (pthread_mutex_t*)aHandle;
			if (mutex)
			{
				pthread_mutex_unlock(mutex);
			}
		}

		struct soloud_thread_data
		{
			threadFunction mFunc;
			void *mParam;
		};

		static void * threadfunc(void * d)
		{
			soloud_thread_data *p = (soloud_thread_data *)d;
			p->mFunc(p->mParam);
			delete p;
			return 0;
		}

		ThreadHandle createThread(threadFunction aThreadFunction, void *aParameter)
		{
			soloud_thread_data *d = new soloud_thread_data;
			d->mFunc = aThreadFunction;
			d->mParam = aParameter;

			ThreadHandleData *threadHandle = new ThreadHandleData;
			pthread_create(&threadHandle->thread, NULL, threadfunc, (void*)d);
            return threadHandle;
		}

		void sleep(int aMSec)
		{
			usleep(aMSec * 1000);
		}

        void wait(ThreadHandle aThreadHandle)
        {
            pthread_join(aThreadHandle->thread, 0);
        }

        void release(ThreadHandle aThreadHandle)
        {
            delete aThreadHandle;
        }
#endif

		static void poolWorker(void *aParam)
		{
			Pool *myPool = (Pool*)aParam;
			while (myPool->mRunning)
			{
				PoolTask *t = myPool->getWork();
				if (!t)
				{
					sleep(1);
				}
				else
				{
					t->work();
				}
			}
		}

		Pool::Pool()
		{
			mRunning = 0;
			mThreadCount = 0;
			mThread = 0;
			mWorkMutex = 0;
			mRobin = 0;
			mMaxTask = 0;
		}

		Pool::~Pool()
		{
			mRunning = 0;
			int i;
			for (i = 0; i < mThreadCount; i++)
			{
				wait(mThread[i]);
				release(mThread[i]);
			}
			delete[] mThread;
			if (mWorkMutex)
				destroyMutex(mWorkMutex);
		}

		void Pool::init(int aThreadCount)
		{
			if (aThreadCount > 0)
			{
				mMaxTask = 0;
				mWorkMutex = createMutex();
				mRunning = 1;
				mThreadCount = aThreadCount;
				mThread = new ThreadHandle[aThreadCount];
				int i;
				for (i = 0; i < mThreadCount; i++)
				{
					mThread[i] = createThread(poolWorker, this);
				}
			}
		}

		void Pool::addWork(PoolTask *aTask)
		{
			if (mThreadCount == 0)
			{
				aTask->work();
			}
			else
			{
				if (mWorkMutex) lockMutex(mWorkMutex);
				if (mMaxTask == MAX_THREADPOOL_TASKS)
				{
					// If we're at max tasks, do the task on calling thread
					// (we're in trouble anyway, might as well slow down adding more work)
					if (mWorkMutex) unlockMutex(mWorkMutex);
					aTask->work();
				}
				else
				{
					mTaskArray[mMaxTask] = aTask;
					mMaxTask++;
					if (mWorkMutex) unlockMutex(mWorkMutex);
				}
			}
		}

		PoolTask * Pool::getWork()
		{
			PoolTask *t = 0;
			if (mWorkMutex) lockMutex(mWorkMutex);
			if (mMaxTask > 0)
			{
				int r = mRobin % mMaxTask;
				mRobin++;
				t = mTaskArray[r];
				mTaskArray[r] = mTaskArray[mMaxTask - 1];
				mMaxTask--;
			}
			if (mWorkMutex) unlockMutex(mWorkMutex);
			return t;
		}
	}
}
// file: soloud/src/core/soloud_fft_lut.cpp
/* **************************************************
 *  WARNING: this is a generated file. Do not edit. *
 *  Any edits will be overwritten by the generator. *
 ************************************************** */

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

/* SoLoud Lookup Table Generator (c)2015 Jari Komppa http://iki.fi/sol/ */

int Soloud_fft_bitrev_10[1024] = {
0, 512, 256, 768, 128, 640, 384, 896, 64, 576, 320, 832, 192, 704, 448,
960, 32, 544, 288, 800, 160, 672, 416, 928, 96, 608, 352, 864, 224, 736,
480, 992, 16, 528, 272, 784, 144, 656, 400, 912, 80, 592, 336, 848, 208,
720, 464, 976, 48, 560, 304, 816, 176, 688, 432, 944, 112, 624, 368, 880,
240, 752, 496, 1008, 8, 520, 264, 776, 136, 648, 392, 904, 72, 584, 328,
840, 200, 712, 456, 968, 40, 552, 296, 808, 168, 680, 424, 936, 104, 616,
360, 872, 232, 744, 488, 1000, 24, 536, 280, 792, 152, 664, 408, 920, 88,
600, 344, 856, 216, 728, 472, 984, 56, 568, 312, 824, 184, 696, 440, 952,
120, 632, 376, 888, 248, 760, 504, 1016, 4, 516, 260, 772, 132, 644, 388,
900, 68, 580, 324, 836, 196, 708, 452, 964, 36, 548, 292, 804, 164, 676,
420, 932, 100, 612, 356, 868, 228, 740, 484, 996, 20, 532, 276, 788, 148,
660, 404, 916, 84, 596, 340, 852, 212, 724, 468, 980, 52, 564, 308, 820,
180, 692, 436, 948, 116, 628, 372, 884, 244, 756, 500, 1012, 12, 524, 268,
780, 140, 652, 396, 908, 76, 588, 332, 844, 204, 716, 460, 972, 44, 556,
300, 812, 172, 684, 428, 940, 108, 620, 364, 876, 236, 748, 492, 1004,
28, 540, 284, 796, 156, 668, 412, 924, 92, 604, 348, 860, 220, 732, 476,
988, 60, 572, 316, 828, 188, 700, 444, 956, 124, 636, 380, 892, 252, 764,
508, 1020, 2, 514, 258, 770, 130, 642, 386, 898, 66, 578, 322, 834, 194,
706, 450, 962, 34, 546, 290, 802, 162, 674, 418, 930, 98, 610, 354, 866,
226, 738, 482, 994, 18, 530, 274, 786, 146, 658, 402, 914, 82, 594, 338,
850, 210, 722, 466, 978, 50, 562, 306, 818, 178, 690, 434, 946, 114, 626,
370, 882, 242, 754, 498, 1010, 10, 522, 266, 778, 138, 650, 394, 906, 74,
586, 330, 842, 202, 714, 458, 970, 42, 554, 298, 810, 170, 682, 426, 938,
106, 618, 362, 874, 234, 746, 490, 1002, 26, 538, 282, 794, 154, 666, 410,
922, 90, 602, 346, 858, 218, 730, 474, 986, 58, 570, 314, 826, 186, 698,
442, 954, 122, 634, 378, 890, 250, 762, 506, 1018, 6, 518, 262, 774, 134,
646, 390, 902, 70, 582, 326, 838, 198, 710, 454, 966, 38, 550, 294, 806,
166, 678, 422, 934, 102, 614, 358, 870, 230, 742, 486, 998, 22, 534, 278,
790, 150, 662, 406, 918, 86, 598, 342, 854, 214, 726, 470, 982, 54, 566,
310, 822, 182, 694, 438, 950, 118, 630, 374, 886, 246, 758, 502, 1014,
14, 526, 270, 782, 142, 654, 398, 910, 78, 590, 334, 846, 206, 718, 462,
974, 46, 558, 302, 814, 174, 686, 430, 942, 110, 622, 366, 878, 238, 750,
494, 1006, 30, 542, 286, 798, 158, 670, 414, 926, 94, 606, 350, 862, 222,
734, 478, 990, 62, 574, 318, 830, 190, 702, 446, 958, 126, 638, 382, 894,
254, 766, 510, 1022, 1, 513, 257, 769, 129, 641, 385, 897, 65, 577, 321,
833, 193, 705, 449, 961, 33, 545, 289, 801, 161, 673, 417, 929, 97, 609,
353, 865, 225, 737, 481, 993, 17, 529, 273, 785, 145, 657, 401, 913, 81,
593, 337, 849, 209, 721, 465, 977, 49, 561, 305, 817, 177, 689, 433, 945,
113, 625, 369, 881, 241, 753, 497, 1009, 9, 521, 265, 777, 137, 649, 393,
905, 73, 585, 329, 841, 201, 713, 457, 969, 41, 553, 297, 809, 169, 681,
425, 937, 105, 617, 361, 873, 233, 745, 489, 1001, 25, 537, 281, 793, 153,
665, 409, 921, 89, 601, 345, 857, 217, 729, 473, 985, 57, 569, 313, 825,
185, 697, 441, 953, 121, 633, 377, 889, 249, 761, 505, 1017, 5, 517, 261,
773, 133, 645, 389, 901, 69, 581, 325, 837, 197, 709, 453, 965, 37, 549,
293, 805, 165, 677, 421, 933, 101, 613, 357, 869, 229, 741, 485, 997, 21,
533, 277, 789, 149, 661, 405, 917, 85, 597, 341, 853, 213, 725, 469, 981,
53, 565, 309, 821, 181, 693, 437, 949, 117, 629, 373, 885, 245, 757, 501,
1013, 13, 525, 269, 781, 141, 653, 397, 909, 77, 589, 333, 845, 205, 717,
461, 973, 45, 557, 301, 813, 173, 685, 429, 941, 109, 621, 365, 877, 237,
749, 493, 1005, 29, 541, 285, 797, 157, 669, 413, 925, 93, 605, 349, 861,
221, 733, 477, 989, 61, 573, 317, 829, 189, 701, 445, 957, 125, 637, 381,
893, 253, 765, 509, 1021, 3, 515, 259, 771, 131, 643, 387, 899, 67, 579,
323, 835, 195, 707, 451, 963, 35, 547, 291, 803, 163, 675, 419, 931, 99,
611, 355, 867, 227, 739, 483, 995, 19, 531, 275, 787, 147, 659, 403, 915,
83, 595, 339, 851, 211, 723, 467, 979, 51, 563, 307, 819, 179, 691, 435,
947, 115, 627, 371, 883, 243, 755, 499, 1011, 11, 523, 267, 779, 139, 651,
395, 907, 75, 587, 331, 843, 203, 715, 459, 971, 43, 555, 299, 811, 171,
683, 427, 939, 107, 619, 363, 875, 235, 747, 491, 1003, 27, 539, 283, 795,
155, 667, 411, 923, 91, 603, 347, 859, 219, 731, 475, 987, 59, 571, 315,
827, 187, 699, 443, 955, 123, 635, 379, 891, 251, 763, 507, 1019, 7, 519,
263, 775, 135, 647, 391, 903, 71, 583, 327, 839, 199, 711, 455, 967, 39,
551, 295, 807, 167, 679, 423, 935, 103, 615, 359, 871, 231, 743, 487, 999,
23, 535, 279, 791, 151, 663, 407, 919, 87, 599, 343, 855, 215, 727, 471,
983, 55, 567, 311, 823, 183, 695, 439, 951, 119, 631, 375, 887, 247, 759,
503, 1015, 15, 527, 271, 783, 143, 655, 399, 911, 79, 591, 335, 847, 207,
719, 463, 975, 47, 559, 303, 815, 175, 687, 431, 943, 111, 623, 367, 879,
239, 751, 495, 1007, 31, 543, 287, 799, 159, 671, 415, 927, 95, 607, 351,
863, 223, 735, 479, 991, 63, 575, 319, 831, 191, 703, 447, 959, 127, 639,
383, 895, 255, 767, 511, 1023};

int Soloud_fft_bitrev_8[256] = {
0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255
};

float Soloud_fft_trig_10[508] = {
1.000000000000000000f, 0.923879504203796390f, 0.707106769084930420f,
0.382683396339416500f, 1.000000000000000000f, 0.980785250663757320f,
0.923879504203796390f, 0.831469595432281490f, 0.707106769084930420f,
0.555570185184478760f, 0.382683396339416500f, 0.195090278983116150f,
1.000000000000000000f, 0.995184719562530520f, 0.980785250663757320f,
0.956940352916717530f, 0.923879504203796390f, 0.881921231746673580f,
0.831469595432281490f, 0.773010432720184330f, 0.707106769084930420f,
0.634393274784088130f, 0.555570185184478760f, 0.471396714448928830f,
0.382683396339416500f, 0.290284633636474610f, 0.195090278983116150f,
0.098017096519470215f, 1.000000000000000000f, 0.998795449733734130f,
0.995184719562530520f, 0.989176511764526370f, 0.980785250663757320f,
0.970031261444091800f, 0.956940352916717530f, 0.941544055938720700f,
0.923879504203796390f, 0.903989315032958980f, 0.881921231746673580f,
0.857728600502014160f, 0.831469595432281490f, 0.803207516670227050f,
0.773010432720184330f, 0.740951120853424070f, 0.707106769084930420f,
0.671558916568756100f, 0.634393274784088130f, 0.595699310302734380f,
0.555570185184478760f, 0.514102697372436520f, 0.471396714448928830f,
0.427555054426193240f, 0.382683396339416500f, 0.336889833211898800f,
0.290284633636474610f, 0.242980137467384340f, 0.195090278983116150f,
0.146730437874794010f, 0.098017096519470215f, 0.049067631363868713f,
1.000000000000000000f, 0.999698817729949950f, 0.998795449733734130f,
0.997290432453155520f, 0.995184719562530520f, 0.992479562759399410f,
0.989176511764526370f, 0.985277652740478520f, 0.980785250663757320f,
0.975702106952667240f, 0.970031261444091800f, 0.963776051998138430f,
0.956940352916717530f, 0.949528157711029050f, 0.941544055938720700f,
0.932992815971374510f, 0.923879504203796390f, 0.914209723472595210f,
0.903989315032958980f, 0.893224298954010010f, 0.881921231746673580f,
0.870086967945098880f, 0.857728600502014160f, 0.844853579998016360f,
0.831469595432281490f, 0.817584812641143800f, 0.803207516670227050f,
0.788346409797668460f, 0.773010432720184330f, 0.757208824157714840f,
0.740951120853424070f, 0.724247097969055180f, 0.707106769084930420f,
0.689540505409240720f, 0.671558916568756100f, 0.653172850608825680f,
0.634393274784088130f, 0.615231573581695560f, 0.595699310302734380f,
0.575808167457580570f, 0.555570185184478760f, 0.534997582435607910f,
0.514102697372436520f, 0.492898166179656980f, 0.471396714448928830f,
0.449611306190490720f, 0.427555054426193240f, 0.405241280794143680f,
0.382683396339416500f, 0.359894990921020510f, 0.336889833211898800f,
0.313681721687316890f, 0.290284633636474610f, 0.266712725162506100f,
0.242980137467384340f, 0.219101205468177800f, 0.195090278983116150f,
0.170961856842041020f, 0.146730437874794010f, 0.122410632669925690f,
0.098017096519470215f, 0.073564521968364716f, 0.049067631363868713f,
0.024541186168789864f, 1.000000000000000000f, 0.999924719333648680f,
0.999698817729949950f, 0.999322354793548580f, 0.998795449733734130f,
0.998118102550506590f, 0.997290432453155520f, 0.996312618255615230f,
0.995184719562530520f, 0.993906974792480470f, 0.992479562759399410f,
0.990902662277221680f, 0.989176511764526370f, 0.987301409244537350f,
0.985277652740478520f, 0.983105480670928960f, 0.980785250663757320f,
0.978317379951477050f, 0.975702106952667240f, 0.972939968109130860f,
0.970031261444091800f, 0.966976463794708250f, 0.963776051998138430f,
0.960430502891540530f, 0.956940352916717530f, 0.953306019306182860f,
0.949528157711029050f, 0.945607304573059080f, 0.941544055938720700f,
0.937339007854461670f, 0.932992815971374510f, 0.928506076335906980f,
0.923879504203796390f, 0.919113874435424800f, 0.914209723472595210f,
0.909168004989624020f, 0.903989315032958980f, 0.898674488067626950f,
0.893224298954010010f, 0.887639641761779790f, 0.881921231746673580f,
0.876070082187652590f, 0.870086967945098880f, 0.863972842693328860f,
0.857728600502014160f, 0.851355195045471190f, 0.844853579998016360f,
0.838224709033966060f, 0.831469595432281490f, 0.824589312076568600f,
0.817584812641143800f, 0.810457170009613040f, 0.803207516670227050f,
0.795836865901947020f, 0.788346409797668460f, 0.780737221240997310f,
0.773010432720184330f, 0.765167236328125000f, 0.757208824157714840f,
0.749136388301849370f, 0.740951120853424070f, 0.732654273509979250f,
0.724247097969055180f, 0.715730786323547360f, 0.707106769084930420f,
0.698376238346099850f, 0.689540505409240720f, 0.680601000785827640f,
0.671558916568756100f, 0.662415742874145510f, 0.653172850608825680f,
0.643831551074981690f, 0.634393274784088130f, 0.624859452247619630f,
0.615231573581695560f, 0.605511009693145750f, 0.595699310302734380f,
0.585797846317291260f, 0.575808167457580570f, 0.565731763839721680f,
0.555570185184478760f, 0.545324981212615970f, 0.534997582435607910f,
0.524589657783508300f, 0.514102697372436520f, 0.503538370132446290f,
0.492898166179656980f, 0.482183754444122310f, 0.471396714448928830f,
0.460538685321807860f, 0.449611306190490720f, 0.438616216182708740f,
0.427555054426193240f, 0.416429519653320310f, 0.405241280794143680f,
0.393992006778717040f, 0.382683396339416500f, 0.371317178010940550f,
0.359894990921020510f, 0.348418653011322020f, 0.336889833211898800f,
0.325310260057449340f, 0.313681721687316890f, 0.302005916833877560f,
0.290284633636474610f, 0.278519660234451290f, 0.266712725162506100f,
0.254865616559982300f, 0.242980137467384340f, 0.231058076024055480f,
0.219101205468177800f, 0.207111343741416930f, 0.195090278983116150f,
0.183039844036102290f, 0.170961856842041020f, 0.158858105540275570f,
0.146730437874794010f, 0.134580671787261960f, 0.122410632669925690f,
0.110222168266773220f, 0.098017096519470215f, 0.085797272622585297f,
0.073564521968364716f, 0.061320696026086807f, 0.049067631363868713f,
0.036807179450988770f, 0.024541186168789864f, 0.012271494604647160f,
1.000000000000000000f, 0.999981164932250980f, 0.999924719333648680f,
0.999830603599548340f, 0.999698817729949950f, 0.999529421329498290f,
0.999322354793548580f, 0.999077737331390380f, 0.998795449733734130f,
0.998475551605224610f, 0.998118102550506590f, 0.997723042964935300f,
0.997290432453155520f, 0.996820271015167240f, 0.996312618255615230f,
0.995767414569854740f, 0.995184719562530520f, 0.994564592838287350f,
0.993906974792480470f, 0.993211925029754640f, 0.992479562759399410f,
0.991709768772125240f, 0.990902662277221680f, 0.990058183670043950f,
0.989176511764526370f, 0.988257586956024170f, 0.987301409244537350f,
0.986308097839355470f, 0.985277652740478520f, 0.984210073947906490f,
0.983105480670928960f, 0.981963872909545900f, 0.980785250663757320f,
0.979569792747497560f, 0.978317379951477050f, 0.977028131484985350f,
0.975702106952667240f, 0.974339365959167480f, 0.972939968109130860f,
0.971503913402557370f, 0.970031261444091800f, 0.968522071838378910f,
0.966976463794708250f, 0.965394437313079830f, 0.963776051998138430f,
0.962121427059173580f, 0.960430502891540530f, 0.958703458309173580f,
0.956940352916717530f, 0.955141186714172360f, 0.953306019306182860f,
0.951435029506683350f, 0.949528157711029050f, 0.947585582733154300f,
0.945607304573059080f, 0.943593442440032960f, 0.941544055938720700f,
0.939459204673767090f, 0.937339007854461670f, 0.935183525085449220f,
0.932992815971374510f, 0.930766940116882320f, 0.928506076335906980f,
0.926210224628448490f, 0.923879504203796390f, 0.921514034271240230f,
0.919113874435424800f, 0.916679084300994870f, 0.914209723472595210f,
0.911706030368804930f, 0.909168004989624020f, 0.906595706939697270f,
0.903989315032958980f, 0.901348829269409180f, 0.898674488067626950f,
0.895966231822967530f, 0.893224298954010010f, 0.890448689460754390f,
0.887639641761779790f, 0.884797096252441410f, 0.881921231746673580f,
0.879012227058410640f, 0.876070082187652590f, 0.873094975948333740f,
0.870086967945098880f, 0.867046236991882320f, 0.863972842693328860f,
0.860866904258728030f, 0.857728600502014160f, 0.854557991027832030f,
0.851355195045471190f, 0.848120331764221190f, 0.844853579998016360f,
0.841554939746856690f, 0.838224709033966060f, 0.834862887859344480f,
0.831469595432281490f, 0.828045010566711430f, 0.824589312076568600f,
0.821102499961853030f, 0.817584812641143800f, 0.814036309719085690f,
0.810457170009613040f, 0.806847572326660160f, 0.803207516670227050f,
0.799537241458892820f, 0.795836865901947020f, 0.792106568813323970f,
0.788346409797668460f, 0.784556567668914790f, 0.780737221240997310f,
0.776888430118560790f, 0.773010432720184330f, 0.769103348255157470f,
0.765167236328125000f, 0.761202394962310790f, 0.757208824157714840f,
0.753186762332916260f, 0.749136388301849370f, 0.745057761669158940f,
0.740951120853424070f, 0.736816525459289550f, 0.732654273509979250f,
0.728464365005493160f, 0.724247097969055180f, 0.720002472400665280f,
0.715730786323547360f, 0.711432158946990970f, 0.707106769084930420f,
0.702754735946655270f, 0.698376238346099850f, 0.693971455097198490f,
0.689540505409240720f, 0.685083627700805660f, 0.680601000785827640f,
0.676092684268951420f, 0.671558916568756100f, 0.666999876499176030f,
0.662415742874145510f, 0.657806694507598880f, 0.653172850608825680f,
0.648514389991760250f, 0.643831551074981690f, 0.639124453067779540f,
0.634393274784088130f, 0.629638195037841800f, 0.624859452247619630f,
0.620057165622711180f, 0.615231573581695560f, 0.610382795333862300f,
0.605511009693145750f, 0.600616455078125000f, 0.595699310302734380f,
0.590759694576263430f, 0.585797846317291260f, 0.580813944339752200f,
0.575808167457580570f, 0.570780694484710690f, 0.565731763839721680f,
0.560661554336547850f, 0.555570185184478760f, 0.550457954406738280f,
0.545324981212615970f, 0.540171444416046140f, 0.534997582435607910f,
0.529803574085235600f, 0.524589657783508300f, 0.519355952739715580f,
0.514102697372436520f, 0.508830130100250240f, 0.503538370132446290f,
0.498227655887603760f, 0.492898166179656980f, 0.487550139427185060f,
0.482183754444122310f, 0.476799190044403080f, 0.471396714448928830f,
0.465976476669311520f, 0.460538685321807860f, 0.455083549022674560f,
0.449611306190490720f, 0.444122105836868290f, 0.438616216182708740f,
0.433093786239624020f, 0.427555054426193240f, 0.422000229358673100f,
0.416429519653320310f, 0.410843133926391600f, 0.405241280794143680f,
0.399624168872833250f, 0.393992006778717040f, 0.388345003128051760f,
0.382683396339416500f, 0.377007365226745610f, 0.371317178010940550f,
0.365612953901290890f, 0.359894990921020510f, 0.354163497686386110f,
0.348418653011322020f, 0.342660695314407350f, 0.336889833211898800f,
0.331106275320053100f, 0.325310260057449340f, 0.319501996040344240f,
0.313681721687316890f, 0.307849615812301640f, 0.302005916833877560f,
0.296150863170623780f, 0.290284633636474610f, 0.284407496452331540f,
0.278519660234451290f, 0.272621333599090580f, 0.266712725162506100f,
0.260794073343276980f, 0.254865616559982300f, 0.248927563428878780f,
0.242980137467384340f, 0.237023577094078060f, 0.231058076024055480f,
0.225083872675895690f, 0.219101205468177800f, 0.213110283017158510f,
0.207111343741416930f, 0.201104596257209780f, 0.195090278983116150f,
0.189068630337715150f, 0.183039844036102290f, 0.177004188299179080f,
0.170961856842041020f, 0.164913088083267210f, 0.158858105540275570f,
0.152797147631645200f, 0.146730437874794010f, 0.140658199787139890f,
0.134580671787261960f, 0.128498077392578130f, 0.122410632669925690f,
0.116318590939044950f, 0.110222168266773220f, 0.104121595621109010f,
0.098017096519470215f, 0.091908916831016541f, 0.085797272622585297f,
0.079682394862174988f, 0.073564521968364716f, 0.067443877458572388f,
0.061320696026086807f, 0.055195201188325882f, 0.049067631363868713f,
0.042938213795423508f, 0.036807179450988770f, 0.030674761161208153f,
0.024541186168789864f, 0.018406687304377556f, 0.012271494604647160f,
0.006135840900242329f
};

float Soloud_fft_trig_8[124] = {
1.000000000000000000f, 0.923879504203796390f, 0.707106769084930420f,
0.382683396339416500f, 1.000000000000000000f, 0.980785250663757320f,
0.923879504203796390f, 0.831469595432281490f, 0.707106769084930420f,
0.555570185184478760f, 0.382683396339416500f, 0.195090278983116150f,
1.000000000000000000f, 0.995184719562530520f, 0.980785250663757320f,
0.956940352916717530f, 0.923879504203796390f, 0.881921231746673580f,
0.831469595432281490f, 0.773010432720184330f, 0.707106769084930420f,
0.634393274784088130f, 0.555570185184478760f, 0.471396714448928830f,
0.382683396339416500f, 0.290284633636474610f, 0.195090278983116150f,
0.098017096519470215f, 1.000000000000000000f, 0.998795449733734130f,
0.995184719562530520f, 0.989176511764526370f, 0.980785250663757320f,
0.970031261444091800f, 0.956940352916717530f, 0.941544055938720700f,
0.923879504203796390f, 0.903989315032958980f, 0.881921231746673580f,
0.857728600502014160f, 0.831469595432281490f, 0.803207516670227050f,
0.773010432720184330f, 0.740951120853424070f, 0.707106769084930420f,
0.671558916568756100f, 0.634393274784088130f, 0.595699310302734380f,
0.555570185184478760f, 0.514102697372436520f, 0.471396714448928830f,
0.427555054426193240f, 0.382683396339416500f, 0.336889833211898800f,
0.290284633636474610f, 0.242980137467384340f, 0.195090278983116150f,
0.146730437874794010f, 0.098017096519470215f, 0.049067631363868713f,
1.000000000000000000f, 0.999698817729949950f, 0.998795449733734130f,
0.997290432453155520f, 0.995184719562530520f, 0.992479562759399410f,
0.989176511764526370f, 0.985277652740478520f, 0.980785250663757320f,
0.975702106952667240f, 0.970031261444091800f, 0.963776051998138430f,
0.956940352916717530f, 0.949528157711029050f, 0.941544055938720700f,
0.932992815971374510f, 0.923879504203796390f, 0.914209723472595210f,
0.903989315032958980f, 0.893224298954010010f, 0.881921231746673580f,
0.870086967945098880f, 0.857728600502014160f, 0.844853579998016360f,
0.831469595432281490f, 0.817584812641143800f, 0.803207516670227050f,
0.788346409797668460f, 0.773010432720184330f, 0.757208824157714840f,
0.740951120853424070f, 0.724247097969055180f, 0.707106769084930420f,
0.689540505409240720f, 0.671558916568756100f, 0.653172850608825680f,
0.634393274784088130f, 0.615231573581695560f, 0.595699310302734380f,
0.575808167457580570f, 0.555570185184478760f, 0.534997582435607910f,
0.514102697372436520f, 0.492898166179656980f, 0.471396714448928830f,
0.449611306190490720f, 0.427555054426193240f, 0.405241280794143680f,
0.382683396339416500f, 0.359894990921020510f, 0.336889833211898800f,
0.313681721687316890f, 0.290284633636474610f, 0.266712725162506100f,
0.242980137467384340f, 0.219101205468177800f, 0.195090278983116150f,
0.170961856842041020f, 0.146730437874794010f, 0.122410632669925690f,
0.098017096519470215f, 0.073564521968364716f, 0.049067631363868713f,
0.024541186168789864f
};

// file: soloud/src/core/soloud_core_faderops.cpp
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

// #include "soloud_internal.h"

// Core operations related to faders (not including filters)

namespace SoLoud
{
	void Soloud::schedulePause(handle aVoiceHandle, time aTime)
	{
		if (aTime <= 0)
		{
			setPause(aVoiceHandle, 1);
			return;
		}
		FOR_ALL_VOICES_PRE
		mVoice[ch]->mPauseScheduler.set(1, 0, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::scheduleStop(handle aVoiceHandle, time aTime)
	{
		if (aTime <= 0)
		{
			stop(aVoiceHandle);
			return;
		}
		FOR_ALL_VOICES_PRE
		mVoice[ch]->mStopScheduler.set(1, 0, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::fadeVolume(handle aVoiceHandle, float aTo, time aTime)
	{
		float from = getVolume(aVoiceHandle);
		if (aTime <= 0 || aTo == from)
		{
			setVolume(aVoiceHandle, aTo);
			return;
		}

		FOR_ALL_VOICES_PRE
		mVoice[ch]->mVolumeFader.set(from, aTo, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::fadePan(handle aVoiceHandle, float aTo, time aTime)
	{
		float from = getPan(aVoiceHandle);
		if (aTime <= 0 || aTo == from)
		{
			setPan(aVoiceHandle, aTo);
			return;
		}

		FOR_ALL_VOICES_PRE
		mVoice[ch]->mPanFader.set(from, aTo, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::fadeRelativePlaySpeed(handle aVoiceHandle, float aTo, time aTime)
	{
		float from = getRelativePlaySpeed(aVoiceHandle);
		if (aTime <= 0 || aTo == from)
		{
			setRelativePlaySpeed(aVoiceHandle, aTo);
			return;
		}
		FOR_ALL_VOICES_PRE
		mVoice[ch]->mRelativePlaySpeedFader.set(from, aTo, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::fadeGlobalVolume(float aTo, time aTime)
	{
		float from = getGlobalVolume();
		if (aTime <= 0 || aTo == from)
		{
			setGlobalVolume(aTo);
			return;
		}
		mGlobalVolumeFader.set(from, aTo, aTime, mStreamTime);
	}


	void Soloud::oscillateVolume(handle aVoiceHandle, float aFrom, float aTo, time aTime)
	{
		if (aTime <= 0 || aTo == aFrom)
		{
			setVolume(aVoiceHandle, aTo);
			return;
		}

		FOR_ALL_VOICES_PRE
		mVoice[ch]->mVolumeFader.setLFO(aFrom, aTo, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::oscillatePan(handle aVoiceHandle, float aFrom, float aTo, time aTime)
	{
		if (aTime <= 0 || aTo == aFrom)
		{
			setPan(aVoiceHandle, aTo);
			return;
		}

		FOR_ALL_VOICES_PRE
		mVoice[ch]->mPanFader.setLFO(aFrom, aTo, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::oscillateRelativePlaySpeed(handle aVoiceHandle, float aFrom, float aTo, time aTime)
	{
		if (aTime <= 0 || aTo == aFrom)
		{
			setRelativePlaySpeed(aVoiceHandle, aTo);
			return;
		}

		FOR_ALL_VOICES_PRE
		mVoice[ch]->mRelativePlaySpeedFader.setLFO(aFrom, aTo, aTime, mVoice[ch]->mStreamTime);
		FOR_ALL_VOICES_POST
	}

	void Soloud::oscillateGlobalVolume(float aFrom, float aTo, time aTime)
	{
		if (aTime <= 0 || aTo == aFrom)
		{
			setGlobalVolume(aTo);
			return;
		}
		mGlobalVolumeFader.setLFO(aFrom, aTo, aTime, mStreamTime);
	}
}
// file: soloud/src/core/soloud_fft.cpp
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

// FFT based on fftreal by Laurent de Soras, under WTFPL

// #include "soloud.h"
// #include "soloud_fft.h"
#include <string.h>

namespace fftimpl
{
// Based on http://www.kurims.kyoto-u.ac.jp/~ooura/fft.html
// "Copyright Takuya OOURA, 1996-2001
//  You may use, copy, modify and distribute this code for any purpose
//  (include commercial use) and without fee. Please refer to this package
//  when you modify this code."


#ifndef M_PI_2
#define M_PI_2      1.570796326794896619231321691639751442098584699687f
#endif
#ifndef WR5000  /* cos(M_PI_2*0.5000) */
#define WR5000      0.707106781186547524400844362104849039284835937688f
#endif
#ifndef WR2500  /* cos(M_PI_2*0.2500) */
#define WR2500      0.923879532511286756128183189396788286822416625863f
#endif
#ifndef WI2500  /* sin(M_PI_2*0.2500) */
#define WI2500      0.382683432365089771728459984030398866761344562485f
#endif
#ifndef WR1250  /* cos(M_PI_2*0.1250) */
#define WR1250      0.980785280403230449126182236134239036973933730893f
#endif
#ifndef WI1250  /* sin(M_PI_2*0.1250) */
#define WI1250      0.195090322016128267848284868477022240927691617751f
#endif
#ifndef WR3750  /* cos(M_PI_2*0.3750) */
#define WR3750      0.831469612302545237078788377617905756738560811987f
#endif
#ifndef WI3750  /* sin(M_PI_2*0.3750) */
#define WI3750      0.555570233019602224742830813948532874374937190754f
#endif

#ifndef CDFT_LOOP_DIV  /* control of the CDFT's speed & tolerance */
#define CDFT_LOOP_DIV 32
#endif

#ifndef RDFT_LOOP_DIV  /* control of the RDFT's speed & tolerance */
#define RDFT_LOOP_DIV 64
#endif

#ifndef DCST_LOOP_DIV  /* control of the DCT,DST's speed & tolerance */
#define DCST_LOOP_DIV 64
#endif

	void bitrv2(int n, float *a)
	{
		int j0, k0, j1, k1, l, m, i, j, k, nh;
		float xr, xi, yr, yi;

		m = 4;
		for (l = n >> 2; l > 8; l >>= 2) {
			m <<= 1;
		}
		nh = n >> 1;
		if (l == 8) {
			j0 = 0;
			for (k0 = 0; k0 < m; k0 += 4) {
				k = k0;
				for (j = j0; j < j0 + k0; j += 4) {
					xr = a[j];
					xi = a[j + 1];
					yr = a[k];
					yi = a[k + 1];
					a[j] = yr;
					a[j + 1] = yi;
					a[k] = xr;
					a[k + 1] = xi;
					j1 = j + m;
					k1 = k + 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 -= m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += nh;
					k1 += 2;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 += m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += 2;
					k1 += nh;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 -= m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= nh;
					k1 -= 2;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 += m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					for (i = nh >> 1; i > (k ^= i); i >>= 1);
				}
				k1 = j0 + k0;
				j1 = k1 + 2;
				k1 += nh;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m;
				k1 += 2 * m;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m;
				k1 -= m;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 -= 2;
				k1 -= nh;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += nh + 2;
				k1 += nh + 2;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 -= nh - m;
				k1 += 2 * m - 2;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
			}
		}
		else {
			j0 = 0;
			for (k0 = 0; k0 < m; k0 += 4) {
				k = k0;
				for (j = j0; j < j0 + k0; j += 4) {
					xr = a[j];
					xi = a[j + 1];
					yr = a[k];
					yi = a[k + 1];
					a[j] = yr;
					a[j + 1] = yi;
					a[k] = xr;
					a[k + 1] = xi;
					j1 = j + m;
					k1 = k + m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += nh;
					k1 += 2;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += 2;
					k1 += nh;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= nh;
					k1 -= 2;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= m;
					xr = a[j1];
					xi = a[j1 + 1];
					yr = a[k1];
					yi = a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					for (i = nh >> 1; i > (k ^= i); i >>= 1);
				}
				k1 = j0 + k0;
				j1 = k1 + 2;
				k1 += nh;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m;
				k1 += m;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
			}
		}
	}


	void bitrv2conj(int n, float *a)
	{
		int j0, k0, j1, k1, l, m, i, j, k, nh;
		float xr, xi, yr, yi;

		m = 4;
		for (l = n >> 2; l > 8; l >>= 2) {
			m <<= 1;
		}
		nh = n >> 1;
		if (l == 8) {
			j0 = 0;
			for (k0 = 0; k0 < m; k0 += 4) {
				k = k0;
				for (j = j0; j < j0 + k0; j += 4) {
					xr = a[j];
					xi = -a[j + 1];
					yr = a[k];
					yi = -a[k + 1];
					a[j] = yr;
					a[j + 1] = yi;
					a[k] = xr;
					a[k + 1] = xi;
					j1 = j + m;
					k1 = k + 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 -= m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += nh;
					k1 += 2;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 += m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += 2;
					k1 += nh;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 -= m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= nh;
					k1 -= 2;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 += m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= 2 * m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					for (i = nh >> 1; i > (k ^= i); i >>= 1);
				}
				k1 = j0 + k0;
				j1 = k1 + 2;
				k1 += nh;
				a[j1 - 1] = -a[j1 - 1];
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				a[k1 + 3] = -a[k1 + 3];
				j1 += m;
				k1 += 2 * m;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m;
				k1 -= m;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 -= 2;
				k1 -= nh;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += nh + 2;
				k1 += nh + 2;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 -= nh - m;
				k1 += 2 * m - 2;
				a[j1 - 1] = -a[j1 - 1];
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				a[k1 + 3] = -a[k1 + 3];
				for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
			}
		}
		else {
			j0 = 0;
			for (k0 = 0; k0 < m; k0 += 4) {
				k = k0;
				for (j = j0; j < j0 + k0; j += 4) {
					xr = a[j];
					xi = -a[j + 1];
					yr = a[k];
					yi = -a[k + 1];
					a[j] = yr;
					a[j + 1] = yi;
					a[k] = xr;
					a[k + 1] = xi;
					j1 = j + m;
					k1 = k + m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += nh;
					k1 += 2;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += 2;
					k1 += nh;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 += m;
					k1 += m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= nh;
					k1 -= 2;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					j1 -= m;
					k1 -= m;
					xr = a[j1];
					xi = -a[j1 + 1];
					yr = a[k1];
					yi = -a[k1 + 1];
					a[j1] = yr;
					a[j1 + 1] = yi;
					a[k1] = xr;
					a[k1 + 1] = xi;
					for (i = nh >> 1; i > (k ^= i); i >>= 1);
				}
				k1 = j0 + k0;
				j1 = k1 + 2;
				k1 += nh;
				a[j1 - 1] = -a[j1 - 1];
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				a[k1 + 3] = -a[k1 + 3];
				j1 += m;
				k1 += m;
				a[j1 - 1] = -a[j1 - 1];
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				a[k1 + 3] = -a[k1 + 3];
				for (i = nh >> 1; i > (j0 ^= i); i >>= 1);
			}
		}
	}


	void bitrv216(float *a)
	{
		float x1r, x1i, x2r, x2i, x3r, x3i, x4r, x4i,
			x5r, x5i, x7r, x7i, x8r, x8i, x10r, x10i,
			x11r, x11i, x12r, x12i, x13r, x13i, x14r, x14i;

		x1r = a[2];
		x1i = a[3];
		x2r = a[4];
		x2i = a[5];
		x3r = a[6];
		x3i = a[7];
		x4r = a[8];
		x4i = a[9];
		x5r = a[10];
		x5i = a[11];
		x7r = a[14];
		x7i = a[15];
		x8r = a[16];
		x8i = a[17];
		x10r = a[20];
		x10i = a[21];
		x11r = a[22];
		x11i = a[23];
		x12r = a[24];
		x12i = a[25];
		x13r = a[26];
		x13i = a[27];
		x14r = a[28];
		x14i = a[29];
		a[2] = x8r;
		a[3] = x8i;
		a[4] = x4r;
		a[5] = x4i;
		a[6] = x12r;
		a[7] = x12i;
		a[8] = x2r;
		a[9] = x2i;
		a[10] = x10r;
		a[11] = x10i;
		a[14] = x14r;
		a[15] = x14i;
		a[16] = x1r;
		a[17] = x1i;
		a[20] = x5r;
		a[21] = x5i;
		a[22] = x13r;
		a[23] = x13i;
		a[24] = x3r;
		a[25] = x3i;
		a[26] = x11r;
		a[27] = x11i;
		a[28] = x7r;
		a[29] = x7i;
	}


	void bitrv216neg(float *a)
	{
		float x1r, x1i, x2r, x2i, x3r, x3i, x4r, x4i,
			x5r, x5i, x6r, x6i, x7r, x7i, x8r, x8i,
			x9r, x9i, x10r, x10i, x11r, x11i, x12r, x12i,
			x13r, x13i, x14r, x14i, x15r, x15i;

		x1r = a[2];
		x1i = a[3];
		x2r = a[4];
		x2i = a[5];
		x3r = a[6];
		x3i = a[7];
		x4r = a[8];
		x4i = a[9];
		x5r = a[10];
		x5i = a[11];
		x6r = a[12];
		x6i = a[13];
		x7r = a[14];
		x7i = a[15];
		x8r = a[16];
		x8i = a[17];
		x9r = a[18];
		x9i = a[19];
		x10r = a[20];
		x10i = a[21];
		x11r = a[22];
		x11i = a[23];
		x12r = a[24];
		x12i = a[25];
		x13r = a[26];
		x13i = a[27];
		x14r = a[28];
		x14i = a[29];
		x15r = a[30];
		x15i = a[31];
		a[2] = x15r;
		a[3] = x15i;
		a[4] = x7r;
		a[5] = x7i;
		a[6] = x11r;
		a[7] = x11i;
		a[8] = x3r;
		a[9] = x3i;
		a[10] = x13r;
		a[11] = x13i;
		a[12] = x5r;
		a[13] = x5i;
		a[14] = x9r;
		a[15] = x9i;
		a[16] = x1r;
		a[17] = x1i;
		a[18] = x14r;
		a[19] = x14i;
		a[20] = x6r;
		a[21] = x6i;
		a[22] = x10r;
		a[23] = x10i;
		a[24] = x2r;
		a[25] = x2i;
		a[26] = x12r;
		a[27] = x12i;
		a[28] = x4r;
		a[29] = x4i;
		a[30] = x8r;
		a[31] = x8i;
	}


	void bitrv208(float *a)
	{
		float x1r, x1i, x3r, x3i, x4r, x4i, x6r, x6i;

		x1r = a[2];
		x1i = a[3];
		x3r = a[6];
		x3i = a[7];
		x4r = a[8];
		x4i = a[9];
		x6r = a[12];
		x6i = a[13];
		a[2] = x4r;
		a[3] = x4i;
		a[6] = x6r;
		a[7] = x6i;
		a[8] = x1r;
		a[9] = x1i;
		a[12] = x3r;
		a[13] = x3i;
	}


	void bitrv208neg(float *a)
	{
		float x1r, x1i, x2r, x2i, x3r, x3i, x4r, x4i,
			x5r, x5i, x6r, x6i, x7r, x7i;

		x1r = a[2];
		x1i = a[3];
		x2r = a[4];
		x2i = a[5];
		x3r = a[6];
		x3i = a[7];
		x4r = a[8];
		x4i = a[9];
		x5r = a[10];
		x5i = a[11];
		x6r = a[12];
		x6i = a[13];
		x7r = a[14];
		x7i = a[15];
		a[2] = x7r;
		a[3] = x7i;
		a[4] = x3r;
		a[5] = x3i;
		a[6] = x5r;
		a[7] = x5i;
		a[8] = x1r;
		a[9] = x1i;
		a[10] = x6r;
		a[11] = x6i;
		a[12] = x2r;
		a[13] = x2i;
		a[14] = x4r;
		a[15] = x4i;
	}

	void cftb1st(int n, float *a)
	{
		int i, i0, j, j0, j1, j2, j3, m, mh;
		float ew, w1r, w1i, wk1r, wk1i, wk3r, wk3i,
			wd1r, wd1i, wd3r, wd3i, ss1, ss3;
		float x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

		mh = n >> 3;
		m = 2 * mh;
		j1 = m;
		j2 = j1 + m;
		j3 = j2 + m;
		x0r = a[0] + a[j2];
		x0i = -a[1] - a[j2 + 1];
		x1r = a[0] - a[j2];
		x1i = -a[1] + a[j2 + 1];
		x2r = a[j1] + a[j3];
		x2i = a[j1 + 1] + a[j3 + 1];
		x3r = a[j1] - a[j3];
		x3i = a[j1 + 1] - a[j3 + 1];
		a[0] = x0r + x2r;
		a[1] = x0i - x2i;
		a[j1] = x0r - x2r;
		a[j1 + 1] = x0i + x2i;
		a[j2] = x1r + x3i;
		a[j2 + 1] = x1i + x3r;
		a[j3] = x1r - x3i;
		a[j3 + 1] = x1i - x3r;
		wd1r = 1;
		wd1i = 0;
		wd3r = 1;
		wd3i = 0;
		ew = M_PI_2 / m;
		w1r = (float)cos(2 * ew);
		w1i = (float)sin(2 * ew);
		wk1r = w1r;
		wk1i = w1i;
		ss1 = 2 * w1i;
		wk3i = 2 * ss1 * wk1r;
		wk3r = wk1r - wk3i * wk1i;
		wk3i = wk1i - wk3i * wk1r;
		ss3 = 2 * wk3i;
		i = 0;
		for (;;) {
			i0 = i + 4 * CDFT_LOOP_DIV;
			if (i0 > mh - 4) {
				i0 = mh - 4;
			}
			for (j = i + 2; j < i0; j += 4) {
				wd1r -= ss1 * wk1i;
				wd1i += ss1 * wk1r;
				wd3r -= ss3 * wk3i;
				wd3i += ss3 * wk3r;
				j1 = j + m;
				j2 = j1 + m;
				j3 = j2 + m;
				x0r = a[j] + a[j2];
				x0i = -a[j + 1] - a[j2 + 1];
				x1r = a[j] - a[j2];
				x1i = -a[j + 1] + a[j2 + 1];
				x2r = a[j1] + a[j3];
				x2i = a[j1 + 1] + a[j3 + 1];
				x3r = a[j1] - a[j3];
				x3i = a[j1 + 1] - a[j3 + 1];
				a[j] = x0r + x2r;
				a[j + 1] = x0i - x2i;
				a[j1] = x0r - x2r;
				a[j1 + 1] = x0i + x2i;
				x0r = x1r + x3i;
				x0i = x1i + x3r;
				a[j2] = wk1r * x0r - wk1i * x0i;
				a[j2 + 1] = wk1r * x0i + wk1i * x0r;
				x0r = x1r - x3i;
				x0i = x1i - x3r;
				a[j3] = wk3r * x0r + wk3i * x0i;
				a[j3 + 1] = wk3r * x0i - wk3i * x0r;
				x0r = a[j + 2] + a[j2 + 2];
				x0i = -a[j + 3] - a[j2 + 3];
				x1r = a[j + 2] - a[j2 + 2];
				x1i = -a[j + 3] + a[j2 + 3];
				x2r = a[j1 + 2] + a[j3 + 2];
				x2i = a[j1 + 3] + a[j3 + 3];
				x3r = a[j1 + 2] - a[j3 + 2];
				x3i = a[j1 + 3] - a[j3 + 3];
				a[j + 2] = x0r + x2r;
				a[j + 3] = x0i - x2i;
				a[j1 + 2] = x0r - x2r;
				a[j1 + 3] = x0i + x2i;
				x0r = x1r + x3i;
				x0i = x1i + x3r;
				a[j2 + 2] = wd1r * x0r - wd1i * x0i;
				a[j2 + 3] = wd1r * x0i + wd1i * x0r;
				x0r = x1r - x3i;
				x0i = x1i - x3r;
				a[j3 + 2] = wd3r * x0r + wd3i * x0i;
				a[j3 + 3] = wd3r * x0i - wd3i * x0r;
				j0 = m - j;
				j1 = j0 + m;
				j2 = j1 + m;
				j3 = j2 + m;
				x0r = a[j0] + a[j2];
				x0i = -a[j0 + 1] - a[j2 + 1];
				x1r = a[j0] - a[j2];
				x1i = -a[j0 + 1] + a[j2 + 1];
				x2r = a[j1] + a[j3];
				x2i = a[j1 + 1] + a[j3 + 1];
				x3r = a[j1] - a[j3];
				x3i = a[j1 + 1] - a[j3 + 1];
				a[j0] = x0r + x2r;
				a[j0 + 1] = x0i - x2i;
				a[j1] = x0r - x2r;
				a[j1 + 1] = x0i + x2i;
				x0r = x1r + x3i;
				x0i = x1i + x3r;
				a[j2] = wk1i * x0r - wk1r * x0i;
				a[j2 + 1] = wk1i * x0i + wk1r * x0r;
				x0r = x1r - x3i;
				x0i = x1i - x3r;
				a[j3] = wk3i * x0r + wk3r * x0i;
				a[j3 + 1] = wk3i * x0i - wk3r * x0r;
				x0r = a[j0 - 2] + a[j2 - 2];
				x0i = -a[j0 - 1] - a[j2 - 1];
				x1r = a[j0 - 2] - a[j2 - 2];
				x1i = -a[j0 - 1] + a[j2 - 1];
				x2r = a[j1 - 2] + a[j3 - 2];
				x2i = a[j1 - 1] + a[j3 - 1];
				x3r = a[j1 - 2] - a[j3 - 2];
				x3i = a[j1 - 1] - a[j3 - 1];
				a[j0 - 2] = x0r + x2r;
				a[j0 - 1] = x0i - x2i;
				a[j1 - 2] = x0r - x2r;
				a[j1 - 1] = x0i + x2i;
				x0r = x1r + x3i;
				x0i = x1i + x3r;
				a[j2 - 2] = wd1i * x0r - wd1r * x0i;
				a[j2 - 1] = wd1i * x0i + wd1r * x0r;
				x0r = x1r - x3i;
				x0i = x1i - x3r;
				a[j3 - 2] = wd3i * x0r + wd3r * x0i;
				a[j3 - 1] = wd3i * x0i - wd3r * x0r;
				wk1r -= ss1 * wd1i;
				wk1i += ss1 * wd1r;
				wk3r -= ss3 * wd3i;
				wk3i += ss3 * wd3r;
			}
			if (i0 == mh - 4) {
				break;
			}
			wd1r = (float)cos(ew * i0);
			wd1i = (float)sin(ew * i0);
			wd3i = 4 * wd1i * wd1r;
			wd3r = wd1r - wd3i * wd1i;
			wd3i = wd1i - wd3i * wd1r;
			wk1r = w1r * wd1r - w1i * wd1i;
			wk1i = w1r * wd1i + w1i * wd1r;
			wk3i = 4 * wk1i * wk1r;
			wk3r = wk1r - wk3i * wk1i;
			wk3i = wk1i - wk3i * wk1r;
			i = i0;
		}
		wd1r = WR5000;
		j0 = mh;
		j1 = j0 + m;
		j2 = j1 + m;
		j3 = j2 + m;
		x0r = a[j0 - 2] + a[j2 - 2];
		x0i = -a[j0 - 1] - a[j2 - 1];
		x1r = a[j0 - 2] - a[j2 - 2];
		x1i = -a[j0 - 1] + a[j2 - 1];
		x2r = a[j1 - 2] + a[j3 - 2];
		x2i = a[j1 - 1] + a[j3 - 1];
		x3r = a[j1 - 2] - a[j3 - 2];
		x3i = a[j1 - 1] - a[j3 - 1];
		a[j0 - 2] = x0r + x2r;
		a[j0 - 1] = x0i - x2i;
		a[j1 - 2] = x0r - x2r;
		a[j1 - 1] = x0i + x2i;
		x0r = x1r + x3i;
		x0i = x1i + x3r;
		a[j2 - 2] = wk1r * x0r - wk1i * x0i;
		a[j2 - 1] = wk1r * x0i + wk1i * x0r;
		x0r = x1r - x3i;
		x0i = x1i - x3r;
		a[j3 - 2] = wk3r * x0r + wk3i * x0i;
		a[j3 - 1] = wk3r * x0i - wk3i * x0r;
		x0r = a[j0] + a[j2];
		x0i = -a[j0 + 1] - a[j2 + 1];
		x1r = a[j0] - a[j2];
		x1i = -a[j0 + 1] + a[j2 + 1];
		x2r = a[j1] + a[j3];
		x2i = a[j1 + 1] + a[j3 + 1];
		x3r = a[j1] - a[j3];
		x3i = a[j1 + 1] - a[j3 + 1];
		a[j0] = x0r + x2r;
		a[j0 + 1] = x0i - x2i;
		a[j1] = x0r - x2r;
		a[j1 + 1] = x0i + x2i;
		x0r = x1r + x3i;
		x0i = x1i + x3r;
		a[j2] = wd1r * (x0r - x0i);
		a[j2 + 1] = wd1r * (x0i + x0r);
		x0r = x1r - x3i;
		x0i = x1i - x3r;
		a[j3] = -wd1r * (x0r + x0i);
		a[j3 + 1] = -wd1r * (x0i - x0r);
		x0r = a[j0 + 2] + a[j2 + 2];
		x0i = -a[j0 + 3] - a[j2 + 3];
		x1r = a[j0 + 2] - a[j2 + 2];
		x1i = -a[j0 + 3] + a[j2 + 3];
		x2r = a[j1 + 2] + a[j3 + 2];
		x2i = a[j1 + 3] + a[j3 + 3];
		x3r = a[j1 + 2] - a[j3 + 2];
		x3i = a[j1 + 3] - a[j3 + 3];
		a[j0 + 2] = x0r + x2r;
		a[j0 + 3] = x0i - x2i;
		a[j1 + 2] = x0r - x2r;
		a[j1 + 3] = x0i + x2i;
		x0r = x1r + x3i;
		x0i = x1i + x3r;
		a[j2 + 2] = wk1i * x0r - wk1r * x0i;
		a[j2 + 3] = wk1i * x0i + wk1r * x0r;
		x0r = x1r - x3i;
		x0i = x1i - x3r;
		a[j3 + 2] = wk3i * x0r + wk3r * x0i;
		a[j3 + 3] = wk3i * x0i - wk3r * x0r;
	}

	void cftmdl1(int n, float *a)
	{
		int i, i0, j, j0, j1, j2, j3, m, mh;
		float ew, w1r, w1i, wk1r, wk1i, wk3r, wk3i,
			wd1r, wd1i, wd3r, wd3i, ss1, ss3;
		float x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

		mh = n >> 3;
		m = 2 * mh;
		j1 = m;
		j2 = j1 + m;
		j3 = j2 + m;
		x0r = a[0] + a[j2];
		x0i = a[1] + a[j2 + 1];
		x1r = a[0] - a[j2];
		x1i = a[1] - a[j2 + 1];
		x2r = a[j1] + a[j3];
		x2i = a[j1 + 1] + a[j3 + 1];
		x3r = a[j1] - a[j3];
		x3i = a[j1 + 1] - a[j3 + 1];
		a[0] = x0r + x2r;
		a[1] = x0i + x2i;
		a[j1] = x0r - x2r;
		a[j1 + 1] = x0i - x2i;
		a[j2] = x1r - x3i;
		a[j2 + 1] = x1i + x3r;
		a[j3] = x1r + x3i;
		a[j3 + 1] = x1i - x3r;
		wd1r = 1;
		wd1i = 0;
		wd3r = 1;
		wd3i = 0;
		ew = M_PI_2 / m;
		w1r = (float)cos(2 * ew);
		w1i = (float)sin(2 * ew);
		wk1r = w1r;
		wk1i = w1i;
		ss1 = 2 * w1i;
		wk3i = 2 * ss1 * wk1r;
		wk3r = wk1r - wk3i * wk1i;
		wk3i = wk1i - wk3i * wk1r;
		ss3 = 2 * wk3i;
		i = 0;
		for (;;) {
			i0 = i + 4 * CDFT_LOOP_DIV;
			if (i0 > mh - 4) {
				i0 = mh - 4;
			}
			for (j = i + 2; j < i0; j += 4) {
				wd1r -= ss1 * wk1i;
				wd1i += ss1 * wk1r;
				wd3r -= ss3 * wk3i;
				wd3i += ss3 * wk3r;
				j1 = j + m;
				j2 = j1 + m;
				j3 = j2 + m;
				x0r = a[j] + a[j2];
				x0i = a[j + 1] + a[j2 + 1];
				x1r = a[j] - a[j2];
				x1i = a[j + 1] - a[j2 + 1];
				x2r = a[j1] + a[j3];
				x2i = a[j1 + 1] + a[j3 + 1];
				x3r = a[j1] - a[j3];
				x3i = a[j1 + 1] - a[j3 + 1];
				a[j] = x0r + x2r;
				a[j + 1] = x0i + x2i;
				a[j1] = x0r - x2r;
				a[j1 + 1] = x0i - x2i;
				x0r = x1r - x3i;
				x0i = x1i + x3r;
				a[j2] = wk1r * x0r - wk1i * x0i;
				a[j2 + 1] = wk1r * x0i + wk1i * x0r;
				x0r = x1r + x3i;
				x0i = x1i - x3r;
				a[j3] = wk3r * x0r + wk3i * x0i;
				a[j3 + 1] = wk3r * x0i - wk3i * x0r;
				x0r = a[j + 2] + a[j2 + 2];
				x0i = a[j + 3] + a[j2 + 3];
				x1r = a[j + 2] - a[j2 + 2];
				x1i = a[j + 3] - a[j2 + 3];
				x2r = a[j1 + 2] + a[j3 + 2];
				x2i = a[j1 + 3] + a[j3 + 3];
				x3r = a[j1 + 2] - a[j3 + 2];
				x3i = a[j1 + 3] - a[j3 + 3];
				a[j + 2] = x0r + x2r;
				a[j + 3] = x0i + x2i;
				a[j1 + 2] = x0r - x2r;
				a[j1 + 3] = x0i - x2i;
				x0r = x1r - x3i;
				x0i = x1i + x3r;
				a[j2 + 2] = wd1r * x0r - wd1i * x0i;
				a[j2 + 3] = wd1r * x0i + wd1i * x0r;
				x0r = x1r + x3i;
				x0i = x1i - x3r;
				a[j3 + 2] = wd3r * x0r + wd3i * x0i;
				a[j3 + 3] = wd3r * x0i - wd3i * x0r;
				j0 = m - j;
				j1 = j0 + m;
				j2 = j1 + m;
				j3 = j2 + m;
				x0r = a[j0] + a[j2];
				x0i = a[j0 + 1] + a[j2 + 1];
				x1r = a[j0] - a[j2];
				x1i = a[j0 + 1] - a[j2 + 1];
				x2r = a[j1] + a[j3];
				x2i = a[j1 + 1] + a[j3 + 1];
				x3r = a[j1] - a[j3];
				x3i = a[j1 + 1] - a[j3 + 1];
				a[j0] = x0r + x2r;
				a[j0 + 1] = x0i + x2i;
				a[j1] = x0r - x2r;
				a[j1 + 1] = x0i - x2i;
				x0r = x1r - x3i;
				x0i = x1i + x3r;
				a[j2] = wk1i * x0r - wk1r * x0i;
				a[j2 + 1] = wk1i * x0i + wk1r * x0r;
				x0r = x1r + x3i;
				x0i = x1i - x3r;
				a[j3] = wk3i * x0r + wk3r * x0i;
				a[j3 + 1] = wk3i * x0i - wk3r * x0r;
				x0r = a[j0 - 2] + a[j2 - 2];
				x0i = a[j0 - 1] + a[j2 - 1];
				x1r = a[j0 - 2] - a[j2 - 2];
				x1i = a[j0 - 1] - a[j2 - 1];
				x2r = a[j1 - 2] + a[j3 - 2];
				x2i = a[j1 - 1] + a[j3 - 1];
				x3r = a[j1 - 2] - a[j3 - 2];
				x3i = a[j1 - 1] - a[j3 - 1];
				a[j0 - 2] = x0r + x2r;
				a[j0 - 1] = x0i + x2i;
				a[j1 - 2] = x0r - x2r;
				a[j1 - 1] = x0i - x2i;
				x0r = x1r - x3i;
				x0i = x1i + x3r;
				a[j2 - 2] = wd1i * x0r - wd1r * x0i;
				a[j2 - 1] = wd1i * x0i + wd1r * x0r;
				x0r = x1r + x3i;
				x0i = x1i - x3r;
				a[j3 - 2] = wd3i * x0r + wd3r * x0i;
				a[j3 - 1] = wd3i * x0i - wd3r * x0r;
				wk1r -= ss1 * wd1i;
				wk1i += ss1 * wd1r;
				wk3r -= ss3 * wd3i;
				wk3i += ss3 * wd3r;
			}
			if (i0 == mh - 4) {
				break;
			}
			wd1r = (float)cos(ew * i0);
			wd1i = (float)sin(ew * i0);
			wd3i = 4 * wd1i * wd1r;
			wd3r = wd1r - wd3i * wd1i;
			wd3i = wd1i - wd3i * wd1r;
			wk1r = w1r * wd1r - w1i * wd1i;
			wk1i = w1r * wd1i + w1i * wd1r;
			wk3i = 4 * wk1i * wk1r;
			wk3r = wk1r - wk3i * wk1i;
			wk3i = wk1i - wk3i * wk1r;
			i = i0;
		}
		wd1r = WR5000;
		j0 = mh;
		j1 = j0 + m;
		j2 = j1 + m;
		j3 = j2 + m;
		x0r = a[j0 - 2] + a[j2 - 2];
		x0i = a[j0 - 1] + a[j2 - 1];
		x1r = a[j0 - 2] - a[j2 - 2];
		x1i = a[j0 - 1] - a[j2 - 1];
		x2r = a[j1 - 2] + a[j3 - 2];
		x2i = a[j1 - 1] + a[j3 - 1];
		x3r = a[j1 - 2] - a[j3 - 2];
		x3i = a[j1 - 1] - a[j3 - 1];
		a[j0 - 2] = x0r + x2r;
		a[j0 - 1] = x0i + x2i;
		a[j1 - 2] = x0r - x2r;
		a[j1 - 1] = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		a[j2 - 2] = wk1r * x0r - wk1i * x0i;
		a[j2 - 1] = wk1r * x0i + wk1i * x0r;
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		a[j3 - 2] = wk3r * x0r + wk3i * x0i;
		a[j3 - 1] = wk3r * x0i - wk3i * x0r;
		x0r = a[j0] + a[j2];
		x0i = a[j0 + 1] + a[j2 + 1];
		x1r = a[j0] - a[j2];
		x1i = a[j0 + 1] - a[j2 + 1];
		x2r = a[j1] + a[j3];
		x2i = a[j1 + 1] + a[j3 + 1];
		x3r = a[j1] - a[j3];
		x3i = a[j1 + 1] - a[j3 + 1];
		a[j0] = x0r + x2r;
		a[j0 + 1] = x0i + x2i;
		a[j1] = x0r - x2r;
		a[j1 + 1] = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		a[j2] = wd1r * (x0r - x0i);
		a[j2 + 1] = wd1r * (x0i + x0r);
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		a[j3] = -wd1r * (x0r + x0i);
		a[j3 + 1] = -wd1r * (x0i - x0r);
		x0r = a[j0 + 2] + a[j2 + 2];
		x0i = a[j0 + 3] + a[j2 + 3];
		x1r = a[j0 + 2] - a[j2 + 2];
		x1i = a[j0 + 3] - a[j2 + 3];
		x2r = a[j1 + 2] + a[j3 + 2];
		x2i = a[j1 + 3] + a[j3 + 3];
		x3r = a[j1 + 2] - a[j3 + 2];
		x3i = a[j1 + 3] - a[j3 + 3];
		a[j0 + 2] = x0r + x2r;
		a[j0 + 3] = x0i + x2i;
		a[j1 + 2] = x0r - x2r;
		a[j1 + 3] = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		a[j2 + 2] = wk1i * x0r - wk1r * x0i;
		a[j2 + 3] = wk1i * x0i + wk1r * x0r;
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		a[j3 + 2] = wk3i * x0r + wk3r * x0i;
		a[j3 + 3] = wk3i * x0i - wk3r * x0r;
	}


	void cftmdl2(int n, float *a)
	{
		int i, i0, j, j0, j1, j2, j3, m, mh;
		float ew, w1r, w1i, wn4r, wk1r, wk1i, wk3r, wk3i,
			wl1r, wl1i, wl3r, wl3i, wd1r, wd1i, wd3r, wd3i,
			we1r, we1i, we3r, we3i, ss1, ss3;
		float x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i, y0r, y0i, y2r, y2i;

		mh = n >> 3;
		m = 2 * mh;
		wn4r = WR5000;
		j1 = m;
		j2 = j1 + m;
		j3 = j2 + m;
		x0r = a[0] - a[j2 + 1];
		x0i = a[1] + a[j2];
		x1r = a[0] + a[j2 + 1];
		x1i = a[1] - a[j2];
		x2r = a[j1] - a[j3 + 1];
		x2i = a[j1 + 1] + a[j3];
		x3r = a[j1] + a[j3 + 1];
		x3i = a[j1 + 1] - a[j3];
		y0r = wn4r * (x2r - x2i);
		y0i = wn4r * (x2i + x2r);
		a[0] = x0r + y0r;
		a[1] = x0i + y0i;
		a[j1] = x0r - y0r;
		a[j1 + 1] = x0i - y0i;
		y0r = wn4r * (x3r - x3i);
		y0i = wn4r * (x3i + x3r);
		a[j2] = x1r - y0i;
		a[j2 + 1] = x1i + y0r;
		a[j3] = x1r + y0i;
		a[j3 + 1] = x1i - y0r;
		wl1r = 1;
		wl1i = 0;
		wl3r = 1;
		wl3i = 0;
		we1r = wn4r;
		we1i = wn4r;
		we3r = -wn4r;
		we3i = -wn4r;
		ew = M_PI_2 / (2 * m);
		w1r = (float)cos(2 * ew);
		w1i = (float)sin(2 * ew);
		wk1r = w1r;
		wk1i = w1i;
		wd1r = wn4r * (w1r - w1i);
		wd1i = wn4r * (w1i + w1r);
		ss1 = 2 * w1i;
		wk3i = 2 * ss1 * wk1r;
		wk3r = wk1r - wk3i * wk1i;
		wk3i = wk1i - wk3i * wk1r;
		ss3 = 2 * wk3i;
		wd3r = -wn4r * (wk3r - wk3i);
		wd3i = -wn4r * (wk3i + wk3r);
		i = 0;
		for (;;) {
			i0 = i + 4 * CDFT_LOOP_DIV;
			if (i0 > mh - 4) {
				i0 = mh - 4;
			}
			for (j = i + 2; j < i0; j += 4) {
				wl1r -= ss1 * wk1i;
				wl1i += ss1 * wk1r;
				wl3r -= ss3 * wk3i;
				wl3i += ss3 * wk3r;
				we1r -= ss1 * wd1i;
				we1i += ss1 * wd1r;
				we3r -= ss3 * wd3i;
				we3i += ss3 * wd3r;
				j1 = j + m;
				j2 = j1 + m;
				j3 = j2 + m;
				x0r = a[j] - a[j2 + 1];
				x0i = a[j + 1] + a[j2];
				x1r = a[j] + a[j2 + 1];
				x1i = a[j + 1] - a[j2];
				x2r = a[j1] - a[j3 + 1];
				x2i = a[j1 + 1] + a[j3];
				x3r = a[j1] + a[j3 + 1];
				x3i = a[j1 + 1] - a[j3];
				y0r = wk1r * x0r - wk1i * x0i;
				y0i = wk1r * x0i + wk1i * x0r;
				y2r = wd1r * x2r - wd1i * x2i;
				y2i = wd1r * x2i + wd1i * x2r;
				a[j] = y0r + y2r;
				a[j + 1] = y0i + y2i;
				a[j1] = y0r - y2r;
				a[j1 + 1] = y0i - y2i;
				y0r = wk3r * x1r + wk3i * x1i;
				y0i = wk3r * x1i - wk3i * x1r;
				y2r = wd3r * x3r + wd3i * x3i;
				y2i = wd3r * x3i - wd3i * x3r;
				a[j2] = y0r + y2r;
				a[j2 + 1] = y0i + y2i;
				a[j3] = y0r - y2r;
				a[j3 + 1] = y0i - y2i;
				x0r = a[j + 2] - a[j2 + 3];
				x0i = a[j + 3] + a[j2 + 2];
				x1r = a[j + 2] + a[j2 + 3];
				x1i = a[j + 3] - a[j2 + 2];
				x2r = a[j1 + 2] - a[j3 + 3];
				x2i = a[j1 + 3] + a[j3 + 2];
				x3r = a[j1 + 2] + a[j3 + 3];
				x3i = a[j1 + 3] - a[j3 + 2];
				y0r = wl1r * x0r - wl1i * x0i;
				y0i = wl1r * x0i + wl1i * x0r;
				y2r = we1r * x2r - we1i * x2i;
				y2i = we1r * x2i + we1i * x2r;
				a[j + 2] = y0r + y2r;
				a[j + 3] = y0i + y2i;
				a[j1 + 2] = y0r - y2r;
				a[j1 + 3] = y0i - y2i;
				y0r = wl3r * x1r + wl3i * x1i;
				y0i = wl3r * x1i - wl3i * x1r;
				y2r = we3r * x3r + we3i * x3i;
				y2i = we3r * x3i - we3i * x3r;
				a[j2 + 2] = y0r + y2r;
				a[j2 + 3] = y0i + y2i;
				a[j3 + 2] = y0r - y2r;
				a[j3 + 3] = y0i - y2i;
				j0 = m - j;
				j1 = j0 + m;
				j2 = j1 + m;
				j3 = j2 + m;
				x0r = a[j0] - a[j2 + 1];
				x0i = a[j0 + 1] + a[j2];
				x1r = a[j0] + a[j2 + 1];
				x1i = a[j0 + 1] - a[j2];
				x2r = a[j1] - a[j3 + 1];
				x2i = a[j1 + 1] + a[j3];
				x3r = a[j1] + a[j3 + 1];
				x3i = a[j1 + 1] - a[j3];
				y0r = wd1i * x0r - wd1r * x0i;
				y0i = wd1i * x0i + wd1r * x0r;
				y2r = wk1i * x2r - wk1r * x2i;
				y2i = wk1i * x2i + wk1r * x2r;
				a[j0] = y0r + y2r;
				a[j0 + 1] = y0i + y2i;
				a[j1] = y0r - y2r;
				a[j1 + 1] = y0i - y2i;
				y0r = wd3i * x1r + wd3r * x1i;
				y0i = wd3i * x1i - wd3r * x1r;
				y2r = wk3i * x3r + wk3r * x3i;
				y2i = wk3i * x3i - wk3r * x3r;
				a[j2] = y0r + y2r;
				a[j2 + 1] = y0i + y2i;
				a[j3] = y0r - y2r;
				a[j3 + 1] = y0i - y2i;
				x0r = a[j0 - 2] - a[j2 - 1];
				x0i = a[j0 - 1] + a[j2 - 2];
				x1r = a[j0 - 2] + a[j2 - 1];
				x1i = a[j0 - 1] - a[j2 - 2];
				x2r = a[j1 - 2] - a[j3 - 1];
				x2i = a[j1 - 1] + a[j3 - 2];
				x3r = a[j1 - 2] + a[j3 - 1];
				x3i = a[j1 - 1] - a[j3 - 2];
				y0r = we1i * x0r - we1r * x0i;
				y0i = we1i * x0i + we1r * x0r;
				y2r = wl1i * x2r - wl1r * x2i;
				y2i = wl1i * x2i + wl1r * x2r;
				a[j0 - 2] = y0r + y2r;
				a[j0 - 1] = y0i + y2i;
				a[j1 - 2] = y0r - y2r;
				a[j1 - 1] = y0i - y2i;
				y0r = we3i * x1r + we3r * x1i;
				y0i = we3i * x1i - we3r * x1r;
				y2r = wl3i * x3r + wl3r * x3i;
				y2i = wl3i * x3i - wl3r * x3r;
				a[j2 - 2] = y0r + y2r;
				a[j2 - 1] = y0i + y2i;
				a[j3 - 2] = y0r - y2r;
				a[j3 - 1] = y0i - y2i;
				wk1r -= ss1 * wl1i;
				wk1i += ss1 * wl1r;
				wk3r -= ss3 * wl3i;
				wk3i += ss3 * wl3r;
				wd1r -= ss1 * we1i;
				wd1i += ss1 * we1r;
				wd3r -= ss3 * we3i;
				wd3i += ss3 * we3r;
			}
			if (i0 == mh - 4) {
				break;
			}
			wl1r = (float)cos(ew * i0);
			wl1i = (float)sin(ew * i0);
			wl3i = 4 * wl1i * wl1r;
			wl3r = wl1r - wl3i * wl1i;
			wl3i = wl1i - wl3i * wl1r;
			we1r = wn4r * (wl1r - wl1i);
			we1i = wn4r * (wl1i + wl1r);
			we3r = -wn4r * (wl3r - wl3i);
			we3i = -wn4r * (wl3i + wl3r);
			wk1r = w1r * wl1r - w1i * wl1i;
			wk1i = w1r * wl1i + w1i * wl1r;
			wk3i = 4 * wk1i * wk1r;
			wk3r = wk1r - wk3i * wk1i;
			wk3i = wk1i - wk3i * wk1r;
			wd1r = wn4r * (wk1r - wk1i);
			wd1i = wn4r * (wk1i + wk1r);
			wd3r = -wn4r * (wk3r - wk3i);
			wd3i = -wn4r * (wk3i + wk3r);
			i = i0;
		}
		wl1r = WR2500;
		wl1i = WI2500;
		j0 = mh;
		j1 = j0 + m;
		j2 = j1 + m;
		j3 = j2 + m;
		x0r = a[j0 - 2] - a[j2 - 1];
		x0i = a[j0 - 1] + a[j2 - 2];
		x1r = a[j0 - 2] + a[j2 - 1];
		x1i = a[j0 - 1] - a[j2 - 2];
		x2r = a[j1 - 2] - a[j3 - 1];
		x2i = a[j1 - 1] + a[j3 - 2];
		x3r = a[j1 - 2] + a[j3 - 1];
		x3i = a[j1 - 1] - a[j3 - 2];
		y0r = wk1r * x0r - wk1i * x0i;
		y0i = wk1r * x0i + wk1i * x0r;
		y2r = wd1r * x2r - wd1i * x2i;
		y2i = wd1r * x2i + wd1i * x2r;
		a[j0 - 2] = y0r + y2r;
		a[j0 - 1] = y0i + y2i;
		a[j1 - 2] = y0r - y2r;
		a[j1 - 1] = y0i - y2i;
		y0r = wk3r * x1r + wk3i * x1i;
		y0i = wk3r * x1i - wk3i * x1r;
		y2r = wd3r * x3r + wd3i * x3i;
		y2i = wd3r * x3i - wd3i * x3r;
		a[j2 - 2] = y0r + y2r;
		a[j2 - 1] = y0i + y2i;
		a[j3 - 2] = y0r - y2r;
		a[j3 - 1] = y0i - y2i;
		x0r = a[j0] - a[j2 + 1];
		x0i = a[j0 + 1] + a[j2];
		x1r = a[j0] + a[j2 + 1];
		x1i = a[j0 + 1] - a[j2];
		x2r = a[j1] - a[j3 + 1];
		x2i = a[j1 + 1] + a[j3];
		x3r = a[j1] + a[j3 + 1];
		x3i = a[j1 + 1] - a[j3];
		y0r = wl1r * x0r - wl1i * x0i;
		y0i = wl1r * x0i + wl1i * x0r;
		y2r = wl1i * x2r - wl1r * x2i;
		y2i = wl1i * x2i + wl1r * x2r;
		a[j0] = y0r + y2r;
		a[j0 + 1] = y0i + y2i;
		a[j1] = y0r - y2r;
		a[j1 + 1] = y0i - y2i;
		y0r = wl1i * x1r - wl1r * x1i;
		y0i = wl1i * x1i + wl1r * x1r;
		y2r = wl1r * x3r - wl1i * x3i;
		y2i = wl1r * x3i + wl1i * x3r;
		a[j2] = y0r - y2r;
		a[j2 + 1] = y0i - y2i;
		a[j3] = y0r + y2r;
		a[j3 + 1] = y0i + y2i;
		x0r = a[j0 + 2] - a[j2 + 3];
		x0i = a[j0 + 3] + a[j2 + 2];
		x1r = a[j0 + 2] + a[j2 + 3];
		x1i = a[j0 + 3] - a[j2 + 2];
		x2r = a[j1 + 2] - a[j3 + 3];
		x2i = a[j1 + 3] + a[j3 + 2];
		x3r = a[j1 + 2] + a[j3 + 3];
		x3i = a[j1 + 3] - a[j3 + 2];
		y0r = wd1i * x0r - wd1r * x0i;
		y0i = wd1i * x0i + wd1r * x0r;
		y2r = wk1i * x2r - wk1r * x2i;
		y2i = wk1i * x2i + wk1r * x2r;
		a[j0 + 2] = y0r + y2r;
		a[j0 + 3] = y0i + y2i;
		a[j1 + 2] = y0r - y2r;
		a[j1 + 3] = y0i - y2i;
		y0r = wd3i * x1r + wd3r * x1i;
		y0i = wd3i * x1i - wd3r * x1r;
		y2r = wk3i * x3r + wk3r * x3i;
		y2i = wk3i * x3i - wk3r * x3r;
		a[j2 + 2] = y0r + y2r;
		a[j2 + 3] = y0i + y2i;
		a[j3 + 2] = y0r - y2r;
		a[j3 + 3] = y0i - y2i;
	}

	void cftf161(float *a)
	{
		float wn4r, wk1r, wk1i,
			x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i,
			y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i,
			y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i,
			y8r, y8i, y9r, y9i, y10r, y10i, y11r, y11i,
			y12r, y12i, y13r, y13i, y14r, y14i, y15r, y15i;

		wn4r = WR5000;
		wk1r = WR2500;
		wk1i = WI2500;
		x0r = a[0] + a[16];
		x0i = a[1] + a[17];
		x1r = a[0] - a[16];
		x1i = a[1] - a[17];
		x2r = a[8] + a[24];
		x2i = a[9] + a[25];
		x3r = a[8] - a[24];
		x3i = a[9] - a[25];
		y0r = x0r + x2r;
		y0i = x0i + x2i;
		y4r = x0r - x2r;
		y4i = x0i - x2i;
		y8r = x1r - x3i;
		y8i = x1i + x3r;
		y12r = x1r + x3i;
		y12i = x1i - x3r;
		x0r = a[2] + a[18];
		x0i = a[3] + a[19];
		x1r = a[2] - a[18];
		x1i = a[3] - a[19];
		x2r = a[10] + a[26];
		x2i = a[11] + a[27];
		x3r = a[10] - a[26];
		x3i = a[11] - a[27];
		y1r = x0r + x2r;
		y1i = x0i + x2i;
		y5r = x0r - x2r;
		y5i = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		y9r = wk1r * x0r - wk1i * x0i;
		y9i = wk1r * x0i + wk1i * x0r;
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		y13r = wk1i * x0r - wk1r * x0i;
		y13i = wk1i * x0i + wk1r * x0r;
		x0r = a[4] + a[20];
		x0i = a[5] + a[21];
		x1r = a[4] - a[20];
		x1i = a[5] - a[21];
		x2r = a[12] + a[28];
		x2i = a[13] + a[29];
		x3r = a[12] - a[28];
		x3i = a[13] - a[29];
		y2r = x0r + x2r;
		y2i = x0i + x2i;
		y6r = x0r - x2r;
		y6i = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		y10r = wn4r * (x0r - x0i);
		y10i = wn4r * (x0i + x0r);
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		y14r = wn4r * (x0r + x0i);
		y14i = wn4r * (x0i - x0r);
		x0r = a[6] + a[22];
		x0i = a[7] + a[23];
		x1r = a[6] - a[22];
		x1i = a[7] - a[23];
		x2r = a[14] + a[30];
		x2i = a[15] + a[31];
		x3r = a[14] - a[30];
		x3i = a[15] - a[31];
		y3r = x0r + x2r;
		y3i = x0i + x2i;
		y7r = x0r - x2r;
		y7i = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		y11r = wk1i * x0r - wk1r * x0i;
		y11i = wk1i * x0i + wk1r * x0r;
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		y15r = wk1r * x0r - wk1i * x0i;
		y15i = wk1r * x0i + wk1i * x0r;
		x0r = y12r - y14r;
		x0i = y12i - y14i;
		x1r = y12r + y14r;
		x1i = y12i + y14i;
		x2r = y13r - y15r;
		x2i = y13i - y15i;
		x3r = y13r + y15r;
		x3i = y13i + y15i;
		a[24] = x0r + x2r;
		a[25] = x0i + x2i;
		a[26] = x0r - x2r;
		a[27] = x0i - x2i;
		a[28] = x1r - x3i;
		a[29] = x1i + x3r;
		a[30] = x1r + x3i;
		a[31] = x1i - x3r;
		x0r = y8r + y10r;
		x0i = y8i + y10i;
		x1r = y8r - y10r;
		x1i = y8i - y10i;
		x2r = y9r + y11r;
		x2i = y9i + y11i;
		x3r = y9r - y11r;
		x3i = y9i - y11i;
		a[16] = x0r + x2r;
		a[17] = x0i + x2i;
		a[18] = x0r - x2r;
		a[19] = x0i - x2i;
		a[20] = x1r - x3i;
		a[21] = x1i + x3r;
		a[22] = x1r + x3i;
		a[23] = x1i - x3r;
		x0r = y5r - y7i;
		x0i = y5i + y7r;
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		x0r = y5r + y7i;
		x0i = y5i - y7r;
		x3r = wn4r * (x0r - x0i);
		x3i = wn4r * (x0i + x0r);
		x0r = y4r - y6i;
		x0i = y4i + y6r;
		x1r = y4r + y6i;
		x1i = y4i - y6r;
		a[8] = x0r + x2r;
		a[9] = x0i + x2i;
		a[10] = x0r - x2r;
		a[11] = x0i - x2i;
		a[12] = x1r - x3i;
		a[13] = x1i + x3r;
		a[14] = x1r + x3i;
		a[15] = x1i - x3r;
		x0r = y0r + y2r;
		x0i = y0i + y2i;
		x1r = y0r - y2r;
		x1i = y0i - y2i;
		x2r = y1r + y3r;
		x2i = y1i + y3i;
		x3r = y1r - y3r;
		x3i = y1i - y3i;
		a[0] = x0r + x2r;
		a[1] = x0i + x2i;
		a[2] = x0r - x2r;
		a[3] = x0i - x2i;
		a[4] = x1r - x3i;
		a[5] = x1i + x3r;
		a[6] = x1r + x3i;
		a[7] = x1i - x3r;
	}


	void cftf162(float *a)
	{
		float wn4r, wk1r, wk1i, wk2r, wk2i, wk3r, wk3i,
			x0r, x0i, x1r, x1i, x2r, x2i,
			y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i,
			y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i,
			y8r, y8i, y9r, y9i, y10r, y10i, y11r, y11i,
			y12r, y12i, y13r, y13i, y14r, y14i, y15r, y15i;

		wn4r = WR5000;
		wk1r = WR1250;
		wk1i = WI1250;
		wk2r = WR2500;
		wk2i = WI2500;
		wk3r = WR3750;
		wk3i = WI3750;
		x1r = a[0] - a[17];
		x1i = a[1] + a[16];
		x0r = a[8] - a[25];
		x0i = a[9] + a[24];
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		y0r = x1r + x2r;
		y0i = x1i + x2i;
		y4r = x1r - x2r;
		y4i = x1i - x2i;
		x1r = a[0] + a[17];
		x1i = a[1] - a[16];
		x0r = a[8] + a[25];
		x0i = a[9] - a[24];
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		y8r = x1r - x2i;
		y8i = x1i + x2r;
		y12r = x1r + x2i;
		y12i = x1i - x2r;
		x0r = a[2] - a[19];
		x0i = a[3] + a[18];
		x1r = wk1r * x0r - wk1i * x0i;
		x1i = wk1r * x0i + wk1i * x0r;
		x0r = a[10] - a[27];
		x0i = a[11] + a[26];
		x2r = wk3i * x0r - wk3r * x0i;
		x2i = wk3i * x0i + wk3r * x0r;
		y1r = x1r + x2r;
		y1i = x1i + x2i;
		y5r = x1r - x2r;
		y5i = x1i - x2i;
		x0r = a[2] + a[19];
		x0i = a[3] - a[18];
		x1r = wk3r * x0r - wk3i * x0i;
		x1i = wk3r * x0i + wk3i * x0r;
		x0r = a[10] + a[27];
		x0i = a[11] - a[26];
		x2r = wk1r * x0r + wk1i * x0i;
		x2i = wk1r * x0i - wk1i * x0r;
		y9r = x1r - x2r;
		y9i = x1i - x2i;
		y13r = x1r + x2r;
		y13i = x1i + x2i;
		x0r = a[4] - a[21];
		x0i = a[5] + a[20];
		x1r = wk2r * x0r - wk2i * x0i;
		x1i = wk2r * x0i + wk2i * x0r;
		x0r = a[12] - a[29];
		x0i = a[13] + a[28];
		x2r = wk2i * x0r - wk2r * x0i;
		x2i = wk2i * x0i + wk2r * x0r;
		y2r = x1r + x2r;
		y2i = x1i + x2i;
		y6r = x1r - x2r;
		y6i = x1i - x2i;
		x0r = a[4] + a[21];
		x0i = a[5] - a[20];
		x1r = wk2i * x0r - wk2r * x0i;
		x1i = wk2i * x0i + wk2r * x0r;
		x0r = a[12] + a[29];
		x0i = a[13] - a[28];
		x2r = wk2r * x0r - wk2i * x0i;
		x2i = wk2r * x0i + wk2i * x0r;
		y10r = x1r - x2r;
		y10i = x1i - x2i;
		y14r = x1r + x2r;
		y14i = x1i + x2i;
		x0r = a[6] - a[23];
		x0i = a[7] + a[22];
		x1r = wk3r * x0r - wk3i * x0i;
		x1i = wk3r * x0i + wk3i * x0r;
		x0r = a[14] - a[31];
		x0i = a[15] + a[30];
		x2r = wk1i * x0r - wk1r * x0i;
		x2i = wk1i * x0i + wk1r * x0r;
		y3r = x1r + x2r;
		y3i = x1i + x2i;
		y7r = x1r - x2r;
		y7i = x1i - x2i;
		x0r = a[6] + a[23];
		x0i = a[7] - a[22];
		x1r = wk1i * x0r + wk1r * x0i;
		x1i = wk1i * x0i - wk1r * x0r;
		x0r = a[14] + a[31];
		x0i = a[15] - a[30];
		x2r = wk3i * x0r - wk3r * x0i;
		x2i = wk3i * x0i + wk3r * x0r;
		y11r = x1r + x2r;
		y11i = x1i + x2i;
		y15r = x1r - x2r;
		y15i = x1i - x2i;
		x1r = y0r + y2r;
		x1i = y0i + y2i;
		x2r = y1r + y3r;
		x2i = y1i + y3i;
		a[0] = x1r + x2r;
		a[1] = x1i + x2i;
		a[2] = x1r - x2r;
		a[3] = x1i - x2i;
		x1r = y0r - y2r;
		x1i = y0i - y2i;
		x2r = y1r - y3r;
		x2i = y1i - y3i;
		a[4] = x1r - x2i;
		a[5] = x1i + x2r;
		a[6] = x1r + x2i;
		a[7] = x1i - x2r;
		x1r = y4r - y6i;
		x1i = y4i + y6r;
		x0r = y5r - y7i;
		x0i = y5i + y7r;
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		a[8] = x1r + x2r;
		a[9] = x1i + x2i;
		a[10] = x1r - x2r;
		a[11] = x1i - x2i;
		x1r = y4r + y6i;
		x1i = y4i - y6r;
		x0r = y5r + y7i;
		x0i = y5i - y7r;
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		a[12] = x1r - x2i;
		a[13] = x1i + x2r;
		a[14] = x1r + x2i;
		a[15] = x1i - x2r;
		x1r = y8r + y10r;
		x1i = y8i + y10i;
		x2r = y9r - y11r;
		x2i = y9i - y11i;
		a[16] = x1r + x2r;
		a[17] = x1i + x2i;
		a[18] = x1r - x2r;
		a[19] = x1i - x2i;
		x1r = y8r - y10r;
		x1i = y8i - y10i;
		x2r = y9r + y11r;
		x2i = y9i + y11i;
		a[20] = x1r - x2i;
		a[21] = x1i + x2r;
		a[22] = x1r + x2i;
		a[23] = x1i - x2r;
		x1r = y12r - y14i;
		x1i = y12i + y14r;
		x0r = y13r + y15i;
		x0i = y13i - y15r;
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		a[24] = x1r + x2r;
		a[25] = x1i + x2i;
		a[26] = x1r - x2r;
		a[27] = x1i - x2i;
		x1r = y12r + y14i;
		x1i = y12i - y14r;
		x0r = y13r - y15i;
		x0i = y13i + y15r;
		x2r = wn4r * (x0r - x0i);
		x2i = wn4r * (x0i + x0r);
		a[28] = x1r - x2i;
		a[29] = x1i + x2r;
		a[30] = x1r + x2i;
		a[31] = x1i - x2r;
	}


	void cftf081(float *a)
	{
		float wn4r, x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i,
			y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i,
			y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i;

		wn4r = WR5000;
		x0r = a[0] + a[8];
		x0i = a[1] + a[9];
		x1r = a[0] - a[8];
		x1i = a[1] - a[9];
		x2r = a[4] + a[12];
		x2i = a[5] + a[13];
		x3r = a[4] - a[12];
		x3i = a[5] - a[13];
		y0r = x0r + x2r;
		y0i = x0i + x2i;
		y2r = x0r - x2r;
		y2i = x0i - x2i;
		y1r = x1r - x3i;
		y1i = x1i + x3r;
		y3r = x1r + x3i;
		y3i = x1i - x3r;
		x0r = a[2] + a[10];
		x0i = a[3] + a[11];
		x1r = a[2] - a[10];
		x1i = a[3] - a[11];
		x2r = a[6] + a[14];
		x2i = a[7] + a[15];
		x3r = a[6] - a[14];
		x3i = a[7] - a[15];
		y4r = x0r + x2r;
		y4i = x0i + x2i;
		y6r = x0r - x2r;
		y6i = x0i - x2i;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		x2r = x1r + x3i;
		x2i = x1i - x3r;
		y5r = wn4r * (x0r - x0i);
		y5i = wn4r * (x0r + x0i);
		y7r = wn4r * (x2r - x2i);
		y7i = wn4r * (x2r + x2i);
		a[8] = y1r + y5r;
		a[9] = y1i + y5i;
		a[10] = y1r - y5r;
		a[11] = y1i - y5i;
		a[12] = y3r - y7i;
		a[13] = y3i + y7r;
		a[14] = y3r + y7i;
		a[15] = y3i - y7r;
		a[0] = y0r + y4r;
		a[1] = y0i + y4i;
		a[2] = y0r - y4r;
		a[3] = y0i - y4i;
		a[4] = y2r - y6i;
		a[5] = y2i + y6r;
		a[6] = y2r + y6i;
		a[7] = y2i - y6r;
	}


	void cftf082(float *a)
	{
		float wn4r, wk1r, wk1i, x0r, x0i, x1r, x1i,
			y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i,
			y4r, y4i, y5r, y5i, y6r, y6i, y7r, y7i;

		wn4r = WR5000;
		wk1r = WR2500;
		wk1i = WI2500;
		y0r = a[0] - a[9];
		y0i = a[1] + a[8];
		y1r = a[0] + a[9];
		y1i = a[1] - a[8];
		x0r = a[4] - a[13];
		x0i = a[5] + a[12];
		y2r = wn4r * (x0r - x0i);
		y2i = wn4r * (x0i + x0r);
		x0r = a[4] + a[13];
		x0i = a[5] - a[12];
		y3r = wn4r * (x0r - x0i);
		y3i = wn4r * (x0i + x0r);
		x0r = a[2] - a[11];
		x0i = a[3] + a[10];
		y4r = wk1r * x0r - wk1i * x0i;
		y4i = wk1r * x0i + wk1i * x0r;
		x0r = a[2] + a[11];
		x0i = a[3] - a[10];
		y5r = wk1i * x0r - wk1r * x0i;
		y5i = wk1i * x0i + wk1r * x0r;
		x0r = a[6] - a[15];
		x0i = a[7] + a[14];
		y6r = wk1i * x0r - wk1r * x0i;
		y6i = wk1i * x0i + wk1r * x0r;
		x0r = a[6] + a[15];
		x0i = a[7] - a[14];
		y7r = wk1r * x0r - wk1i * x0i;
		y7i = wk1r * x0i + wk1i * x0r;
		x0r = y0r + y2r;
		x0i = y0i + y2i;
		x1r = y4r + y6r;
		x1i = y4i + y6i;
		a[0] = x0r + x1r;
		a[1] = x0i + x1i;
		a[2] = x0r - x1r;
		a[3] = x0i - x1i;
		x0r = y0r - y2r;
		x0i = y0i - y2i;
		x1r = y4r - y6r;
		x1i = y4i - y6i;
		a[4] = x0r - x1i;
		a[5] = x0i + x1r;
		a[6] = x0r + x1i;
		a[7] = x0i - x1r;
		x0r = y1r - y3i;
		x0i = y1i + y3r;
		x1r = y5r - y7r;
		x1i = y5i - y7i;
		a[8] = x0r + x1r;
		a[9] = x0i + x1i;
		a[10] = x0r - x1r;
		a[11] = x0i - x1i;
		x0r = y1r + y3i;
		x0i = y1i - y3r;
		x1r = y5r + y7r;
		x1i = y5i + y7i;
		a[12] = x0r - x1i;
		a[13] = x0i + x1r;
		a[14] = x0r + x1i;
		a[15] = x0i - x1r;
	}

	void cftf040(float *a)
	{
		float x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

		x0r = a[0] + a[4];
		x0i = a[1] + a[5];
		x1r = a[0] - a[4];
		x1i = a[1] - a[5];
		x2r = a[2] + a[6];
		x2i = a[3] + a[7];
		x3r = a[2] - a[6];
		x3i = a[3] - a[7];
		a[0] = x0r + x2r;
		a[1] = x0i + x2i;
		a[2] = x1r - x3i;
		a[3] = x1i + x3r;
		a[4] = x0r - x2r;
		a[5] = x0i - x2i;
		a[6] = x1r + x3i;
		a[7] = x1i - x3r;
	}

	void cftb040(float *a)
	{
		float x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

		x0r = a[0] + a[4];
		x0i = a[1] + a[5];
		x1r = a[0] - a[4];
		x1i = a[1] - a[5];
		x2r = a[2] + a[6];
		x2i = a[3] + a[7];
		x3r = a[2] - a[6];
		x3i = a[3] - a[7];
		a[0] = x0r + x2r;
		a[1] = x0i + x2i;
		a[2] = x1r + x3i;
		a[3] = x1i - x3r;
		a[4] = x0r - x2r;
		a[5] = x0i - x2i;
		a[6] = x1r - x3i;
		a[7] = x1i + x3r;
	}

	void cftx020(float *a)
	{
		float x0r, x0i;

		x0r = a[0] - a[2];
		x0i = a[1] - a[3];
		a[0] += a[2];
		a[1] += a[3];
		a[2] = x0r;
		a[3] = x0i;
	}

	void cftfx41(int n, float *a)
	{
		if (n == 128) {
			cftf161(a);
			cftf162(&a[32]);
			cftf161(&a[64]);
			cftf161(&a[96]);
		}
		else {
			cftf081(a);
			cftf082(&a[16]);
			cftf081(&a[32]);
			cftf081(&a[48]);
		}
	}

	int cfttree(int n, int j, int k, float *a)
	{
		int i, isplt, m;

		if ((k & 3) != 0) {
			isplt = k & 1;
			if (isplt != 0) {
				cftmdl1(n, &a[j - n]);
			}
			else {
				cftmdl2(n, &a[j - n]);
			}
		}
		else {
			m = n;
			for (i = k; (i & 3) == 0; i >>= 2) {
				m <<= 2;
			}
			isplt = i & 1;
			if (isplt != 0) {
				while (m > 128) {
					cftmdl1(m, &a[j - m]);
					m >>= 2;
				}
			}
			else {
				while (m > 128) {
					cftmdl2(m, &a[j - m]);
					m >>= 2;
				}
			}
		}
		return isplt;
	}

	void cftleaf(int n, int isplt, float *a)
	{
		if (n == 512) {
			cftmdl1(128, a);
			cftf161(a);
			cftf162(&a[32]);
			cftf161(&a[64]);
			cftf161(&a[96]);
			cftmdl2(128, &a[128]);
			cftf161(&a[128]);
			cftf162(&a[160]);
			cftf161(&a[192]);
			cftf162(&a[224]);
			cftmdl1(128, &a[256]);
			cftf161(&a[256]);
			cftf162(&a[288]);
			cftf161(&a[320]);
			cftf161(&a[352]);
			if (isplt != 0) {
				cftmdl1(128, &a[384]);
				cftf161(&a[480]);
			}
			else {
				cftmdl2(128, &a[384]);
				cftf162(&a[480]);
			}
			cftf161(&a[384]);
			cftf162(&a[416]);
			cftf161(&a[448]);
		}
		else {
			cftmdl1(64, a);
			cftf081(a);
			cftf082(&a[16]);
			cftf081(&a[32]);
			cftf081(&a[48]);
			cftmdl2(64, &a[64]);
			cftf081(&a[64]);
			cftf082(&a[80]);
			cftf081(&a[96]);
			cftf082(&a[112]);
			cftmdl1(64, &a[128]);
			cftf081(&a[128]);
			cftf082(&a[144]);
			cftf081(&a[160]);
			cftf081(&a[176]);
			if (isplt != 0) {
				cftmdl1(64, &a[192]);
				cftf081(&a[240]);
			}
			else {
				cftmdl2(64, &a[192]);
				cftf082(&a[240]);
			}
			cftf081(&a[192]);
			cftf082(&a[208]);
			cftf081(&a[224]);
		}
	}

	void cftrec4(int n, float *a)
	{
		int isplt, j, k, m;

		m = n;
		while (m > 512) {
			m >>= 2;
			cftmdl1(m, &a[n - m]);
		}
		cftleaf(m, 1, &a[n - m]);
		k = 0;
		for (j = n - m; j > 0; j -= m) {
			k++;
			isplt = cfttree(m, j, k, a);
			cftleaf(m, isplt, &a[j - m]);
		}
	}

	void cftfsub(int n, float *a)
	{
		if (n > 8) {
			if (n > 32) {
				cftmdl1(n, a);
				if (n > 512) {
					cftrec4(n, a);
				}
				else if (n > 128) {
					cftleaf(n, 1, a);
				}
				else {
					cftfx41(n, a);
				}
				bitrv2(n, a);
			}
			else if (n == 32) {
				cftf161(a);
				bitrv216(a);
			}
			else {
				cftf081(a);
				bitrv208(a);
			}
		}
		else if (n == 8) {
			cftf040(a);
		}
		else if (n == 4) {
			cftx020(a);
		}
	}


	void cftbsub(int n, float *a)
	{
		if (n > 8) {
			if (n > 32) {
				cftb1st(n, a);
				if (n > 512) {
					cftrec4(n, a);
				}
				else if (n > 128) {
					cftleaf(n, 1, a);
				}
				else {
					cftfx41(n, a);
				}
				bitrv2conj(n, a);
			}
			else if (n == 32) {
				cftf161(a);
				bitrv216neg(a);
			}
			else {
				cftf081(a);
				bitrv208neg(a);
			}
		}
		else if (n == 8) {
			cftb040(a);
		}
		else if (n == 4) {
			cftx020(a);
		}
	}
} // fftimpl


namespace SoLoud
{
    namespace FFT
    {
        void fft1024(float *aBuffer)
        {
			fft(aBuffer, 1024);
        }

        void fft256(float *aBuffer)
        {
			fft(aBuffer, 256);
        }

        void ifft256(float *aBuffer)
        {
			ifft(aBuffer, 256);
		}

		void fft(float *aBuffer, unsigned int aBufferLength)
		{
			fftimpl::cftbsub(aBufferLength, aBuffer);
		}

		void ifft(float *aBuffer, unsigned int aBufferLength)
		{
			unsigned int i;
			fftimpl::cftfsub(aBufferLength, aBuffer);
			for (i = 0; i < aBufferLength; i++)
				aBuffer[i] *= 1.0f / (aBufferLength / 2);
		}
    };
};
// file: soloud/src/core/soloud.cpp
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

#include <string.h>
#include <stdlib.h>
#include <math.h> // sin
// #include "soloud_internal.h"
// #include "soloud_thread.h"
// #include "soloud_fft.h"

#ifdef SOLOUD_SSE_INTRINSICS
#include <xmmintrin.h>
#endif

//#define FLOATING_POINT_DEBUG

#ifdef FLOATING_POINT_DEBUG
#include <float.h>
#endif

#if !defined(WITH_SDL2) && !defined(WITH_SDL1) && !defined(WITH_PORTAUDIO) && \
   !defined(WITH_OPENAL) && !defined(WITH_XAUDIO2) && !defined(WITH_WINMM) && \
   !defined(WITH_WASAPI) && !defined(WITH_OSS) && !defined(WITH_SDL1_STATIC) && \
   !defined(WITH_SDL2_STATIC) && !defined(WITH_ALSA) && !defined(WITH_OPENSLES) && \
   !defined(WITH_NULL) && !defined(WITH_COREAUDIO) && !defined(WITH_VITA_HOMEBREW)
#error It appears you haven't enabled any of the back-ends. Please #define one or more of the WITH_ defines (or use premake) '
#endif


namespace SoLoud
{
	AlignedFloatBuffer::AlignedFloatBuffer()
	{
		mBasePtr = 0;
		mData = 0;
	}

	result AlignedFloatBuffer::init(unsigned int aFloats)
	{
		delete[] mBasePtr;
		mBasePtr = 0;
		mData = 0;
		mFloats = aFloats;
#ifdef DISABLE_SIMD
		mBasePtr = new unsigned char[aFloats * sizeof(float)];
		if (mBasePtr == NULL)
			return OUT_OF_MEMORY;
		mData = mBasePtr;
#else
		mBasePtr = new unsigned char[aFloats * sizeof(float) + 16];
		if (mBasePtr == NULL)
			return OUT_OF_MEMORY;
		mData = (float *)(((size_t)mBasePtr + 15)&~15);
#endif
		return SO_NO_ERROR;
	}

	void AlignedFloatBuffer::clear()
	{
		memset(mData, 0, sizeof(float) * mFloats);
	}

	AlignedFloatBuffer::~AlignedFloatBuffer()
	{
		delete[] mBasePtr;
	}

	TinyAlignedFloatBuffer::TinyAlignedFloatBuffer()
	{
		unsigned char * basePtr = &mActualData[0];
		mData = (float *)(((size_t)basePtr + 15)&~15);
	}

	Soloud::Soloud()
	{
#ifdef FLOATING_POINT_DEBUG
		unsigned int u;
		u = _controlfp(0, 0);
		u = u & ~(_EM_INVALID | /*_EM_DENORMAL |*/ _EM_ZERODIVIDE | _EM_OVERFLOW /*| _EM_UNDERFLOW  | _EM_INEXACT*/);
		_controlfp(u, _MCW_EM);
#endif
		mInsideAudioThreadMutex = false;
		mScratchSize = 0;
		mScratchNeeded = 0;
		mSamplerate = 0;
		mBufferSize = 0;
		mFlags = 0;
		mGlobalVolume = 0;
		mPlayIndex = 0;
		mBackendData = NULL;
		mAudioThreadMutex = NULL;
		mPostClipScaler = 0;
		mBackendCleanupFunc = NULL;
		mChannels = 2;
		mStreamTime = 0;
		mLastClockedTime = 0;
		mAudioSourceID = 1;
		mBackendString = 0;
		mBackendID = 0;
		mActiveVoiceDirty = true;
		mActiveVoiceCount = 0;
		int i;
		for (i = 0; i < VOICE_COUNT; i++)
			mActiveVoice[i] = 0;
		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			mFilter[i] = NULL;
			mFilterInstance[i] = NULL;
		}
		for (i = 0; i < 256; i++)
		{
			mFFTData[i] = 0;
			mVisualizationWaveData[i] = 0;
			mWaveData[i] = 0;
		}
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			mVisualizationChannelVolume[i] = 0;
		}
		for (i = 0; i < VOICE_COUNT; i++)
		{
			mVoice[i] = 0;
		}
		mVoiceGroup = 0;
		mVoiceGroupCount = 0;

		m3dPosition[0] = 0;
		m3dPosition[1] = 0;
		m3dPosition[2] = 0;
		m3dAt[0] = 0;
		m3dAt[1] = 0;
		m3dAt[2] = -1;
		m3dUp[0] = 0;
		m3dUp[1] = 1;
		m3dUp[2] = 0;
		m3dVelocity[0] = 0;
		m3dVelocity[1] = 0;
		m3dVelocity[2] = 0;
		m3dSoundSpeed = 343.3f;
		mMaxActiveVoices = 16;
		mHighestVoice = 0;
		mActiveVoiceDirty = true;
		mResampleData = NULL;
	}

	Soloud::~Soloud()
	{
		// let's stop all sounds before deinit, so we don't mess up our mutexes
		stopAll();
		deinit();
		unsigned int i;
		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			delete mFilterInstance[i];
		}
		for (i = 0; i < mVoiceGroupCount; i++)
			delete[] mVoiceGroup[i];
		delete[] mVoiceGroup;
		delete[] mResampleData;
	}

	void Soloud::deinit()
	{
		SOLOUD_ASSERT(!mInsideAudioThreadMutex);
		if (mBackendCleanupFunc)
			mBackendCleanupFunc(this);
		mBackendCleanupFunc = 0;
		if (mAudioThreadMutex)
			Thread::destroyMutex(mAudioThreadMutex);
		mAudioThreadMutex = NULL;
	}

	result Soloud::init(unsigned int aFlags, unsigned int aBackend, unsigned int aSamplerate, unsigned int aBufferSize, unsigned int aChannels)
	{
		if (aBackend >= BACKEND_MAX || aChannels == 3 || aChannels == 5 || aChannels == 7 || aChannels > MAX_CHANNELS)
			return INVALID_PARAMETER;

		deinit();

		mAudioThreadMutex = Thread::createMutex();

		mBackendID = 0;
		mBackendString = 0;

		int samplerate = 44100;
		int buffersize = 2048;
		int inited = 0;

		if (aSamplerate != Soloud::AUTO) samplerate = aSamplerate;
		if (aBufferSize != Soloud::AUTO) buffersize = aBufferSize;

#if defined(WITH_SDL1_STATIC)
		if (!inited &&
			(aBackend == Soloud::SDL1 ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = sdl1static_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::SDL1;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_SDL2_STATIC)
		if (!inited &&
			(aBackend == Soloud::SDL2 ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = sdl2static_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::SDL2;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_SDL2)
		if (!inited &&
			(aBackend == Soloud::SDL2 ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = sdl2_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::SDL2;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_SDL1)
		if (!inited &&
			(aBackend == Soloud::SDL1 ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = sdl1_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::SDL1;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_PORTAUDIO)
		if (!inited &&
			(aBackend == Soloud::PORTAUDIO ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = portaudio_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::PORTAUDIO;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_XAUDIO2)
		if (!inited &&
			(aBackend == Soloud::XAUDIO2 ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 4096;

			int ret = xaudio2_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::XAUDIO2;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_WINMM)
		if (!inited &&
			(aBackend == Soloud::WINMM ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 4096;

			int ret = winmm_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::WINMM;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_WASAPI)
		if (!inited &&
			(aBackend == Soloud::WASAPI ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 4096;
			if (aSamplerate == Soloud::AUTO) samplerate = 48000;

			int ret = wasapi_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::WASAPI;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_ALSA)
		if (!inited &&
			(aBackend == Soloud::ALSA ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = alsa_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::ALSA;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_OSS)
		if (!inited &&
			(aBackend == Soloud::OSS ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = oss_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::OSS;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_OPENAL)
		if (!inited &&
			(aBackend == Soloud::OPENAL ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 4096;

			int ret = openal_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::OPENAL;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_COREAUDIO)
		if (!inited &&
			(aBackend == Soloud::COREAUDIO ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = coreaudio_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::COREAUDIO;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_OPENSLES)
		if (!inited &&
			(aBackend == Soloud::OPENSLES ||
			aBackend == Soloud::AUTO))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 4096;

			int ret = opensles_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::OPENSLES;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_VITA_HOMEBREW)
		if (!inited &&
			(aBackend == Soloud::VITA_HOMEBREW ||
			aBackend == Soloud::AUTO))
		{
			int ret = vita_homebrew_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::VITA_HOMEBREW;
			}

			if (ret != 0 && aBackend != Soloud::AUTO)
				return ret;
		}
#endif

#if defined(WITH_NULL)
		if (!inited &&
			(aBackend == Soloud::NULLDRIVER))
		{
			if (aBufferSize == Soloud::AUTO) buffersize = 2048;

			int ret = null_init(this, aFlags, samplerate, buffersize, aChannels);
			if (ret == 0)
			{
				inited = 1;
				mBackendID = Soloud::NULLDRIVER;
			}

			if (ret != 0)
				return ret;
		}
#endif

		if (!inited && aBackend != Soloud::AUTO)
			return NOT_IMPLEMENTED;
		if (!inited)
			return UNKNOWN_ERROR;
		return 0;
	}

	void Soloud::postinit(unsigned int aSamplerate, unsigned int aBufferSize, unsigned int aFlags, unsigned int aChannels)
	{
		mGlobalVolume = 1;
		mChannels = aChannels;
		mSamplerate = aSamplerate;
		mBufferSize = aBufferSize;
		mScratchSize = aBufferSize;
		if (mScratchSize < SAMPLE_GRANULARITY * 2) mScratchSize = SAMPLE_GRANULARITY * 2;
		if (mScratchSize < 4096) mScratchSize = 4096;
		mScratchNeeded = mScratchSize;
		mScratch.init(mScratchSize * MAX_CHANNELS);
		mOutputScratch.init(mScratchSize * MAX_CHANNELS);
		mResampleData = new AlignedFloatBuffer[mMaxActiveVoices * 2];
		mResampleDataOwner = new AudioSourceInstance*[mMaxActiveVoices];
		unsigned int i;
		for (i = 0; i < mMaxActiveVoices * 2; i++)
			mResampleData[i].init(SAMPLE_GRANULARITY * MAX_CHANNELS);
		for (i = 0; i < mMaxActiveVoices; i++)
			mResampleDataOwner[i] = NULL;
		mFlags = aFlags;
		mPostClipScaler = 0.95f;
		switch (mChannels)
		{
		case 1:
			m3dSpeakerPosition[0 * 3 + 0] = 0;
			m3dSpeakerPosition[0 * 3 + 1] = 0;
			m3dSpeakerPosition[0 * 3 + 2] = 1;
			break;
		case 2:
			m3dSpeakerPosition[0 * 3 + 0] = 2;
			m3dSpeakerPosition[0 * 3 + 1] = 0;
			m3dSpeakerPosition[0 * 3 + 2] = 1;
			m3dSpeakerPosition[1 * 3 + 0] = -2;
			m3dSpeakerPosition[1 * 3 + 1] = 0;
			m3dSpeakerPosition[1 * 3 + 2] = 1;
			break;
		case 4:
			m3dSpeakerPosition[0 * 3 + 0] = 2;
			m3dSpeakerPosition[0 * 3 + 1] = 0;
			m3dSpeakerPosition[0 * 3 + 2] = 1;
			m3dSpeakerPosition[1 * 3 + 0] = -2;
			m3dSpeakerPosition[1 * 3 + 1] = 0;
			m3dSpeakerPosition[1 * 3 + 2] = 1;
			// I suppose technically the second pair should be straight left & right,
			// but I prefer moving them a bit back to mirror the front speakers.
			m3dSpeakerPosition[2 * 3 + 0] = 2;
			m3dSpeakerPosition[2 * 3 + 1] = 0;
			m3dSpeakerPosition[2 * 3 + 2] = -1;
			m3dSpeakerPosition[3 * 3 + 0] = -2;
			m3dSpeakerPosition[3 * 3 + 1] = 0;
			m3dSpeakerPosition[3 * 3 + 2] = -1;
			break;
		case 6:
			m3dSpeakerPosition[0 * 3 + 0] = 2;
			m3dSpeakerPosition[0 * 3 + 1] = 0;
			m3dSpeakerPosition[0 * 3 + 2] = 1;
			m3dSpeakerPosition[1 * 3 + 0] = -2;
			m3dSpeakerPosition[1 * 3 + 1] = 0;
			m3dSpeakerPosition[1 * 3 + 2] = 1;

			// center and subwoofer.
			m3dSpeakerPosition[2 * 3 + 0] = 0;
			m3dSpeakerPosition[2 * 3 + 1] = 0;
			m3dSpeakerPosition[2 * 3 + 2] = 1;
			// Sub should be "mix of everything". We'll handle it as a special case and make it a null vector.
			m3dSpeakerPosition[3 * 3 + 0] = 0;
			m3dSpeakerPosition[3 * 3 + 1] = 0;
			m3dSpeakerPosition[3 * 3 + 2] = 0;

			// I suppose technically the second pair should be straight left & right,
			// but I prefer moving them a bit back to mirror the front speakers.
			m3dSpeakerPosition[4 * 3 + 0] = 2;
			m3dSpeakerPosition[4 * 3 + 1] = 0;
			m3dSpeakerPosition[4 * 3 + 2] = -1;
			m3dSpeakerPosition[5 * 3 + 0] = -2;
			m3dSpeakerPosition[5 * 3 + 1] = 0;
			m3dSpeakerPosition[5 * 3 + 2] = -1;
			break;
		case 8:
			m3dSpeakerPosition[0 * 3 + 0] = 2;
			m3dSpeakerPosition[0 * 3 + 1] = 0;
			m3dSpeakerPosition[0 * 3 + 2] = 1;
			m3dSpeakerPosition[1 * 3 + 0] = -2;
			m3dSpeakerPosition[1 * 3 + 1] = 0;
			m3dSpeakerPosition[1 * 3 + 2] = 1;

			// center and subwoofer.
			m3dSpeakerPosition[2 * 3 + 0] = 0;
			m3dSpeakerPosition[2 * 3 + 1] = 0;
			m3dSpeakerPosition[2 * 3 + 2] = 1;
			// Sub should be "mix of everything". We'll handle it as a special case and make it a null vector.
			m3dSpeakerPosition[3 * 3 + 0] = 0;
			m3dSpeakerPosition[3 * 3 + 1] = 0;
			m3dSpeakerPosition[3 * 3 + 2] = 0;

			// side
			m3dSpeakerPosition[4 * 3 + 0] = 2;
			m3dSpeakerPosition[4 * 3 + 1] = 0;
			m3dSpeakerPosition[4 * 3 + 2] = 0;
			m3dSpeakerPosition[5 * 3 + 0] = -2;
			m3dSpeakerPosition[5 * 3 + 1] = 0;
			m3dSpeakerPosition[5 * 3 + 2] = 0;

			// back
			m3dSpeakerPosition[6 * 3 + 0] = 2;
			m3dSpeakerPosition[6 * 3 + 1] = 0;
			m3dSpeakerPosition[6 * 3 + 2] = -1;
			m3dSpeakerPosition[7 * 3 + 0] = -2;
			m3dSpeakerPosition[7 * 3 + 1] = 0;
			m3dSpeakerPosition[7 * 3 + 2] = -1;
			break;
		}
	}

	const char * Soloud::getErrorString(result aErrorCode) const
	{
		switch (aErrorCode)
		{
		case SO_NO_ERROR: return "No error";
		case INVALID_PARAMETER: return "Some parameter is invalid";
		case FILE_NOT_FOUND: return "File not found";
		case FILE_LOAD_FAILED: return "File found, but could not be loaded";
		case DLL_NOT_FOUND: return "DLL not found, or wrong DLL";
		case OUT_OF_MEMORY: return "Out of memory";
		case NOT_IMPLEMENTED: return "Feature not implemented";
		/*case UNKNOWN_ERROR: return "Other error";*/
		}
		return "Other error";
	}


	float * Soloud::getWave()
	{
		int i;
		lockAudioMutex();
		for (i = 0; i < 256; i++)
			mWaveData[i] = mVisualizationWaveData[i];
		unlockAudioMutex();
		return mWaveData;
	}

	float Soloud::getApproximateVolume(unsigned int aChannel)
	{
		if (aChannel > mChannels)
			return 0;
		float vol = 0;
		lockAudioMutex();
		vol = mVisualizationChannelVolume[aChannel];
		unlockAudioMutex();
		return vol;
	}


	float * Soloud::calcFFT()
	{
		lockAudioMutex();
		float temp[1024];
		int i;
		for (i = 0; i < 256; i++)
		{
			temp[i*2] = mVisualizationWaveData[i];
			temp[i*2+1] = 0;
			temp[i+512] = 0;
			temp[i+768] = 0;
		}
		unlockAudioMutex();

		SoLoud::FFT::fft1024(temp);

		for (i = 0; i < 256; i++)
		{
			float real = temp[i * 2];
			float imag = temp[i * 2 + 1];
			mFFTData[i] = (float)sqrt(real*real+imag*imag);
		}

		return mFFTData;
	}

#ifdef SOLOUD_SSE_INTRINSICS
	void Soloud::clip(AlignedFloatBuffer &aBuffer, AlignedFloatBuffer &aDestBuffer, unsigned int aSamples, float aVolume0, float aVolume1)
	{
		float vd = (aVolume1 - aVolume0) / aSamples;
		float v = aVolume0;
		unsigned int i, j, c, d;
		unsigned int samplequads = (aSamples + 3) / 4; // rounded up

		// Clip
		if (mFlags & CLIP_ROUNDOFF)
		{
			float nb = -1.65f;		__m128 negbound = _mm_load_ps1(&nb);
			float pb = 1.65f;		__m128 posbound = _mm_load_ps1(&pb);
			float ls = 0.87f;		__m128 linearscale = _mm_load_ps1(&ls);
			float cs = -0.1f;		__m128 cubicscale = _mm_load_ps1(&cs);
			float nw = -0.9862875f;	__m128 negwall = _mm_load_ps1(&nw);
			float pw = 0.9862875f;	__m128 poswall = _mm_load_ps1(&pw);
			__m128 postscale = _mm_load_ps1(&mPostClipScaler);
			TinyAlignedFloatBuffer volumes;
			volumes.mData[0] = v;
			volumes.mData[1] = v + vd;
			volumes.mData[2] = v + vd + vd;
			volumes.mData[3] = v + vd + vd + vd;
			vd *= 4;
			__m128 vdelta = _mm_load_ps1(&vd);
			c = 0;
			d = 0;
			for (j = 0; j < mChannels; j++)
			{
				__m128 vol = _mm_load_ps(volumes.mData);

				for (i = 0; i < samplequads; i++)
				{
					//float f1 = origdata[c] * v;	c++; v += vd;
					__m128 f = _mm_load_ps(&aBuffer.mData[c]);
					c += 4;
					f = _mm_mul_ps(f, vol);
					vol = _mm_add_ps(vol, vdelta);

					//float u1 = (f1 > -1.65f);
					__m128 u = _mm_cmpgt_ps(f, negbound);

					//float o1 = (f1 < 1.65f);
					__m128 o = _mm_cmplt_ps(f, posbound);

					//f1 = (0.87f * f1 - 0.1f * f1 * f1 * f1) * u1 * o1;
					__m128 lin = _mm_mul_ps(f, linearscale);
					__m128 cubic = _mm_mul_ps(f, f);
					cubic = _mm_mul_ps(cubic, f);
					cubic = _mm_mul_ps(cubic, cubicscale);
					f = _mm_add_ps(cubic, lin);

					//f1 = f1 * u1 + !u1 * -0.9862875f;
					__m128 lowmask = _mm_andnot_ps(u, negwall);
					__m128 ilowmask = _mm_and_ps(u, f);
					f = _mm_add_ps(lowmask, ilowmask);

					//f1 = f1 * o1 + !o1 * 0.9862875f;
					__m128 himask = _mm_andnot_ps(o, poswall);
					__m128 ihimask = _mm_and_ps(o, f);
					f = _mm_add_ps(himask, ihimask);

					// outdata[d] = f1 * postclip; d++;
					f = _mm_mul_ps(f, postscale);
					_mm_store_ps(&aDestBuffer.mData[d], f);
					d += 4;
				}
			}
		}
		else
		{
			float nb = -1.0f;	__m128 negbound = _mm_load_ps1(&nb);
			float pb = 1.0f;	__m128 posbound = _mm_load_ps1(&pb);
			__m128 postscale = _mm_load_ps1(&mPostClipScaler);
			TinyAlignedFloatBuffer volumes;
			volumes.mData[0] = v;
			volumes.mData[1] = v + vd;
			volumes.mData[2] = v + vd + vd;
			volumes.mData[3] = v + vd + vd + vd;
			vd *= 4;
			__m128 vdelta = _mm_load_ps1(&vd);
			c = 0;
			d = 0;
			for (j = 0; j < mChannels; j++)
			{
				__m128 vol = _mm_load_ps(volumes.mData);
				for (i = 0; i < samplequads; i++)
				{
					//float f1 = aBuffer.mData[c] * v; c++; v += vd;
					__m128 f = _mm_load_ps(&aBuffer.mData[c]);
					c += 4;
					f = _mm_mul_ps(f, vol);
					vol = _mm_add_ps(vol, vdelta);

					//f1 = (f1 <= -1) ? -1 : (f1 >= 1) ? 1 : f1;
					f = _mm_max_ps(f, negbound);
					f = _mm_min_ps(f, posbound);

					//aDestBuffer.mData[d] = f1 * mPostClipScaler; d++;
					f = _mm_mul_ps(f, postscale);
					_mm_store_ps(&aDestBuffer.mData[d], f);
					d += 4;
				}
			}
		}
	}
#else // fallback code
	void Soloud::clip(AlignedFloatBuffer &aBuffer, AlignedFloatBuffer &aDestBuffer, unsigned int aSamples, float aVolume0, float aVolume1)
	{
		float vd = (aVolume1 - aVolume0) / aSamples;
		float v = aVolume0;
		unsigned int i, j, c, d;
		unsigned int samplequads = (aSamples + 3) / 4; // rounded up
		// Clip
		if (mFlags & CLIP_ROUNDOFF)
		{
			c = 0;
			d = 0;
			for (j = 0; j < mChannels; j++)
			{
				v = aVolume0;
				for (i = 0; i < samplequads; i++)
				{
					float f1 = aBuffer.mData[c] * v; c++; v += vd;
					float f2 = aBuffer.mData[c] * v; c++; v += vd;
					float f3 = aBuffer.mData[c] * v; c++; v += vd;
					float f4 = aBuffer.mData[c] * v; c++; v += vd;

					f1 = (f1 <= -1.65f) ? -0.9862875f : (f1 >= 1.65f) ? 0.9862875f : (0.87f * f1 - 0.1f * f1 * f1 * f1);
					f2 = (f2 <= -1.65f) ? -0.9862875f : (f2 >= 1.65f) ? 0.9862875f : (0.87f * f2 - 0.1f * f2 * f2 * f2);
					f3 = (f3 <= -1.65f) ? -0.9862875f : (f3 >= 1.65f) ? 0.9862875f : (0.87f * f3 - 0.1f * f3 * f3 * f3);
					f4 = (f4 <= -1.65f) ? -0.9862875f : (f4 >= 1.65f) ? 0.9862875f : (0.87f * f4 - 0.1f * f4 * f4 * f4);

					aDestBuffer.mData[d] = f1 * mPostClipScaler; d++;
					aDestBuffer.mData[d] = f2 * mPostClipScaler; d++;
					aDestBuffer.mData[d] = f3 * mPostClipScaler; d++;
					aDestBuffer.mData[d] = f4 * mPostClipScaler; d++;
				}
			}
		}
		else
		{
			c = 0;
			d = 0;
			for (j = 0; j < mChannels; j++)
			{
				v = aVolume0;
				for (i = 0; i < samplequads; i++)
				{
					float f1 = aBuffer.mData[c] * v; c++; v += vd;
					float f2 = aBuffer.mData[c] * v; c++; v += vd;
					float f3 = aBuffer.mData[c] * v; c++; v += vd;
					float f4 = aBuffer.mData[c] * v; c++; v += vd;

					f1 = (f1 <= -1) ? -1 : (f1 >= 1) ? 1 : f1;
					f2 = (f2 <= -1) ? -1 : (f2 >= 1) ? 1 : f2;
					f3 = (f3 <= -1) ? -1 : (f3 >= 1) ? 1 : f3;
					f4 = (f4 <= -1) ? -1 : (f4 >= 1) ? 1 : f4;

					aDestBuffer.mData[d] = f1 * mPostClipScaler; d++;
					aDestBuffer.mData[d] = f2 * mPostClipScaler; d++;
					aDestBuffer.mData[d] = f3 * mPostClipScaler; d++;
					aDestBuffer.mData[d] = f4 * mPostClipScaler; d++;
			}
		}
	}
}
#endif

#define FIXPOINT_FRAC_BITS 20
#define FIXPOINT_FRAC_MUL (1 << FIXPOINT_FRAC_BITS)
#define FIXPOINT_FRAC_MASK ((1 << FIXPOINT_FRAC_BITS) - 1)

	void resample(float *aSrc,
		          float *aSrc1,
				  float *aDst,
				  int aSrcOffset,
				  int aDstSampleCount,
				  float aSrcSamplerate,
				  float aDstSamplerate,
				  int aStepFixed)
	{
#if 0

#elif defined(RESAMPLER_LINEAR)
		int i;
		int pos = aSrcOffset;

		for (i = 0; i < aDstSampleCount; i++, pos += aStepFixed)
		{
			int p = pos >> FIXPOINT_FRAC_BITS;
			int f = pos & FIXPOINT_FRAC_MASK;
#ifdef _DEBUG
			if (p >= SAMPLE_GRANULARITY || p < 0)
			{
				// This should never actually happen
				p = SAMPLE_GRANULARITY - 1;
			}
#endif
			float s1 = aSrc1[SAMPLE_GRANULARITY - 1];
			float s2 = aSrc[p];
			if (p != 0)
			{
				s1 = aSrc[p-1];
			}
			aDst[i] = s1 + (s2 - s1) * f * (1 / (float)FIXPOINT_FRAC_MUL);
		}
#else // Point sample
		int i;
		int pos = aSrcOffset;

		for (i = 0; i < aDstSampleCount; i++, pos += aStepFixed)
		{
			int p = pos >> FIXPOINT_FRAC_BITS;
			aDst[i] = aSrc[p];
		}
#endif
	}

	void panAndExpand(AudioSourceInstance *aVoice, float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize, float *aScratch, unsigned int aChannels)
	{
		float pan[MAX_CHANNELS]; // current speaker volume
		float pand[MAX_CHANNELS]; // destination speaker volume
		float pani[MAX_CHANNELS]; // speaker volume increment per sample
		unsigned int j, k;
		for (k = 0; k < aChannels; k++)
		{
			pan[k] = aVoice->mCurrentChannelVolume[k];
			pand[k] = aVoice->mChannelVolume[k] * aVoice->mOverallVolume;
			pani[k] = (pand[k] - pan[k]) / aSamplesToRead; // TODO: this is a bit inconsistent.. but it's a hack to begin with
		}

		int ofs = 0;
		switch (aChannels)
		{
		case 1: // Target is mono. Sum everything. (1->1, 2->1, 4->1, 6->1, 8->1)
			for (j = 0, ofs = 0; j < aVoice->mChannels; j++, ofs += aBufferSize)
			{
				pan[0] = aVoice->mCurrentChannelVolume[0];
				for (k = 0; k < aSamplesToRead; k++)
				{
					pan[0] += pani[0];
					aBuffer[k] += aScratch[ofs + k] * pan[0];
				}
			}
			break;
		case 2:
			switch (aVoice->mChannels)
			{
			case 8: // 8->2, just sum lefties and righties, add a bit of center and sub?
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					float s7 = aScratch[aBufferSize * 6 + j];
					float s8 = aScratch[aBufferSize * 7 + j];
					aBuffer[j + 0]           += 0.2f * (s1 + s3 + s4 + s5 + s7) * pan[0];
					aBuffer[j + aBufferSize] += 0.2f * (s2 + s3 + s4 + s6 + s8) * pan[1];
				}
				break;
			case 6: // 6->2, just sum lefties and righties, add a bit of center and sub?
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					aBuffer[j + 0] += 0.3f * (s1 + s3 + s4 + s5) * pan[0];
					aBuffer[j + aBufferSize] += 0.3f * (s2 + s3 + s4 + s6) * pan[1];
				}
				break;
			case 4: // 4->2, just sum lefties and righties
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					aBuffer[j + 0] += 0.5f * (s1 + s3) * pan[0];
					aBuffer[j + aBufferSize] += 0.5f * (s2 + s4) * pan[1];
				}
				break;
			case 2: // 2->2
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
				}
				break;
			case 1: // 1->2
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					float s = aScratch[j];
					aBuffer[j + 0] += s * pan[0];
					aBuffer[j + aBufferSize] += s * pan[1];
				}
				break;
			}
			break;
		case 4:
			switch (aVoice->mChannels)
			{
			case 8: // 8->4, add a bit of center, sub?
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					float s7 = aScratch[aBufferSize * 6 + j];
					float s8 = aScratch[aBufferSize * 7 + j];
					float c = (s3 + s4) * 0.7f;
					aBuffer[j + 0]               += s1 * pan[0] + c;
					aBuffer[j + aBufferSize]     += s2 * pan[1] + c;
					aBuffer[j + aBufferSize * 2] += 0.5f * (s5 + s7) * pan[2];
					aBuffer[j + aBufferSize * 3] += 0.5f * (s6 + s8) * pan[3];
				}
				break;
			case 6: // 6->4, add a bit of center, sub?
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					float c = (s3 + s4) * 0.7f;
					aBuffer[j + 0] += s1 * pan[0] + c;
					aBuffer[j + aBufferSize] += s2 * pan[1] + c;
					aBuffer[j + aBufferSize * 2] += s5 * pan[2];
					aBuffer[j + aBufferSize * 3] += s6 * pan[3];
				}
				break;
			case 4: // 4->4
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += s3 * pan[2];
					aBuffer[j + aBufferSize * 3] += s4 * pan[3];
				}
				break;
			case 2: // 2->4
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += s1 * pan[2];
					aBuffer[j + aBufferSize * 3] += s2 * pan[3];
				}
				break;
			case 1: // 1->4
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					float s = aScratch[j];
					aBuffer[j + 0] += s * pan[0];
					aBuffer[j + aBufferSize] += s * pan[1];
					aBuffer[j + aBufferSize * 2] += s * pan[2];
					aBuffer[j + aBufferSize * 3] += s * pan[3];
				}
				break;
			}
			break;
		case 6:
			switch (aVoice->mChannels)
			{
			case 8: // 8->6
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					float s7 = aScratch[aBufferSize * 6 + j];
					float s8 = aScratch[aBufferSize * 7 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += s3 * pan[2];
					aBuffer[j + aBufferSize * 3] += s4 * pan[3];
					aBuffer[j + aBufferSize * 4] += 0.5f * (s5 + s7) * pan[4];
					aBuffer[j + aBufferSize * 5] += 0.5f * (s6 + s8) * pan[5];
				}
				break;
			case 6: // 6->6
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += s3 * pan[2];
					aBuffer[j + aBufferSize * 3] += s4 * pan[3];
					aBuffer[j + aBufferSize * 4] += s5 * pan[4];
					aBuffer[j + aBufferSize * 5] += s6 * pan[5];
				}
				break;
			case 4: // 4->6
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += 0.5f * (s1 + s2) * pan[2];
					aBuffer[j + aBufferSize * 3] += 0.25f * (s1 + s2 + s3 + s4) * pan[3];
					aBuffer[j + aBufferSize * 4] += s3 * pan[4];
					aBuffer[j + aBufferSize * 5] += s4 * pan[5];
				}
				break;
			case 2: // 2->6
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += 0.5f * (s1 + s2) * pan[2];
					aBuffer[j + aBufferSize * 3] += 0.5f * (s1 + s2) * pan[3];
					aBuffer[j + aBufferSize * 4] += s1 * pan[4];
					aBuffer[j + aBufferSize * 5] += s2 * pan[5];
				}
				break;
			case 1: // 1->6
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					float s = aScratch[j];
					aBuffer[j + 0] += s * pan[0];
					aBuffer[j + aBufferSize] += s * pan[1];
					aBuffer[j + aBufferSize * 2] += s * pan[2];
					aBuffer[j + aBufferSize * 3] += s * pan[3];
					aBuffer[j + aBufferSize * 4] += s * pan[4];
					aBuffer[j + aBufferSize * 5] += s * pan[5];
				}
				break;
			}
			break;
		case 8:
			switch (aVoice->mChannels)
			{
			case 8: // 8->8
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					pan[6] += pani[6];
					pan[7] += pani[7];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					float s7 = aScratch[aBufferSize * 6 + j];
					float s8 = aScratch[aBufferSize * 7 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += s3 * pan[2];
					aBuffer[j + aBufferSize * 3] += s4 * pan[3];
					aBuffer[j + aBufferSize * 4] += s5 * pan[4];
					aBuffer[j + aBufferSize * 5] += s6 * pan[5];
					aBuffer[j + aBufferSize * 6] += s7 * pan[6];
					aBuffer[j + aBufferSize * 7] += s8 * pan[7];
				}
				break;
			case 6: // 6->8
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					pan[6] += pani[6];
					pan[7] += pani[7];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					float s5 = aScratch[aBufferSize * 4 + j];
					float s6 = aScratch[aBufferSize * 5 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += s3 * pan[2];
					aBuffer[j + aBufferSize * 3] += s4 * pan[3];
					aBuffer[j + aBufferSize * 4] += 0.5f * (s5 + s1) * pan[4];
					aBuffer[j + aBufferSize * 5] += 0.5f * (s6 + s2) * pan[5];
					aBuffer[j + aBufferSize * 6] += s5 * pan[6];
					aBuffer[j + aBufferSize * 7] += s6 * pan[7];
				}
				break;
			case 4: // 4->8
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					pan[6] += pani[6];
					pan[7] += pani[7];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					float s3 = aScratch[aBufferSize * 2 + j];
					float s4 = aScratch[aBufferSize * 3 + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += 0.5f * (s1 + s2) * pan[2];
					aBuffer[j + aBufferSize * 3] += 0.25f * (s1 + s2 + s3 + s4) * pan[3];
					aBuffer[j + aBufferSize * 4] += 0.5f * (s1 + s3) * pan[4];
					aBuffer[j + aBufferSize * 5] += 0.5f * (s2 + s4) * pan[5];
					aBuffer[j + aBufferSize * 6] += s3 * pan[4];
					aBuffer[j + aBufferSize * 7] += s4 * pan[5];
				}
				break;
			case 2: // 2->8
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					pan[6] += pani[6];
					pan[7] += pani[7];
					float s1 = aScratch[j];
					float s2 = aScratch[aBufferSize + j];
					aBuffer[j + 0] += s1 * pan[0];
					aBuffer[j + aBufferSize] += s2 * pan[1];
					aBuffer[j + aBufferSize * 2] += 0.5f * (s1 + s2) * pan[2];
					aBuffer[j + aBufferSize * 3] += 0.5f * (s1 + s2) * pan[3];
					aBuffer[j + aBufferSize * 4] += s1 * pan[4];
					aBuffer[j + aBufferSize * 5] += s2 * pan[5];
					aBuffer[j + aBufferSize * 6] += s1 * pan[6];
					aBuffer[j + aBufferSize * 7] += s2 * pan[7];
				}
				break;
			case 1: // 1->8
				for (j = 0; j < aSamplesToRead; j++)
				{
					pan[0] += pani[0];
					pan[1] += pani[1];
					pan[2] += pani[2];
					pan[3] += pani[3];
					pan[4] += pani[4];
					pan[5] += pani[5];
					pan[6] += pani[6];
					pan[7] += pani[7];
					float s = aScratch[j];
					aBuffer[j + 0] += s * pan[0];
					aBuffer[j + aBufferSize] += s * pan[1];
					aBuffer[j + aBufferSize * 2] += s * pan[2];
					aBuffer[j + aBufferSize * 3] += s * pan[3];
					aBuffer[j + aBufferSize * 4] += s * pan[4];
					aBuffer[j + aBufferSize * 5] += s * pan[5];
					aBuffer[j + aBufferSize * 6] += s * pan[6];
					aBuffer[j + aBufferSize * 7] += s * pan[7];
				}
				break;
			}
			break;
		}

		for (k = 0; k < aChannels; k++)
			aVoice->mCurrentChannelVolume[k] = pand[k];
	}

	void Soloud::mixBus(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize, float *aScratch, unsigned int aBus, float aSamplerate, unsigned int aChannels)
	{
		unsigned int i, j;
		// Clear accumulation buffer
		for (i = 0; i < aSamplesToRead; i++)
		{
			for (j = 0; j < aChannels; j++)
			{
				aBuffer[i + j * aBufferSize] = 0;
			}
		}

		// Accumulate sound sources
		for (i = 0; i < mActiveVoiceCount; i++)
		{
			AudioSourceInstance *voice = mVoice[mActiveVoice[i]];
			if (voice &&
				voice->mBusHandle == aBus &&
				!(voice->mFlags & AudioSourceInstance::PAUSED) &&
				!(voice->mFlags & AudioSourceInstance::INAUDIBLE))
			{
				unsigned int j;
				float step = voice->mSamplerate / aSamplerate;
				// avoid step overflow
				if (step > (1 << (32 - FIXPOINT_FRAC_BITS)))
					step = 0;
				unsigned int step_fixed = (int)floor(step * FIXPOINT_FRAC_MUL);
				unsigned int outofs = 0;

				if (voice->mDelaySamples)
				{
					if (voice->mDelaySamples > aSamplesToRead)
					{
						outofs = aSamplesToRead;
						voice->mDelaySamples -= aSamplesToRead;
					}
					else
					{
						outofs = voice->mDelaySamples;
						voice->mDelaySamples = 0;
					}

					// Clear scratch where we're skipping
					for (j = 0; j < voice->mChannels; j++)
					{
						memset(aScratch + j * aBufferSize, 0, sizeof(float) * outofs);
					}
				}

				while (step_fixed != 0 && outofs < aSamplesToRead)
				{
					if (voice->mLeftoverSamples == 0)
					{
						// Swap resample buffers (ping-pong)
						AlignedFloatBuffer * t = voice->mResampleData[0];
						voice->mResampleData[0] = voice->mResampleData[1];
						voice->mResampleData[1] = t;

						// Get a block of source data

						int readcount = 0;
						if (!voice->hasEnded() || voice->mFlags & AudioSourceInstance::LOOPING)
						{
							readcount = voice->getAudio(voice->mResampleData[0]->mData, SAMPLE_GRANULARITY, SAMPLE_GRANULARITY);
							if (readcount < SAMPLE_GRANULARITY)
							{
								if (voice->mFlags & AudioSourceInstance::LOOPING)
								{
									while (readcount < SAMPLE_GRANULARITY && voice->seek(voice->mLoopPoint, mScratch.mData, mScratchSize) == SO_NO_ERROR)
									{
										voice->mLoopCount++;
										int inc = voice->getAudio(voice->mResampleData[0]->mData + readcount, SAMPLE_GRANULARITY - readcount, SAMPLE_GRANULARITY);
										readcount += inc;
										if (inc == 0) break;
									}
								}
							}
						}

                        // Clear remaining of the resample data if the full scratch wasn't used
						if (readcount < SAMPLE_GRANULARITY)
						{
							unsigned int i;
							for (i = 0; i < voice->mChannels; i++)
								memset(voice->mResampleData[0]->mData + readcount + SAMPLE_GRANULARITY * i, 0, sizeof(float) * (SAMPLE_GRANULARITY - readcount));
						}

						// If we go past zero, crop to zero (a bit of a kludge)
						if (voice->mSrcOffset < SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL)
						{
							voice->mSrcOffset = 0;
						}
						else
						{
							// We have new block of data, move pointer backwards
							voice->mSrcOffset -= SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL;
						}


						// Run the per-stream filters to get our source data

						for (j = 0; j < FILTERS_PER_STREAM; j++)
						{
							if (voice->mFilter[j])
							{
								voice->mFilter[j]->filter(
									voice->mResampleData[0]->mData,
									SAMPLE_GRANULARITY,
									voice->mChannels,
									voice->mSamplerate,
									mStreamTime);
							}
						}
					}
					else
					{
						voice->mLeftoverSamples = 0;
					}

					// Figure out how many samples we can generate from this source data.
					// The value may be zero.

					unsigned int writesamples = 0;

					if (voice->mSrcOffset < SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL)
					{
						writesamples = ((SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL) - voice->mSrcOffset) / step_fixed + 1;

						// avoid reading past the current buffer..
						if (((writesamples * step_fixed + voice->mSrcOffset) >> FIXPOINT_FRAC_BITS) >= SAMPLE_GRANULARITY)
							writesamples--;
					}


					// If this is too much for our output buffer, don't write that many:
					if (writesamples + outofs > aSamplesToRead)
					{
						voice->mLeftoverSamples = (writesamples + outofs) - aSamplesToRead;
						writesamples = aSamplesToRead - outofs;
					}

					// Call resampler to generate the samples, once per channel
					if (writesamples)
					{
						for (j = 0; j < voice->mChannels; j++)
						{
							resample(voice->mResampleData[0]->mData + SAMPLE_GRANULARITY * j,
								voice->mResampleData[1]->mData + SAMPLE_GRANULARITY * j,
									 aScratch + aBufferSize * j + outofs,
									 voice->mSrcOffset,
									 writesamples,
									 voice->mSamplerate,
									 aSamplerate,
									 step_fixed);
						}
					}

					// Keep track of how many samples we've written so far
					outofs += writesamples;

					// Move source pointer onwards (writesamples may be zero)
					voice->mSrcOffset += writesamples * step_fixed;
				}

				// Handle panning and channel expansion (and/or shrinking)
				panAndExpand(voice, aBuffer, aSamplesToRead, aBufferSize, aScratch, aChannels);

				// clear voice if the sound is over
				if (!(voice->mFlags & AudioSourceInstance::LOOPING) && voice->hasEnded())
				{
					stopVoice(mActiveVoice[i]);
				}
			}
			else
				if (voice &&
					voice->mBusHandle == aBus &&
					!(voice->mFlags & AudioSourceInstance::PAUSED) &&
					(voice->mFlags & AudioSourceInstance::INAUDIBLE) &&
					(voice->mFlags & AudioSourceInstance::INAUDIBLE_TICK))
			{
				// Inaudible but needs ticking. Do minimal work (keep counters up to date and ask audiosource for data)
				float step = voice->mSamplerate / aSamplerate;
				int step_fixed = (int)floor(step * FIXPOINT_FRAC_MUL);
				unsigned int outofs = 0;

				if (voice->mDelaySamples)
				{
					if (voice->mDelaySamples > aSamplesToRead)
					{
						outofs = aSamplesToRead;
						voice->mDelaySamples -= aSamplesToRead;
					}
					else
					{
						outofs = voice->mDelaySamples;
						voice->mDelaySamples = 0;
					}
				}

				while (step_fixed != 0 && outofs < aSamplesToRead)
				{
					if (voice->mLeftoverSamples == 0)
					{
						// Swap resample buffers (ping-pong)
						AlignedFloatBuffer * t = voice->mResampleData[0];
						voice->mResampleData[0] = voice->mResampleData[1];
						voice->mResampleData[1] = t;

						// Get a block of source data

						int readcount = 0;
						if (!voice->hasEnded() || voice->mFlags & AudioSourceInstance::LOOPING)
						{
							readcount = voice->getAudio(voice->mResampleData[0]->mData, SAMPLE_GRANULARITY, SAMPLE_GRANULARITY);
							if (readcount < SAMPLE_GRANULARITY)
							{
								if (voice->mFlags & AudioSourceInstance::LOOPING)
								{
									while (readcount < SAMPLE_GRANULARITY && voice->seek(voice->mLoopPoint, mScratch.mData, mScratchSize) == SO_NO_ERROR)
									{
										voice->mLoopCount++;
										readcount += voice->getAudio(voice->mResampleData[0]->mData + readcount, SAMPLE_GRANULARITY - readcount, SAMPLE_GRANULARITY);
									}
								}
							}
						}

						// If we go past zero, crop to zero (a bit of a kludge)
						if (voice->mSrcOffset < SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL)
						{
							voice->mSrcOffset = 0;
						}
						else
						{
							// We have new block of data, move pointer backwards
							voice->mSrcOffset -= SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL;
						}

						// Skip filters
					}
					else
					{
						voice->mLeftoverSamples = 0;
					}

					// Figure out how many samples we can generate from this source data.
					// The value may be zero.

					unsigned int writesamples = 0;

					if (voice->mSrcOffset < SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL)
					{
						writesamples = ((SAMPLE_GRANULARITY * FIXPOINT_FRAC_MUL) - voice->mSrcOffset) / step_fixed + 1;

						// avoid reading past the current buffer..
						if (((writesamples * step_fixed + voice->mSrcOffset) >> FIXPOINT_FRAC_BITS) >= SAMPLE_GRANULARITY)
							writesamples--;
					}


					// If this is too much for our output buffer, don't write that many:
					if (writesamples + outofs > aSamplesToRead)
					{
						voice->mLeftoverSamples = (writesamples + outofs) - aSamplesToRead;
						writesamples = aSamplesToRead - outofs;
					}

					// Skip resampler

					// Keep track of how many samples we've written so far
					outofs += writesamples;

					// Move source pointer onwards (writesamples may be zero)
					voice->mSrcOffset += writesamples * step_fixed;
				}

				// clear voice if the sound is over
				if (!(voice->mFlags & AudioSourceInstance::LOOPING) && voice->hasEnded())
				{
					stopVoice(mActiveVoice[i]);
				}
			}
		}
	}

	void Soloud::mapResampleBuffers()
	{
		SOLOUD_ASSERT(mMaxActiveVoices < 256);
		char live[256];
		memset(live, 0, mMaxActiveVoices);
		unsigned int i, j;
		for (i = 0; i < mMaxActiveVoices; i++)
		{
			for (j = 0; j < mMaxActiveVoices; j++)
			{
				if (mResampleDataOwner[i] && mResampleDataOwner[i] == mVoice[mActiveVoice[j]])
				{
					live[i] |= 1; // Live channel
					live[j] |= 2; // Live voice
				}
			}
		}

		for (i = 0; i < mMaxActiveVoices; i++)
		{
			if (!(live[i] & 1) && mResampleDataOwner[i]) // For all dead channels with owners..
			{
				mResampleDataOwner[i]->mResampleData[0] = 0;
				mResampleDataOwner[i]->mResampleData[1] = 0;
				mResampleDataOwner[i] = 0;
			}
		}

		int latestfree = 0;
		for (i = 0; i < mActiveVoiceCount; i++)
		{
			if (!(live[i] & 2) && mVoice[mActiveVoice[i]]) // For all live voices with no channel..
			{
				int found = -1;
				for (j = latestfree; found == -1 && j < mMaxActiveVoices; j++)
				{
					if (mResampleDataOwner[j] == 0)
					{
						found = j;
					}
				}
				SOLOUD_ASSERT(found != -1);
				mResampleDataOwner[found] = mVoice[mActiveVoice[i]];
				mResampleDataOwner[found]->mResampleData[0] = &mResampleData[found * 2 + 0];
				mResampleDataOwner[found]->mResampleData[1] = &mResampleData[found * 2 + 1];
				mResampleDataOwner[found]->mResampleData[0]->clear();
				mResampleDataOwner[found]->mResampleData[1]->clear();
				latestfree = found + 1;
			}
		}
	}

	void Soloud::calcActiveVoices()
	{
		// TODO: consider whether we need to re-evaluate the active voices all the time.
		// It is a must when new voices are started, but otherwise we could get away
		// with postponing it sometimes..

		mActiveVoiceDirty = false;

		// Populate
		unsigned int i, candidates, mustlive;
		candidates = 0;
		mustlive = 0;
		for (i = 0; i < mHighestVoice; i++)
		{
			if (mVoice[i] && (!(mVoice[i]->mFlags & (AudioSourceInstance::INAUDIBLE | AudioSourceInstance::PAUSED)) || (mVoice[i]->mFlags & AudioSourceInstance::INAUDIBLE_TICK)))
			{
				mActiveVoice[candidates] = i;
				candidates++;
				if (mVoice[i]->mFlags & AudioSourceInstance::INAUDIBLE_TICK)
				{
					mActiveVoice[candidates - 1] = mActiveVoice[mustlive];
					mActiveVoice[mustlive] = i;
					mustlive++;
				}
			}
		}

		// Check for early out
		if (candidates <= mMaxActiveVoices)
		{
			// everything is audible, early out
			mActiveVoiceCount = candidates;
			mapResampleBuffers();
			return;
		}

		mActiveVoiceCount = mMaxActiveVoices;

		if (mustlive >= mMaxActiveVoices)
		{
			// Oopsie. Well, nothing to sort, since the "must live" voices already
			// ate all our active voice slots.
			// This is a potentially an error situation, but we have no way to report
			// error from here. And asserting could be bad, too.
			return;
		}

		// If we get this far, there's nothing to it: we'll have to sort the voices to find the most audible.

		// Iterative partial quicksort:
		int left = 0, stack[24], pos = 0, right;
		int len = candidates - mustlive;
		unsigned int *data = mActiveVoice + mustlive;
		int k = mActiveVoiceCount;
		for (;;)
		{
			for (; left + 1 < len; len++)
			{
				if (pos == 24) len = stack[pos = 0];
				int pivot = data[left];
				float pivotvol = mVoice[pivot]->mOverallVolume;
				stack[pos++] = len;
				for (right = left - 1;;)
				{
					do
					{
						right++;
					}
					while (mVoice[data[right]]->mOverallVolume > pivotvol);
					do
					{
						len--;
					}
					while (pivotvol > mVoice[data[len]]->mOverallVolume);
					if (right >= len) break;
					int temp = data[right];
					data[right] = data[len];
					data[len] = temp;
				}
			}
			if (pos == 0) break;
			if (left >= k) break;
			left = len;
			len = stack[--pos];
		}
		// TODO: should the rest of the voices be flagged INAUDIBLE?
		mapResampleBuffers();
	}

	void Soloud::mix_internal(unsigned int aSamples)
	{
#ifdef FLOATING_POINT_DEBUG
		// This needs to be done in the audio thread as well..
		static int done = 0;
		if (!done)
		{
			unsigned int u;
			u = _controlfp(0, 0);
			u = u & ~(_EM_INVALID | /*_EM_DENORMAL |*/ _EM_ZERODIVIDE | _EM_OVERFLOW /*| _EM_UNDERFLOW  | _EM_INEXACT*/);
			_controlfp(u, _MCW_EM);
			done = 1;
		}
#endif

		float buffertime = aSamples / (float)mSamplerate;
		float globalVolume[2];
		mStreamTime += buffertime;
		mLastClockedTime = 0;

		globalVolume[0] = mGlobalVolume;
		if (mGlobalVolumeFader.mActive)
		{
			mGlobalVolume = mGlobalVolumeFader.get(mStreamTime);
		}
		globalVolume[1] = mGlobalVolume;

		lockAudioMutex();

		// Process faders. May change scratch size.
		int i;
		for (i = 0; i < (signed)mHighestVoice; i++)
		{
			if (mVoice[i] && !(mVoice[i]->mFlags & AudioSourceInstance::PAUSED))
			{
				float volume[2];

				mVoice[i]->mActiveFader = 0;

				if (mGlobalVolumeFader.mActive > 0)
				{
					mVoice[i]->mActiveFader = 1;
				}

				mVoice[i]->mStreamTime += buffertime;
				mVoice[i]->mStreamPosition += buffertime;

				// TODO: this is actually unstable, because mStreamTime depends on the relative
				// play speed.
				if (mVoice[i]->mRelativePlaySpeedFader.mActive > 0)
				{
					float speed = mVoice[i]->mRelativePlaySpeedFader.get(mVoice[i]->mStreamTime);
					setVoiceRelativePlaySpeed(i, speed);
				}

				volume[0] = mVoice[i]->mOverallVolume;
				if (mVoice[i]->mVolumeFader.mActive > 0)
				{
					mVoice[i]->mSetVolume = mVoice[i]->mVolumeFader.get(mVoice[i]->mStreamTime);
					mVoice[i]->mActiveFader = 1;
					updateVoiceVolume(i);
					mActiveVoiceDirty = true;
				}
				volume[1] = mVoice[i]->mOverallVolume;

				if (mVoice[i]->mPanFader.mActive > 0)
				{
					float pan = mVoice[i]->mPanFader.get(mVoice[i]->mStreamTime);
					setVoicePan(i, pan);
					mVoice[i]->mActiveFader = 1;
				}

				if (mVoice[i]->mPauseScheduler.mActive)
				{
					mVoice[i]->mPauseScheduler.get(mVoice[i]->mStreamTime);
					if (mVoice[i]->mPauseScheduler.mActive == -1)
					{
						mVoice[i]->mPauseScheduler.mActive = 0;
						setVoicePause(i, 1);
					}
				}

				if (mVoice[i]->mStopScheduler.mActive)
				{
					mVoice[i]->mStopScheduler.get(mVoice[i]->mStreamTime);
					if (mVoice[i]->mStopScheduler.mActive == -1)
					{
						mVoice[i]->mStopScheduler.mActive = 0;
						stopVoice(i);
					}
				}
			}
		}

		if (mActiveVoiceDirty)
			calcActiveVoices();

		// Resize scratch if needed.
		if (mScratchSize < mScratchNeeded)
		{
			mScratchSize = mScratchNeeded;
			mScratch.init(mScratchSize * MAX_CHANNELS);
		}

		mixBus(mOutputScratch.mData, aSamples, aSamples, mScratch.mData, 0, (float)mSamplerate, mChannels);

		for (i = 0; i < FILTERS_PER_STREAM; i++)
		{
			if (mFilterInstance[i])
			{
				mFilterInstance[i]->filter(mOutputScratch.mData, aSamples, mChannels, (float)mSamplerate, mStreamTime);
			}
		}

		unlockAudioMutex();

		clip(mOutputScratch, mScratch, aSamples, globalVolume[0], globalVolume[1]);

		if (mFlags & ENABLE_VISUALIZATION)
		{
			for (i = 0; i < MAX_CHANNELS; i++)
			{
				mVisualizationChannelVolume[i] = 0;
			}
			if (aSamples > 255)
			{
				for (i = 0; i < 256; i++)
				{
					int j;
					mVisualizationWaveData[i] = 0;
					for (j = 0; j < (signed)mChannels; j++)
					{
						float sample = mScratch.mData[i + j * aSamples];
						float absvol = (float)fabs(sample);
						if (mVisualizationChannelVolume[j] < absvol)
							mVisualizationChannelVolume[j] = absvol;
						mVisualizationWaveData[i] += sample;
					}
				}
			}
			else
			{
				// Very unlikely failsafe branch
				for (i = 0; i < 256; i++)
				{
					int j;
					mVisualizationWaveData[i] = 0;
					for (j = 0; j < (signed)mChannels; j++)
					{
						float sample = mScratch.mData[(i % aSamples) + j * aSamples];
						float absvol = (float)fabs(sample);
						if (mVisualizationChannelVolume[j] < absvol)
							mVisualizationChannelVolume[j] = absvol;
						mVisualizationWaveData[i] += sample;
					}
				}
			}
		}
	}

	void Soloud::mix(float *aBuffer, unsigned int aSamples)
	{
		mix_internal(aSamples);
		interlace_samples_float(mScratch.mData, aBuffer, aSamples, mChannels);
	}

	void Soloud::mixSigned16(short *aBuffer, unsigned int aSamples)
	{
		mix_internal(aSamples);
		interlace_samples_s16(mScratch.mData, aBuffer, aSamples, mChannels);
	}

	void deinterlace_samples_float(const float *aSourceBuffer, float *aDestBuffer, unsigned int aSamples, unsigned int aChannels)
	{
		// 121212 -> 111222
		unsigned int i, j, c;
		c = 0;
		for (j = 0; j < aChannels; j++)
		{
			for (i = j; i < aSamples; i += aChannels)
			{
				aDestBuffer[c] = aSourceBuffer[i + j];
				c++;
			}
		}
	}

	void interlace_samples_float(const float *aSourceBuffer, float *aDestBuffer, unsigned int aSamples, unsigned int aChannels)
	{
		// 111222 -> 121212
		unsigned int i, j, c;
		c = 0;
		for (j = 0; j < aChannels; j++)
		{
			for (i = j; i < aSamples * aChannels; i += aChannels)
			{
				aDestBuffer[i] = aSourceBuffer[c];
				c++;
			}
		}
	}

	void interlace_samples_s16(const float *aSourceBuffer, short *aDestBuffer, unsigned int aSamples, unsigned int aChannels)
	{
		// 111222 -> 121212
		unsigned int i, j, c;
		c = 0;
		for (j = 0; j < aChannels; j++)
		{
			for (i = j; i < aSamples * aChannels; i += aChannels)
			{
				aDestBuffer[i] = (short)(aSourceBuffer[c] * 0x7fff);
				c++;
			}
		}
	}

	void Soloud::lockAudioMutex()
	{
		if (mAudioThreadMutex)
		{
			Thread::lockMutex(mAudioThreadMutex);
		}
		SOLOUD_ASSERT(!mInsideAudioThreadMutex);
		mInsideAudioThreadMutex = true;
	}

	void Soloud::unlockAudioMutex()
	{
		SOLOUD_ASSERT(mInsideAudioThreadMutex);
		mInsideAudioThreadMutex = false;
		if (mAudioThreadMutex)
		{
			Thread::unlockMutex(mAudioThreadMutex);
		}
	}

};
// file: soloud/src/core/soloud_core_setters.cpp
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

// #include "soloud_internal.h"

// Setters - set various bits of SoLoud state

namespace SoLoud
{
	void Soloud::setPostClipScaler(float aScaler)
	{
		mPostClipScaler = aScaler;
	}

	void Soloud::setGlobalVolume(float aVolume)
	{
		mGlobalVolumeFader.mActive = 0;
		mGlobalVolume = aVolume;
	}

	result Soloud::setRelativePlaySpeed(handle aVoiceHandle, float aSpeed)
	{
		result retVal = 0;
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mRelativePlaySpeedFader.mActive = 0;
			retVal = setVoiceRelativePlaySpeed(ch, aSpeed);
			FOR_ALL_VOICES_POST
		return retVal;
	}

	void Soloud::setSamplerate(handle aVoiceHandle, float aSamplerate)
	{
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mBaseSamplerate = aSamplerate;
			updateVoiceRelativePlaySpeed(ch);
		FOR_ALL_VOICES_POST
	}

	void Soloud::setPause(handle aVoiceHandle, bool aPause)
	{
		FOR_ALL_VOICES_PRE
			setVoicePause(ch, aPause);
		FOR_ALL_VOICES_POST
	}

	result Soloud::setMaxActiveVoiceCount(unsigned int aVoiceCount)
	{
		if (aVoiceCount == 0 || aVoiceCount >= VOICE_COUNT)
			return INVALID_PARAMETER;
		lockAudioMutex();
		mMaxActiveVoices = aVoiceCount;
		delete[] mResampleData;
		delete[] mResampleDataOwner;
		mResampleData = new AlignedFloatBuffer[aVoiceCount * 2];
		mResampleDataOwner = new AudioSourceInstance*[aVoiceCount];
		unsigned int i;
		for (i = 0; i < aVoiceCount * 2; i++)
			mResampleData[i].init(SAMPLE_GRANULARITY * MAX_CHANNELS);
		for (i = 0; i < aVoiceCount; i++)
			mResampleDataOwner[i] = NULL;
		mActiveVoiceDirty = true;
		unlockAudioMutex();
		return SO_NO_ERROR;
	}

	void Soloud::setPauseAll(bool aPause)
	{
		lockAudioMutex();
		int ch;
		for (ch = 0; ch < (signed)mHighestVoice; ch++)
		{
			setVoicePause(ch, aPause);
		}
		unlockAudioMutex();
	}

	void Soloud::setProtectVoice(handle aVoiceHandle, bool aProtect)
	{
		FOR_ALL_VOICES_PRE
			if (aProtect)
			{
				mVoice[ch]->mFlags |= AudioSourceInstance::PROTECTED;
			}
			else
			{
				mVoice[ch]->mFlags &= ~AudioSourceInstance::PROTECTED;
			}
		FOR_ALL_VOICES_POST
	}

	void Soloud::setPan(handle aVoiceHandle, float aPan)
	{
		FOR_ALL_VOICES_PRE
			setVoicePan(ch, aPan);
		FOR_ALL_VOICES_POST
	}

	void Soloud::setPanAbsolute(handle aVoiceHandle, float aLVolume, float aRVolume, float aLBVolume, float aRBVolume, float aCVolume, float aSVolume)
	{
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mPanFader.mActive = 0;
			mVoice[ch]->mChannelVolume[0] = aLVolume;
			mVoice[ch]->mChannelVolume[1] = aRVolume;
			if (mVoice[ch]->mChannels == 4)
			{
				mVoice[ch]->mChannelVolume[2] = aLBVolume;
				mVoice[ch]->mChannelVolume[3] = aRBVolume;
			}
			if (mVoice[ch]->mChannels == 6)
			{
				mVoice[ch]->mChannelVolume[2] = aCVolume;
				mVoice[ch]->mChannelVolume[3] = aSVolume;
				mVoice[ch]->mChannelVolume[4] = aLBVolume;
				mVoice[ch]->mChannelVolume[5] = aRBVolume;
			}
			if (mVoice[ch]->mChannels == 8)
			{
				mVoice[ch]->mChannelVolume[2] = aCVolume;
				mVoice[ch]->mChannelVolume[3] = aSVolume;
				mVoice[ch]->mChannelVolume[4] = (aLVolume + aLBVolume) * 0.5f;
				mVoice[ch]->mChannelVolume[5] = (aRVolume + aRBVolume) * 0.5f;
				mVoice[ch]->mChannelVolume[6] = aLBVolume;
				mVoice[ch]->mChannelVolume[7] = aRBVolume;
			}
			FOR_ALL_VOICES_POST
	}

	void Soloud::setInaudibleBehavior(handle aVoiceHandle, bool aMustTick, bool aKill)
	{
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mFlags &= ~(AudioSourceInstance::INAUDIBLE_KILL | AudioSourceInstance::INAUDIBLE_TICK);
			if (aMustTick)
			{
				mVoice[ch]->mFlags |= AudioSourceInstance::INAUDIBLE_TICK;
			}
			if (aKill)
			{
				mVoice[ch]->mFlags |= AudioSourceInstance::INAUDIBLE_KILL;
			}
		FOR_ALL_VOICES_POST
	}

	void Soloud::setLoopPoint(handle aVoiceHandle, time aLoopPoint)
	{
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mLoopPoint = aLoopPoint;
		FOR_ALL_VOICES_POST
	}

	void Soloud::setLooping(handle aVoiceHandle, bool aLooping)
	{
		FOR_ALL_VOICES_PRE
			if (aLooping)
			{
				mVoice[ch]->mFlags |= AudioSourceInstance::LOOPING;
			}
			else
			{
				mVoice[ch]->mFlags &= ~AudioSourceInstance::LOOPING;
			}
		FOR_ALL_VOICES_POST
	}


	void Soloud::setVolume(handle aVoiceHandle, float aVolume)
	{
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mVolumeFader.mActive = 0;
			setVoiceVolume(ch, aVolume);
		FOR_ALL_VOICES_POST
	}

	void Soloud::setDelaySamples(handle aVoiceHandle, unsigned int aSamples)
	{
		FOR_ALL_VOICES_PRE
			mVoice[ch]->mDelaySamples = aSamples;
		FOR_ALL_VOICES_POST
	}

	void Soloud::setVisualizationEnable(bool aEnable)
	{
		if (aEnable)
		{
			mFlags |= ENABLE_VISUALIZATION;
		}
		else
		{
			mFlags &= ~ENABLE_VISUALIZATION;
		}
	}

	result Soloud::setSpeakerPosition(unsigned int aChannel, float aX, float aY, float aZ)
	{
		if (aChannel >= mChannels)
			return INVALID_PARAMETER;
		m3dSpeakerPosition[3 * aChannel + 0] = aX;
		m3dSpeakerPosition[3 * aChannel + 1] = aY;
		m3dSpeakerPosition[3 * aChannel + 2] = aZ;
		return SO_NO_ERROR;
	}

}
// file: soloud/src/core/soloud_core_3d.cpp
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

#include <math.h>
// #include "soloud_internal.h"

// 3d audio operations

namespace SoLoud
{
	struct vec3
	{
		float mX, mY, mZ;

		bool null()
		{
			if (mX == 0 && mY == 0 && mZ == 0)
				return true;
			return false;
		}

		void neg()
		{
			mX = -mX;
			mY = -mY;
			mZ = -mZ;
		}

		float mag()
		{
			return (float)sqrt(mX * mX + mY * mY + mZ * mZ);
		}

		void normalize()
		{
			float m = mag();
			if (m == 0)
			{
				mX = mY = mZ = 0;
				return;
			}
			mX /= m;
			mY /= m;
			mZ /= m;
		}

		float dot(vec3 a)
		{
			return mX * a.mX + mY * a.mY + mZ * a.mZ;
		}

		vec3 sub(vec3 a)
		{
			vec3 r;
			r.mX = mX - a.mX;
			r.mY = mY - a.mY;
			r.mZ = mZ - a.mZ;
			return r;
		}

		vec3 cross(vec3 a)
		{
			vec3 r;

			r.mX = mY * a.mZ - a.mY * mZ;
			r.mY = mZ * a.mX - a.mZ * mX;
			r.mZ = mX * a.mY - a.mX * mY;

			return r;
		}
	};

	struct mat3
	{
		vec3 m[3];

		vec3 mul(vec3 a)
		{
			vec3 r;

			r.mX = m[0].mX * a.mX + m[0].mY * a.mY + m[0].mZ * a.mZ;
			r.mY = m[1].mX * a.mX + m[1].mY * a.mY + m[1].mZ * a.mZ;
			r.mZ = m[2].mX * a.mX + m[2].mY * a.mY + m[2].mZ * a.mZ;

			return r;
		}

		void lookatRH(vec3 at, vec3 up)
		{
			vec3 z = at;
			z.normalize();
			vec3 x = up.cross(z);
			x.normalize();
			vec3 y = z.cross(x);
			m[0] = x;
			m[1] = y;
			m[2] = z;
		}

		void lookatLH(vec3 at, vec3 up)
		{
			vec3 z = at;
			z.normalize();
			vec3 x = up.cross(z);
			x.normalize();
			vec3 y = z.cross(x);
			x.neg();  // flip x
			m[0] = x;
			m[1] = y;
			m[2] = z;
		}
	};

#ifndef MIN
#define MIN(a,b) ((a) < (b)) ? (a) : (b)
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b)) ? (a) : (b)
#endif

	float doppler(vec3 aDeltaPos, vec3 aSrcVel, vec3 aDstVel, float aFactor, float aSoundSpeed)
	{
		float deltamag = aDeltaPos.mag();
		if (deltamag == 0)
			return 1.0f;
		float vls = aDeltaPos.dot(aDstVel) / deltamag;
		float vss = aDeltaPos.dot(aSrcVel) / deltamag;
		float maxspeed = aSoundSpeed / aFactor;
		vss = MIN(vss, maxspeed);
		vls = MIN(vls, maxspeed);
		return (aSoundSpeed - aFactor * vls) / (aSoundSpeed - aFactor * vss);
	}

	float attenuateInvDistance(float aDistance, float aMinDistance, float aMaxDistance, float aRolloffFactor)
	{
		float distance = MAX(aDistance, aMinDistance);
		distance = MIN(distance, aMaxDistance);
		return aMinDistance / (aMinDistance + aRolloffFactor * (distance - aMinDistance));
	}

	float attenuateLinearDistance(float aDistance, float aMinDistance, float aMaxDistance, float aRolloffFactor)
	{
		float distance = MAX(aDistance, aMinDistance);
		distance = MIN(distance, aMaxDistance);
		return 1 - aRolloffFactor * (distance - aMinDistance) / (aMaxDistance - aMinDistance);
	}

	float attenuateExponentialDistance(float aDistance, float aMinDistance, float aMaxDistance, float aRolloffFactor)
	{
		float distance = MAX(aDistance, aMinDistance);
		distance = MIN(distance, aMaxDistance);
		return (float)pow(distance / aMinDistance, -aRolloffFactor);
	}

	void Soloud::update3dVoices(unsigned int *aVoiceArray, unsigned int aVoiceCount)
	{
		vec3 speaker[MAX_CHANNELS];

		int i;
		for (i = 0; i < (signed)mChannels; i++)
		{
			speaker[i].mX = m3dSpeakerPosition[3 * i + 0];
			speaker[i].mY = m3dSpeakerPosition[3 * i + 1];
			speaker[i].mZ = m3dSpeakerPosition[3 * i + 2];
			speaker[i].normalize();
		}
		for (; i < MAX_CHANNELS; i++)
		{
			speaker[i].mX = 0;
			speaker[i].mY = 0;
			speaker[i].mZ = 0;
		}

		vec3 lpos, lvel, at, up;
		at.mX = m3dAt[0];
		at.mY = m3dAt[1];
		at.mZ = m3dAt[2];
		up.mX = m3dUp[0];
		up.mY = m3dUp[1];
		up.mZ = m3dUp[2];
		lpos.mX = m3dPosition[0];
		lpos.mY = m3dPosition[1];
		lpos.mZ = m3dPosition[2];
		lvel.mX = m3dVelocity[0];
		lvel.mY = m3dVelocity[1];
		lvel.mZ = m3dVelocity[2];
		mat3 m;
		if (mFlags & LEFT_HANDED_3D)
		{
			m.lookatLH(at, up);
		}
		else
		{
			m.lookatRH(at, up);
		}

		for (i = 0; i < (signed)aVoiceCount; i++)
		{
			AudioSourceInstance3dData * v = &m3dData[aVoiceArray[i]];

			float vol = 1;

			// custom collider
			if (v->mCollider)
			{
				vol *= v->mCollider->collide(this, v, v->mColliderData);
			}

			vec3 pos, vel;
			pos.mX = v->m3dPosition[0];
			pos.mY = v->m3dPosition[1];
			pos.mZ = v->m3dPosition[2];

			vel.mX = v->m3dVelocity[0];
			vel.mY = v->m3dVelocity[1];
			vel.mZ = v->m3dVelocity[2];

			if (!(v->mFlags & AudioSourceInstance::LISTENER_RELATIVE))
			{
				pos = pos.sub(lpos);
			}

			float dist = pos.mag();

			// attenuation

			if (v->mAttenuator)
			{
				vol *= v->mAttenuator->attenuate(dist, v->m3dMinDistance, v->m3dMaxDistance, v->m3dAttenuationRolloff);
			}
			else
			{
				switch (v->m3dAttenuationModel)
				{
				case AudioSource::INVERSE_DISTANCE:
					vol *= attenuateInvDistance(dist, v->m3dMinDistance, v->m3dMaxDistance, v->m3dAttenuationRolloff);
					break;
				case AudioSource::LINEAR_DISTANCE:
					vol *= attenuateLinearDistance(dist, v->m3dMinDistance, v->m3dMaxDistance, v->m3dAttenuationRolloff);
					break;
				case AudioSource::EXPONENTIAL_DISTANCE:
					vol *= attenuateExponentialDistance(dist, v->m3dMinDistance, v->m3dMaxDistance, v->m3dAttenuationRolloff);
					break;
				default:
					//case AudioSource::NO_ATTENUATION:
					break;
				}
			}

			// cone

			// (todo) vol *= conev;

			// doppler
			v->mDopplerValue = doppler(pos, vel, lvel, v->m3dDopplerFactor, m3dSoundSpeed);

			// panning
			pos = m.mul(pos);
			pos.normalize();

			// Apply volume to channels based on speaker vectors
			int j;
			for (j = 0; j < (signed)mChannels; j++)
			{
				float speakervol = (speaker[j].dot(pos) + 1) / 2;
				if (speaker[j].null())
					speakervol = 1;
				// Different speaker "focus" calculations to try, if the default "bleeds" too much..
				//speakervol = (speakervol * speakervol + speakervol) / 2;
				//speakervol = speakervol * speakervol;
				v->mChannelVolume[j] = vol * speakervol;
			}
			for (; j < MAX_CHANNELS; j++)
			{
				v->mChannelVolume[j] = 0;
			}

			v->m3dVolume = vol;
		}
	}

	void Soloud::update3dAudio()
	{
		unsigned int voicecount = 0;
		unsigned int voices[VOICE_COUNT];

		// Step 1 - find voices that need 3d processing
		lockAudioMutex();
		int i;
		for (i = 0; i < (signed)mHighestVoice; i++)
		{
			if (mVoice[i] && mVoice[i]->mFlags & AudioSourceInstance::PROCESS_3D)
			{
				voices[voicecount] = i;
				voicecount++;
				m3dData[i].mFlags = mVoice[i]->mFlags;
			}
		}
		unlockAudioMutex();

		// Step 2 - do 3d processing

		update3dVoices(voices, voicecount);

		// Step 3 - update SoLoud voices

		lockAudioMutex();
		for (i = 0; i < (int)voicecount; i++)
		{
			AudioSourceInstance3dData * v = &m3dData[voices[i]];
			AudioSourceInstance * vi = mVoice[voices[i]];
			if (vi)
			{
				updateVoiceRelativePlaySpeed(voices[i]);
				updateVoiceVolume(voices[i]);
				int j;
				for (j = 0; j < MAX_CHANNELS; j++)
				{
					vi->mChannelVolume[j] = v->mChannelVolume[j];
				}

				if (vi->mOverallVolume < 0.01f)
				{
					// Inaudible.
					vi->mFlags |= AudioSourceInstance::INAUDIBLE;

					if (vi->mFlags & AudioSourceInstance::INAUDIBLE_KILL)
					{
						stopVoice(voices[i]);
					}
				}
				else
				{
					vi->mFlags &= ~AudioSourceInstance::INAUDIBLE;
				}
			}
		}

		mActiveVoiceDirty = true;
		unlockAudioMutex();
	}


	handle Soloud::play3d(AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX, float aVelY, float aVelZ, float aVolume, bool aPaused, unsigned int aBus)
	{
		handle h = play(aSound, aVolume, 0, 1, aBus);
		lockAudioMutex();
		int v = getVoiceFromHandle(h);
		if (v < 0)
		{
			unlockAudioMutex();
			return h;
		}
		m3dData[v].mHandle = h;
		mVoice[v]->mFlags |= AudioSourceInstance::PROCESS_3D;
		set3dSourceParameters(h, aPosX, aPosY, aPosZ, aVelX, aVelY, aVelZ);

		int samples = 0;
		if (aSound.mFlags & AudioSource::DISTANCE_DELAY)
		{
			vec3 pos;
			pos.mX = aPosX;
			pos.mY = aPosY;
			pos.mZ = aPosZ;
			if (!(mVoice[v]->mFlags & AudioSource::LISTENER_RELATIVE))
			{
				pos.mX -= m3dPosition[0];
				pos.mY -= m3dPosition[1];
				pos.mZ -= m3dPosition[2];
			}
			float dist = pos.mag();
			samples += (int)floor((dist / m3dSoundSpeed) * mSamplerate);
		}

		update3dVoices((unsigned int *)&v, 1);
		updateVoiceRelativePlaySpeed(v);
		int j;
		for (j = 0; j < MAX_CHANNELS; j++)
		{
			mVoice[v]->mChannelVolume[j] = m3dData[v].mChannelVolume[j];
		}

		updateVoiceVolume(v);

		// Fix initial voice volume ramp up
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			mVoice[v]->mCurrentChannelVolume[i] = mVoice[v]->mChannelVolume[i] * mVoice[v]->mOverallVolume;
		}

		if (mVoice[v]->mOverallVolume < 0.01f)
		{
			// Inaudible.
			mVoice[v]->mFlags |= AudioSourceInstance::INAUDIBLE;

			if (mVoice[v]->mFlags & AudioSourceInstance::INAUDIBLE_KILL)
			{
				stopVoice(v);
			}
		}
		else
		{
			mVoice[v]->mFlags &= ~AudioSourceInstance::INAUDIBLE;
		}
		mActiveVoiceDirty = true;

		unlockAudioMutex();
		setDelaySamples(h, samples);
		setPause(h, aPaused);
		return h;
	}

	handle Soloud::play3dClocked(time aSoundTime, AudioSource &aSound, float aPosX, float aPosY, float aPosZ, float aVelX, float aVelY, float aVelZ, float aVolume, unsigned int aBus)
	{
		handle h = play(aSound, aVolume, 0, 1, aBus);
		lockAudioMutex();
		int v = getVoiceFromHandle(h);
		if (v < 0)
		{
			unlockAudioMutex();
			return h;
		}
		m3dData[v].mHandle = h;
		mVoice[v]->mFlags |= AudioSourceInstance::PROCESS_3D;
		set3dSourceParameters(h, aPosX, aPosY, aPosZ, aVelX, aVelY, aVelZ);
		time lasttime = mLastClockedTime;
		if (lasttime == 0)
			mLastClockedTime = aSoundTime;
		vec3 pos;
		pos.mX = aPosX;
		pos.mY = aPosY;
		pos.mZ = aPosZ;
		unlockAudioMutex();
		int samples = 0;
		if (lasttime != 0)
		{
			samples = (int)floor((aSoundTime - lasttime) * mSamplerate);
		}
		if (aSound.mFlags & AudioSource::DISTANCE_DELAY)
		{
			float dist = pos.mag();
			samples += (int)floor((dist / m3dSoundSpeed) * mSamplerate);
		}

		update3dVoices((unsigned int *)&v, 1);
		lockAudioMutex();
		updateVoiceRelativePlaySpeed(v);
		int j;
		for (j = 0; j < MAX_CHANNELS; j++)
		{
			mVoice[v]->mChannelVolume[j] = m3dData[v].mChannelVolume[j];
		}

		updateVoiceVolume(v);

		// Fix initial voice volume ramp up
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			mVoice[v]->mCurrentChannelVolume[i] = mVoice[v]->mChannelVolume[i] * mVoice[v]->mOverallVolume;
		}

		if (mVoice[v]->mOverallVolume < 0.01f)
		{
			// Inaudible.
			mVoice[v]->mFlags |= AudioSourceInstance::INAUDIBLE;

			if (mVoice[v]->mFlags & AudioSourceInstance::INAUDIBLE_KILL)
			{
				stopVoice(v);
			}
		}
		else
		{
			mVoice[v]->mFlags &= ~AudioSourceInstance::INAUDIBLE;
		}
		mActiveVoiceDirty = true;
		unlockAudioMutex();

		setDelaySamples(h, samples);
		setPause(h, 0);
		return h;
	}



	result Soloud::set3dSoundSpeed(float aSpeed)
	{
		if (aSpeed <= 0)
			return INVALID_PARAMETER;
		m3dSoundSpeed = aSpeed;
		return SO_NO_ERROR;
	}


	float Soloud::get3dSoundSpeed()
	{
		return m3dSoundSpeed;
	}


	void Soloud::set3dListenerParameters(float aPosX, float aPosY, float aPosZ, float aAtX, float aAtY, float aAtZ, float aUpX, float aUpY, float aUpZ, float aVelocityX, float aVelocityY, float aVelocityZ)
	{
		m3dPosition[0] = aPosX;
		m3dPosition[1] = aPosY;
		m3dPosition[2] = aPosZ;
		m3dAt[0] = aAtX;
		m3dAt[1] = aAtY;
		m3dAt[2] = aAtZ;
		m3dUp[0] = aUpX;
		m3dUp[1] = aUpY;
		m3dUp[2] = aUpZ;
		m3dVelocity[0] = aVelocityX;
		m3dVelocity[1] = aVelocityY;
		m3dVelocity[2] = aVelocityZ;
	}


	void Soloud::set3dListenerPosition(float aPosX, float aPosY, float aPosZ)
	{
		m3dPosition[0] = aPosX;
		m3dPosition[1] = aPosY;
		m3dPosition[2] = aPosZ;
	}


	void Soloud::set3dListenerAt(float aAtX, float aAtY, float aAtZ)
	{
		m3dAt[0] = aAtX;
		m3dAt[1] = aAtY;
		m3dAt[2] = aAtZ;
	}


	void Soloud::set3dListenerUp(float aUpX, float aUpY, float aUpZ)
	{
		m3dUp[0] = aUpX;
		m3dUp[1] = aUpY;
		m3dUp[2] = aUpZ;
	}


	void Soloud::set3dListenerVelocity(float aVelocityX, float aVelocityY, float aVelocityZ)
	{
		m3dVelocity[0] = aVelocityX;
		m3dVelocity[1] = aVelocityY;
		m3dVelocity[2] = aVelocityZ;
	}


	void Soloud::set3dSourceParameters(handle aVoiceHandle, float aPosX, float aPosY, float aPosZ, float aVelocityX, float aVelocityY, float aVelocityZ)
	{
		FOR_ALL_VOICES_PRE_3D
			m3dData[ch].m3dPosition[0] = aPosX;
			m3dData[ch].m3dPosition[1] = aPosY;
			m3dData[ch].m3dPosition[2] = aPosZ;
			m3dData[ch].m3dVelocity[0] = aVelocityX;
			m3dData[ch].m3dVelocity[1] = aVelocityY;
			m3dData[ch].m3dVelocity[2] = aVelocityZ;
		FOR_ALL_VOICES_POST_3D
	}


	void Soloud::set3dSourcePosition(handle aVoiceHandle, float aPosX, float aPosY, float aPosZ)
	{
		FOR_ALL_VOICES_PRE_3D
			m3dData[ch].m3dPosition[0] = aPosX;
			m3dData[ch].m3dPosition[1] = aPosY;
			m3dData[ch].m3dPosition[2] = aPosZ;
		FOR_ALL_VOICES_POST_3D
	}


	void Soloud::set3dSourceVelocity(handle aVoiceHandle, float aVelocityX, float aVelocityY, float aVelocityZ)
	{
		FOR_ALL_VOICES_PRE_3D
			m3dData[ch].m3dVelocity[0] = aVelocityX;
			m3dData[ch].m3dVelocity[1] = aVelocityY;
			m3dData[ch].m3dVelocity[2] = aVelocityZ;
		FOR_ALL_VOICES_POST_3D
	}


	void Soloud::set3dSourceMinMaxDistance(handle aVoiceHandle, float aMinDistance, float aMaxDistance)
	{
		FOR_ALL_VOICES_PRE_3D
			m3dData[ch].m3dMinDistance = aMinDistance;
			m3dData[ch].m3dMaxDistance = aMaxDistance;
		FOR_ALL_VOICES_POST_3D
	}


	void Soloud::set3dSourceAttenuation(handle aVoiceHandle, unsigned int aAttenuationModel, float aAttenuationRolloffFactor)
	{
		FOR_ALL_VOICES_PRE_3D
			m3dData[ch].m3dAttenuationModel = aAttenuationModel;
			m3dData[ch].m3dAttenuationRolloff = aAttenuationRolloffFactor;
		FOR_ALL_VOICES_POST_3D
	}


	void Soloud::set3dSourceDopplerFactor(handle aVoiceHandle, float aDopplerFactor)
	{
		FOR_ALL_VOICES_PRE_3D
			m3dData[ch].m3dDopplerFactor = aDopplerFactor;
		FOR_ALL_VOICES_POST_3D
	}
};
// file: soloud/src/filter/soloud_dcremovalfilter.cpp
/*
SoLoud audio engine
Copyright (c) 2015 Jari Komppa

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

// #include "soloud.h"
// #include "soloud_dcremovalfilter.h"

namespace SoLoud
{
	DCRemovalFilterInstance::DCRemovalFilterInstance(DCRemovalFilter *aParent)
	{
		mParent = aParent;
		mBuffer = 0;
		mBufferLength = 0;
		mTotals = 0;
		mOffset = 0;
		initParams(1);

	}

	void DCRemovalFilterInstance::filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, double aTime)
	{
		updateParams(aTime);

		if (mBuffer == 0)
		{
			mBufferLength = (int)ceil(mParent->mLength * aSamplerate);
			mBuffer = new float[mBufferLength * aChannels];
			mTotals = new float[aChannels];
			unsigned int i;
			for (i = 0; i < aChannels; i++)
			{
			    mTotals[i] = 0;
			}
			for (i = 0; i < mBufferLength * aChannels; i++)
			{
				mBuffer[i] = 0;
			}
		}

		unsigned int i, j;
		int prevofs = (mOffset + mBufferLength - 1) % mBufferLength;
		for (i = 0; i < aSamples; i++)
		{
			for (j = 0; j < aChannels; j++)
			{
				int chofs = j * mBufferLength;
				int bchofs = j * aSamples;

				float n = aBuffer[i + bchofs];
				mTotals[j] -= mBuffer[mOffset + chofs];
				mTotals[j] += n;
				mBuffer[mOffset + chofs] = n;

			    n -= mTotals[j] / mBufferLength;

				aBuffer[i + bchofs] += (n - aBuffer[i + bchofs]) * mParam[0];
			}
			prevofs = mOffset;
			mOffset = (mOffset + 1) % mBufferLength;
		}
	}

	DCRemovalFilterInstance::~DCRemovalFilterInstance()
	{
		delete[] mBuffer;
		delete[] mTotals;
	}

	DCRemovalFilter::DCRemovalFilter()
	{
		mLength = 0.1f;
	}

	result DCRemovalFilter::setParams(float aLength)
	{
		if (aLength <= 0)
			return INVALID_PARAMETER;

        mLength = aLength;

		return 0;
	}


	FilterInstance *DCRemovalFilter::createInstance()
	{
		return new DCRemovalFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_fftfilter.cpp
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

#include <string.h>
// #include "soloud.h"
// #include "soloud_fftfilter.h"
// #include "soloud_fft.h"


namespace SoLoud
{
	FFTFilterInstance::FFTFilterInstance()
	{
		mParent = 0;
		mInputBuffer = 0;
		mMixBuffer = 0;
		mTemp = 0;
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
			mOffset[i] = 0;
	}

	FFTFilterInstance::FFTFilterInstance(FFTFilter *aParent)
	{
		mParent = aParent;
		mInputBuffer = 0;
		mMixBuffer = 0;
		mTemp = 0;
		int i;
		for (i = 0; i < MAX_CHANNELS; i++)
			mOffset[i] = 0;
		initParams(1);
	}

	void FFTFilterInstance::filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, double aTime, unsigned int aChannel, unsigned int aChannels)
	{
		if (aChannel == 0)
		{
			updateParams(aTime);
		}

		if (mInputBuffer == 0)
		{
			mInputBuffer = new float[512 * aChannels];
			mMixBuffer = new float[512 * aChannels];
			mTemp = new float[256];
			memset(mInputBuffer, 0x2f, sizeof(float) * 512 * aChannels);
			memset(mMixBuffer, 0, sizeof(float) * 512 * aChannels);
		}

		float * b = mTemp;

		int i;
		unsigned int ofs = 0;
		unsigned int chofs = 512 * aChannel;
		unsigned int bofs = mOffset[aChannel];

		while (ofs < aSamples)
		{
			for (i = 0; i < 128; i++)
			{
				mInputBuffer[chofs + ((bofs + i + 128) & 511)] = aBuffer[ofs + i];
				mMixBuffer[chofs + ((bofs + i + 128) & 511)] = 0;
			}

			for (i = 0; i < 256; i++)
			{
				b[i] = mInputBuffer[chofs + ((bofs + i) & 511)];
			}
			FFT::fft256(b);

			// do magic
			fftFilterChannel(b, 128, aSamplerate, aTime, aChannel, aChannels);

			FFT::ifft256(b);

			for (i = 0; i < 256; i++)
			{
				mMixBuffer[chofs + ((bofs + i) & 511)] += b[i] * (128 - abs(128 - i)) * (1.0f / 128.0f);
			}

			for (i = 0; i < 128; i++)
			{
				aBuffer[ofs + i] += (mMixBuffer[chofs + ((bofs + i) & 511)] - aBuffer[ofs + i]) * mParam[0];
			}
			ofs += 128;
			bofs += 128;
		}
		mOffset[aChannel] = bofs;
	}

	void FFTFilterInstance::fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels)
	{
		unsigned int i;
		for (i = 4; i < aSamples; i++)
		{
			aFFTBuffer[(i - 4) * 2] = aFFTBuffer[i * 2];
			aFFTBuffer[(i - 4) * 2 + 1] = aFFTBuffer[i * 2 + 1];
		}
		for (i = 0; i < 4; i++)
		{
			aFFTBuffer[aSamples - 4 * 2 + i * 2] = 0;
			aFFTBuffer[aSamples - 4 * 2 + i * 2 + 1] = 0;
		}
	}

	FFTFilterInstance::~FFTFilterInstance()
	{
		delete[] mTemp;
		delete[] mInputBuffer;
		delete[] mMixBuffer;
	}

	FFTFilter::FFTFilter()
	{
	}

	FilterInstance *FFTFilter::createInstance()
	{
		return new FFTFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_lofifilter.cpp
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

#include <math.h>
#include <string.h>
// #include "soloud.h"
// #include "soloud_lofifilter.h"

namespace SoLoud
{

	LofiFilterInstance::LofiFilterInstance(LofiFilter *aParent)
	{
		mParent = aParent;
		initParams(3);
		mParam[SAMPLERATE] = aParent->mSampleRate;
		mParam[BITDEPTH] = aParent->mBitdepth;
		mChannelData[0].mSample = 0;
		mChannelData[0].mSamplesToSkip = 0;
		mChannelData[1].mSample = 0;
		mChannelData[1].mSamplesToSkip = 0;
	}

	void LofiFilterInstance::filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, double aTime, unsigned int aChannel, unsigned int aChannels)
	{
		updateParams(aTime);

		unsigned int i;
		for (i = 0; i < aSamples; i++)
		{
			if (mChannelData[aChannel].mSamplesToSkip <= 0)
			{
				mChannelData[aChannel].mSamplesToSkip += (aSamplerate / mParam[SAMPLERATE]) - 1;
				float q = (float)pow(2, mParam[BITDEPTH]);
				mChannelData[aChannel].mSample = (float)floor(q*aBuffer[i])/q;
			}
			else
			{
				mChannelData[aChannel].mSamplesToSkip--;
			}
			aBuffer[i] += (mChannelData[aChannel].mSample - aBuffer[i]) * mParam[WET];
		}

	}

	LofiFilterInstance::~LofiFilterInstance()
	{
	}

	LofiFilter::LofiFilter()
	{
		setParams(4000, 3);
	}

	result LofiFilter::setParams(float aSampleRate, float aBitdepth)
	{
		if (aSampleRate <= 0 || aBitdepth <= 0)
			return INVALID_PARAMETER;

		mSampleRate = aSampleRate;
		mBitdepth = aBitdepth;
		return 0;
	}

	LofiFilter::~LofiFilter()
	{
	}


	LofiFilterInstance *LofiFilter::createInstance()
	{
		return new LofiFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_flangerfilter.cpp
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
#include <string.h>
// #include "soloud.h"
// #include "soloud_flangerfilter.h"

namespace SoLoud
{
	FlangerFilterInstance::FlangerFilterInstance(FlangerFilter *aParent)
	{
		mParent = aParent;
		mBuffer = 0;
		mBufferLength = 0;
		mOffset = 0;
		mIndex = 0;
		initParams(3);
		mParam[FlangerFilter::WET] = 1;
		mParam[FlangerFilter::FREQ] = mParent->mFreq;
		mParam[FlangerFilter::DELAY] = mParent->mDelay;
	}

	void FlangerFilterInstance::filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, double aTime)
	{
		updateParams(aTime);

		if (mBufferLength < mParam[FlangerFilter::DELAY] * aSamplerate)
		{
			delete[] mBuffer;
			mBufferLength = (int)ceil(mParam[FlangerFilter::DELAY] * aSamplerate);
			mBuffer = new float[mBufferLength * aChannels];
			if (mBuffer == NULL)
			{
				mBufferLength = 0;
				return;
			}
			memset(mBuffer, 0, sizeof(float) * mBufferLength * aChannels);
		}

		unsigned int i, j;
		int maxsamples = (int)ceil(mParam[FlangerFilter::DELAY] * aSamplerate);
		double inc = mParam[FlangerFilter::FREQ] * M_PI * 2 / aSamplerate;
		for (i = 0; i < aChannels; i++)
		{
			int mbofs = i * mBufferLength;
			int abofs = i * aSamples;
			for (j = 0; j < aSamples; j++, abofs++)
			{
				int delay = (int)floor(maxsamples * (1 + cos(mIndex))) / 2;
				mIndex += inc;
				mBuffer[mbofs + mOffset % mBufferLength] = aBuffer[abofs];
				float n = 0.5f * (aBuffer[abofs] + mBuffer[mbofs + (mBufferLength - delay + mOffset) % mBufferLength]);
				mOffset++;
				aBuffer[abofs] += (n - aBuffer[abofs]) * mParam[FlangerFilter::WET];
			}
			mOffset -= aSamples;
		}
		mOffset += aSamples;
		mOffset %= mBufferLength;
	}

	FlangerFilterInstance::~FlangerFilterInstance()
	{
		delete[] mBuffer;
	}

	FlangerFilter::FlangerFilter()
	{
		mDelay = 0.005f;
		mFreq = 10;
	}

	result FlangerFilter::setParams(float aDelay, float aFreq)
	{
		if (aDelay <= 0 || aFreq <= 0)
			return INVALID_PARAMETER;

		mDelay = aDelay;
		mFreq = aFreq;

		return 0;
	}


	FilterInstance *FlangerFilter::createInstance()
	{
		return new FlangerFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_biquadresonantfilter.cpp
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

--

Based on "Using the Biquad Resonant Filter",
Phil Burk, Game Programming Gems 3, p. 606
*/

#include <math.h>
#include <string.h>
// #include "soloud.h"
// #include "soloud_biquadresonantfilter.h"

namespace SoLoud
{
	void BiquadResonantFilterInstance::calcBQRParams()
	{
		mDirty = 0;

		float omega = (float)((2.0f * M_PI * mParam[FREQUENCY]) / mParam[SAMPLERATE]);
		float sin_omega = (float)sin(omega);
		float cos_omega = (float)cos(omega);
		float alpha = sin_omega / (2.0f * mParam[RESONANCE]);
		float scalar = 1.0f / (1.0f + alpha);

		mActive = 1;

		switch (mFilterType)
		{
		case BiquadResonantFilter::NONE:
			mActive = 0;
			break;
		case BiquadResonantFilter::LOWPASS:
			mA0 = 0.5f * (1.0f - cos_omega) * scalar;
			mA1 = (1.0f - cos_omega) * scalar;
			mA2 = mA0;
			mB1 = -2.0f * cos_omega * scalar;
			mB2 = (1.0f - alpha) * scalar;
			break;
		case BiquadResonantFilter::HIGHPASS:
			mA0 = 0.5f * (1.0f + cos_omega) * scalar;
			mA1 = -(1.0f + cos_omega) * scalar;
			mA2 = mA0;
			mB1 = -2.0f * cos_omega * scalar;
			mB2 = (1.0f - alpha) * scalar;
			break;
		case BiquadResonantFilter::BANDPASS:
			mA0 = alpha * scalar;
			mA1 = 0;
			mA2 = -mA0;
			mB1 = -2.0f * cos_omega * scalar;
			mB2 = (1.0f - alpha) * scalar;
			break;
		}
	}


	BiquadResonantFilterInstance::BiquadResonantFilterInstance(BiquadResonantFilter *aParent)
	{
		int i;
		for (i = 0; i < 2; i++)
		{
			mState[i].mX1 = 0;
			mState[i].mY1 = 0;
			mState[i].mX2 = 0;
			mState[i].mY2 = 0;
		}

		mParent = aParent;
		mFilterType = aParent->mFilterType;

		initParams(4);

		mParam[SAMPLERATE] = aParent->mSampleRate;
		mParam[RESONANCE] = aParent->mResonance;
		mParam[FREQUENCY] = aParent->mFrequency;
		mParam[WET] = 1;

		calcBQRParams();
	}

	void BiquadResonantFilterInstance::filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, double aTime, unsigned int aChannel, unsigned int aChannels)
	{
		if (!mActive)
			return;

		if (aChannel == 0)
		{
			updateParams(aTime);

			if (mParamChanged & ((1 << FREQUENCY) | (1 << RESONANCE) | (1 << SAMPLERATE)))
			{
				calcBQRParams();
			}
			mParamChanged = 0;
		}

		float x;
		unsigned int i;
		int c = 0;

		BQRStateData &s = mState[aChannel];

		for (i = 0; i < aSamples; i +=2, c++)
		{
			// Generate outputs by filtering inputs.
			x = aBuffer[c];
			s.mY2 = (mA0 * x) + (mA1 * s.mX1) + (mA2 * s.mX2) - (mB1 * s.mY1) - (mB2 * s.mY2);
			aBuffer[c] += (s.mY2 - aBuffer[c]) * mParam[WET];

			c++;

			// Permute filter operations to reduce data movement.
			// Just substitute variables instead of doing mX1=x, etc.
			s.mX2 = aBuffer[c];
			s.mY1 = (mA0 * s.mX2) + (mA1 * x) + (mA2 * s.mX1) - (mB1 * s.mY2) - (mB2 * s.mY1);
			aBuffer[c] += (s.mY1 - aBuffer[c]) * mParam[WET];

			// Only move a little data.
			s.mX1 = s.mX2;
			s.mX2 = x;
		}

		// Apply a small impulse to filter to prevent arithmetic underflow,
		// which can cause the FPU to interrupt the CPU.
		s.mY1 += (float) 1.0E-26;
	}


	BiquadResonantFilterInstance::~BiquadResonantFilterInstance()
	{
	}

	BiquadResonantFilter::BiquadResonantFilter()
	{
		setParams(LOWPASS, 44100, 1000, 2);
	}

	result BiquadResonantFilter::setParams(int aType, float aSampleRate, float aFrequency, float aResonance)
	{
		if (aType < 0 || aType > 3 || aSampleRate <= 0 || aFrequency <= 0 || aResonance <= 0)
			return INVALID_PARAMETER;

		mFilterType = aType;
		mSampleRate = aSampleRate;
		mFrequency = aFrequency;
		mResonance = aResonance;

		return 0;
	}

	BiquadResonantFilter::~BiquadResonantFilter()
	{
	}


	BiquadResonantFilterInstance *BiquadResonantFilter::createInstance()
	{
		return new BiquadResonantFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_bassboostfilter.cpp
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

#include <string.h>
// #include "soloud.h"
// #include "soloud_bassboostfilter.h"


namespace SoLoud
{
	BassboostFilterInstance::BassboostFilterInstance(BassboostFilter *aParent)
	{
		mParent = aParent;
		initParams(2);
		mParam[BOOST] = aParent->mBoost;
	}

	void BassboostFilterInstance::fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels)
	{
		unsigned int i;
		for (i = 0; i < 2; i++)
		{
			aFFTBuffer[i * 2] *= mParam[BOOST];
		}
	}

	result BassboostFilter::setParams(float aBoost)
	{
		if (aBoost < 0)
			return INVALID_PARAMETER;
		mBoost = aBoost;
		return SO_NO_ERROR;
	}

	BassboostFilter::BassboostFilter()
	{
		mBoost = 2;
	}

	FilterInstance *BassboostFilter::createInstance()
	{
		return new BassboostFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_echofilter.cpp
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

// #include "soloud.h"
// #include "soloud_echofilter.h"

namespace SoLoud
{
	EchoFilterInstance::EchoFilterInstance(EchoFilter *aParent)
	{
		mParent = aParent;
		mBuffer = 0;
		mBufferLength = 0;
		mOffset = 0;
		initParams(1);

	}

	void EchoFilterInstance::filter(float *aBuffer, unsigned int aSamples, unsigned int aChannels, float aSamplerate, double aTime)
	{
		updateParams(aTime);

		if (mBuffer == 0)
		{
			mBufferLength = (int)ceil(mParent->mDelay * aSamplerate);
			mBuffer = new float[mBufferLength * aChannels];
			unsigned int i;
			for (i = 0; i < mBufferLength * aChannels; i++)
			{
				mBuffer[i] = 0;
			}
		}

		float decay = mParent->mDecay;
		unsigned int i, j;
		int prevofs = (mOffset + mBufferLength - 1) % mBufferLength;
		for (i = 0; i < aSamples; i++)
		{
			for (j = 0; j < aChannels; j++)
			{
				int chofs = j * mBufferLength;
				int bchofs = j * aSamples;

				mBuffer[mOffset + chofs] = mParent->mFilter * mBuffer[prevofs + chofs] + (1 - mParent->mFilter) * mBuffer[mOffset + chofs];

				float n = aBuffer[i + bchofs] + mBuffer[mOffset + chofs] * decay;
				mBuffer[mOffset + chofs] = n;

				aBuffer[i + bchofs] += (n - aBuffer[i + bchofs]) * mParam[0];
			}
			prevofs = mOffset;
			mOffset = (mOffset + 1) % mBufferLength;
		}
	}

	EchoFilterInstance::~EchoFilterInstance()
	{
		delete[] mBuffer;
	}

	EchoFilter::EchoFilter()
	{
		mDelay = 0.3f;
		mDecay = 0.7f;
		mFilter = 0.0f;
	}

	result EchoFilter::setParams(float aDelay, float aDecay, float aFilter)
	{
		if (aDelay <= 0 || aDecay <= 0 || aFilter < 0 || aFilter >= 1.0f)
			return INVALID_PARAMETER;

		mDecay = aDecay;
		mDelay = aDelay;
		mFilter = aFilter;

		return 0;
	}


	FilterInstance *EchoFilter::createInstance()
	{
		return new EchoFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_robotizefilter.cpp
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

#include <string.h>
// #include "soloud.h"
// #include "soloud_robotizefilter.h"


namespace SoLoud
{
	RobotizeFilterInstance::RobotizeFilterInstance(RobotizeFilter *aParent)
	{
		mParent = aParent;
		initParams(1);
		mParam[WET] = 1.0;
	}

	void RobotizeFilterInstance::fftFilterChannel(float *aFFTBuffer, unsigned int aSamples, float aSamplerate, time aTime, unsigned int aChannel, unsigned int aChannels)
	{
		unsigned int i;
		for (i = 0; i < aSamples; i++)
		{
			aFFTBuffer[i*2] = 0;
		}
	}

	RobotizeFilter::RobotizeFilter()
	{
	}

	FilterInstance *RobotizeFilter::createInstance()
	{
		return new RobotizeFilterInstance(this);
	}
}
// file: soloud/src/filter/soloud_waveshaperfilter.cpp
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

#include <math.h>
#include <string.h>
// #include "soloud.h"
// #include "soloud_waveshaperfilter.h"

namespace SoLoud
{

	WaveShaperFilterInstance::WaveShaperFilterInstance(WaveShaperFilter *aParent)
	{
		mParent = aParent;
		initParams(2);
		mParam[0] = mParent->mWet;
		mParam[1] = mParent->mAmount;
	}

	void WaveShaperFilterInstance::filterChannel(float *aBuffer, unsigned int aSamples, float aSamplerate, double aTime, unsigned int aChannel, unsigned int aChannels)
	{
		updateParams(aTime);

		unsigned int i;
		float k = 0;
		if (mParam[1] == 1)
			k = 2 * mParam[1] / 0.01f;
		else
			k = 2 * mParam[1] / (1 - mParam[1]);

		for (i = 0; i < aSamples; i++)
		{
			float dry = aBuffer[i];
			float wet = (1 + k) * aBuffer[i] / (1 + k * (float)fabs(aBuffer[i]));
			aBuffer[i] += (wet - dry) * mParam[0];
		}
	}

	WaveShaperFilterInstance::~WaveShaperFilterInstance()
	{
	}

	result WaveShaperFilter::setParams(float aAmount, float aWet)
	{
		if (aAmount < -1 || aAmount > 1 || aWet < 0 || aWet > 1)
			return INVALID_PARAMETER;
		mAmount = aAmount;
		mWet = aWet;
		return 0;
	}

	WaveShaperFilter::WaveShaperFilter()
	{
		mAmount = 0.0f;
		mWet = 0.0f;
	}

	WaveShaperFilter::~WaveShaperFilter()
	{
	}

	WaveShaperFilterInstance *WaveShaperFilter::createInstance()
	{
		return new WaveShaperFilterInstance(this);
	}
}
// file: patches/src/soloud_midi.cpp
#include <stdlib.h>
#include <stdio.h>

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
// #include "tsf.h"
#define TML_IMPLEMENTATION
#define TML_NO_STDIO
// #include "tml.h"

// #include "soloud_midi.h"
// #include "soloud_file.h"

namespace SoLoud
{
	int MidiInstance::tick(float *stream, int SampleCount)
	{
		tml_message *mf = (tml_message *)mTrack;
		tsf *sf = (tsf *)mParent->mSoundFont->mHandle;

		float *begin = stream;
		int SampleBlock = 0;
		for (SampleBlock = 64; SampleCount; SampleCount -= SampleBlock, stream += SampleBlock)
		{
			if (SampleBlock > SampleCount)
				SampleBlock = SampleCount;

			for (mMsec += SampleBlock * (1000.0 / 44100.0); mf && mMsec >= mf->time; mf = mf->next)
			{
				switch (mf->type)
				{
					case TML_PROGRAM_CHANGE:
						tsf_channel_set_presetnumber(sf, mf->channel, mf->program, (mf->channel == 9));
						break;
					case TML_NOTE_ON:
						tsf_channel_note_on(sf, mf->channel, mf->key, mf->velocity / 127.0f);
						break;
					case TML_NOTE_OFF:
						tsf_channel_note_off(sf, mf->channel, mf->key);
						break;
					case TML_PITCH_BEND:
						tsf_channel_set_pitchwheel(sf, mf->channel, mf->pitch_bend);
						break;
					case TML_CONTROL_CHANGE:
						tsf_channel_midi_control(sf, mf->channel, mf->control, mf->control_value);
						break;
				}
			}
			mTrack = mf;
			tsf_render_float(sf, stream, SampleBlock, 0);
		}
		return stream - begin;
	}

	MidiInstance::MidiInstance(Midi *aParent)
	{
		mParent = aParent;
		mParent->mSoundFont->mHandle = tsf_load_memory((const void*)mParent->mSoundFont->mData, mParent->mSoundFont->mDataLen);
		mParent->mHandle = tml_load_memory((const void*)mParent->mData, mParent->mDataLen);

		tsf_channel_set_bank_preset((tsf *)mParent->mSoundFont->mHandle, 9, 128, 0);
		tsf_set_output((tsf *)mParent->mSoundFont->mHandle, TSF_STEREO_UNWEAVED, 44100, 5.0f);

		mTrack = mParent->mHandle;
		mPlaying = mTrack != NULL;
	}

	unsigned int MidiInstance::getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int)
	{
		if (mParent->mHandle == NULL || mParent->mSoundFont->mHandle == NULL || mTrack == NULL)
			return 0;

		aSamplesToRead = tick(aBuffer, aSamplesToRead);
		mPlaying = mTrack != NULL;

		return aSamplesToRead;
	}

	void MidiInstance::seek(float aSeconds, float *mScratch, int mScratchSize)
	{
		mMsec = 0;
		double targetSec = aSeconds;
		tml_message *mf = (tml_message *)mParent->mHandle;
		for (mMsec += 8 * (1000.0 / 44100.0); mf && mMsec < targetSec && mMsec <= mf->time; mf = mf->next)
		{
			;
		}
		mTrack = mf;
	}

	unsigned int MidiInstance::rewind()
	{
		mTrack = (tml_message *)mParent->mHandle;
		mMsec = 0;
		return 0;
	}

	bool MidiInstance::hasEnded()
	{
		return !mPlaying;
	}

	MidiInstance::~MidiInstance()
	{
		if (mParent->mData)
		{
			tml_free((tml_message *)mParent->mData);
		}
		mParent->mData = 0;
	}

	result SoundFont::loadMem(unsigned char *aMem, unsigned int aLength, bool aCopy, bool aTakeOwnership)
	{
		MemoryFile f;
		int res = f.openMem(aMem, aLength, aCopy, aTakeOwnership);
		if (res != SO_NO_ERROR)
			return res;

		return loadFile(&f);
	}

	result SoundFont::load(const char *aFilename)
	{
		DiskFile f;
		int res = f.open(aFilename);
		if (res != SO_NO_ERROR)
			return res;

		return loadFile(&f);
	}

	result SoundFont::loadFile(File *aFile)
	{
		if (mData)
		{
			delete[] mData;
		}

		mDataLen = aFile->length();
		mData = new char[mDataLen];
		if (!mData)
		{
			mData = 0;
			mDataLen = 0;
			return OUT_OF_MEMORY;
		}
		aFile->read((unsigned char*)mData, mDataLen);
/*
		mHandle = tsf_load_memory((const void*)mData, mDataLen);
		if (!mHandle)
		{
			delete[] mData;
			mDataLen = 0;
			return FILE_LOAD_FAILED;
		}
		tsf_channel_set_bank_preset((tsf *)mHandle, 9, 128, 0);
		tsf_set_output((tsf *)mHandle, TSF_STEREO_UNWEAVED, 44100, 5.0f);*/
		return 0;
	}

	SoundFont::SoundFont()
	{
		mData = 0;
		mDataLen = 0;
	}

	SoundFont::~SoundFont()
	{
		tsf_close((tsf *)mHandle);
		delete[] mData;
		mData = 0;
		mDataLen = 0;
	}

	result Midi::loadMem(unsigned char *aMem, unsigned int aLength, SoundFont &sf, bool aCopy, bool aTakeOwnership)
	{
		MemoryFile f;
		int res = f.openMem(aMem, aLength, aCopy, aTakeOwnership);
		if (res != SO_NO_ERROR)
			return res;

		return loadFile(&f, sf);
	}

	result Midi::load(const char *aFilename, SoundFont &sf)
	{
		DiskFile f;
		int res = f.open(aFilename);
		if (res != SO_NO_ERROR)
			return res;

		return loadFile(&f, sf);
	}

	result Midi::loadFile(File *aFile, SoundFont &sf)
	{
		if (mData)
		{
			delete[] mData;
		}

		mDataLen = aFile->length();
		mData = new char[mDataLen];
		if (!mData)
		{
			mData = 0;
			mDataLen = 0;
			return OUT_OF_MEMORY;
		}
		aFile->read((unsigned char*)mData, mDataLen);
/*
		mHandle = tml_load_memory((const void*)mData, mDataLen);
		if (!mHandle)
		{
			delete[] mData;
			mDataLen = 0;
			return FILE_LOAD_FAILED;
		}*/

		mSoundFont = &sf;
		return 0;
	}

	Midi::Midi()
	{
		mBaseSamplerate = 44100;
		mChannels = 2;
		mData = 0;
		mDataLen = 0;
		mSoundFont = 0;
	}

	Midi::~Midi()
	{
		stop();
		delete[] mData;
		mData = 0;
		mDataLen = 0;
		mSoundFont = 0;
	}

	AudioSourceInstance * Midi::createInstance()
	{
		return new MidiInstance(this);
	}

};

