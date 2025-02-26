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

#include "soloud.h"

#if !defined(WITH_SDL3)

namespace SoLoud
{
	result sdl3_init(
		SoLoud::Soloud * aSoloud,
		unsigned int aFlags,
		unsigned int aSamplerate,
		unsigned int aBuffer,
		unsigned int aChannels,
		int deviceId
	)
	{
		return NOT_IMPLEMENTED;
	}
}

#else

#include "SDL3/SDL.h"
#include <math.h>

extern "C"
{
	int dll_SDL3_found();

	Uint32 dll_SDL3_WasInit(Uint32 flags);
	int dll_SDL3_InitSubSystem(Uint32 flags);
	SDL_AudioStream * dll_SDL3_OpenAudioDeviceStream(
		SDL_AudioDeviceID device,
		SDL_AudioSpec const * spec,
		SDL_AudioStreamCallback callback,
		void * userdata
	);
	void dll_SDL3_CloseAudioDevice(SDL_AudioDeviceID dev);
	bool dll_SDL3_PauseAudioStreamDevice(SDL_AudioStream * dev);
};

namespace SoLoud
{
	static SDL_AudioSpec gActiveAudioSpec;
	static SDL_AudioStream * gAudioStream;
	static SDL_AudioDeviceID gAudioDeviceId;

	void soloud_sdl2_audiomixer(Uint8 * stream, void * userdata, int len)
	{
		SoLoud::Soloud *soloud = (SoLoud::Soloud *)userdata;
		if (gActiveAudioSpec.format == SDL_AUDIO_F32)
		{
			int samples = len / (gActiveAudioSpec.channels * sizeof(float));
			soloud->mix((float *)stream, samples);
		}
		else // assume s16 if not float
		{
			int samples = len / (gActiveAudioSpec.channels * sizeof(short));
			soloud->mixSigned16((short *)stream, samples);
		}
	}

	void soloud_sdl3_audiomixer(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount)
	{
		SoLoud::Soloud *soloud = (SoLoud::Soloud *)userdata;

		if (additionalAmount > 0)
		{
			Uint8 * data = SDL_stack_alloc(Uint8, additionalAmount);
			if (data)
			{
				soloud_sdl2_audiomixer(
					data,
					userdata,
					additionalAmount
				);
				SDL_PutAudioStreamData(stream, data, additionalAmount);
				SDL_stack_free(data);
			}
		}
	}

	static void soloud_sdl3_deinit(SoLoud::Soloud * /*aSoloud*/)
	{
		dll_SDL3_CloseAudioDevice(gAudioDeviceId);
	}

	result sdl3_init(
		SoLoud::Soloud *aSoloud,
		unsigned int aFlags,
		unsigned int aSamplerate,
		unsigned int aBuffer,
		unsigned int aChannels,
		unsigned int deviceId
	)
	{
		if (!dll_SDL3_found())
			return DLL_NOT_FOUND;

		if (!dll_SDL3_WasInit(SDL_INIT_AUDIO))
		{
			if (dll_SDL3_InitSubSystem(SDL_INIT_AUDIO) < 0)
			{
				return UNKNOWN_ERROR;
			}
		}

		SDL_AudioSpec as{};
		as.freq = aSamplerate;
		as.format = SDL_AUDIO_F32;
		as.channels = (Uint8)aChannels;

		auto * audioStream = dll_SDL3_OpenAudioDeviceStream(
			deviceId,
			&as,
			soloud_sdl3_audiomixer,
			static_cast<void *>(aSoloud)
		);

		auto const deviceIdFromStream = SDL_GetAudioStreamDevice(audioStream);

		gAudioDeviceId = deviceIdFromStream;

		if (gAudioDeviceId == NULL)
		{
			as.format = SDL_AUDIO_S16;
			audioStream = dll_SDL3_OpenAudioDeviceStream(
				deviceId,
				NULL,
				soloud_sdl3_audiomixer,
				static_cast<void *>(aSoloud)
			);

			gAudioDeviceId = SDL_GetAudioStreamDevice(audioStream);

			if (gAudioDeviceId == NULL)
			{
				return UNKNOWN_ERROR;
			}
		}

		SDL_GetAudioDeviceFormat(
			gAudioDeviceId,
			&gActiveAudioSpec,
			NULL
		);

		aSoloud->setDeviceId(gAudioDeviceId);

		aSoloud->postinit_internal(
			gActiveAudioSpec.freq,
			aBuffer,
			aFlags,
			gActiveAudioSpec.channels
		);

		aSoloud->mBackendCleanupFunc = soloud_sdl3_deinit;

		dll_SDL3_PauseAudioStreamDevice(gAudioStream);
		aSoloud->mBackendString = "SDL3 (dynamic)";
		return 0;
	}
};
#endif
