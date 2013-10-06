/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
//#include <sys/shm.h>
#include <sys/wait.h>
#ifdef __linux__ // rb0101023 - guard this
#include <linux/soundcard.h>
#endif
#ifdef __FreeBSD__ // rb0101023 - added
#include <sys/soundcard.h>
#endif
#include <stdio.h>

#include "../game/q_shared.h"
#include "../client/snd_local.h"

//Updated by Emile Belanger for OpenSL

// for native audio
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>

pthread_mutex_t dma_mutex;

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
#ifdef ANDROID_NDK
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
#else
static SLBufferQueueItf bqPlayerBufferQueue;
#endif

static SLEffectSendItf bqPlayerEffectSend;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

void myassert(int v,const char * message)
{
	if (!v)
		Com_Printf("myassert: %s",message);
}

int audio_fd;
int snd_inited = 0;

cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;

cvar_t *snddevice;

/* Some devices may work only with 48000 */
static int tryrates[] = { 22050, 11025, 44100, 48000, 8000 };

static int dmapos = 0;
static int dmasize = 0;

#define OPENSL_BUFF_LEN 1024

static unsigned char play_buffer[OPENSL_BUFF_LEN];

//NOTE!! There are definetly threading issues with this, but it appears to work for now...

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
	//LOGI("bqPlayerCallback");
	pthread_mutex_lock(&dma_mutex);

	int pos = (dmapos * (dma.samplebits/8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	int len = OPENSL_BUFF_LEN;
	int factor = 2*2;
	int FrameCount = (unsigned int)OPENSL_BUFF_LEN / factor;

	if (!snd_inited)  /* shouldn't happen, but just in case... */
	{
		memset(play_buffer, '\0', len);
		return;
	}
	else
	{
		int tobufend = dmasize - pos;  /* bytes to buffer's end. */
		int len1 = len;
		int len2 = 0;

		if (len1 > tobufend)
		{
			len1 = tobufend;
			len2 = len - len1;
		}
		memcpy(play_buffer, dma.buffer + pos, len1);
		if (len2 <= 0)
			dmapos += (len1 / (dma.samplebits/8));
		else  /* wraparound? */
		{
			memcpy(play_buffer+len1, dma.buffer, len2);
			dmapos = (len2 / (dma.samplebits/8));
		}
	}

	if (dmapos >= dmasize)
		dmapos = 0;

	SLresult result;
	//LOGI("Frame count = %d",FrameCount);
	if (FrameCount == 0)
		FrameCount = 1;
	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, play_buffer,FrameCount * factor);
	myassert(SL_RESULT_SUCCESS == result,"Enqueue failed");

	 pthread_mutex_unlock(&dma_mutex);
}

qboolean SNDDMA_Init( void ) {
	int rc;
	int fmt;
	int tmp;
	int i;
	// char *s; // bk001204 - unused
	struct audio_buf_info info;
	int caps;
	extern uid_t saved_euid;

	if ( snd_inited ) {
		return 1;
	}

	dmapos = 0;
	dma.samplebits = 16;
	dma.channels = 2;
	dma.samples = 1024*16;
	dma.submission_chunk = 1024*2;
	//dma.submission_chunk = 1;
	dma.speed = 44100;
	dma.speed = 22050;
	dmasize = (dma.samples * (dma.samplebits/8));
	dma.buffer = calloc(1, dmasize);

	SLresult result;

	// create engine
	result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
	myassert(SL_RESULT_SUCCESS == result,"slCreateEngine");

	// realize the engine
	result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
	myassert(SL_RESULT_SUCCESS == result,"Realize");

	// get the engine interface, which is needed in order to create other objects
	result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
	myassert(SL_RESULT_SUCCESS == result,"GetInterface");

	// create output mix
	result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
	myassert(SL_RESULT_SUCCESS == result,"CreateOutputMix");

	// realize the output mix
	result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
	myassert(SL_RESULT_SUCCESS == result,"Realize output mix");

	//CREATE THE PLAYER

	// configure audio source
	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1};
	SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_22_05,
			SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
			SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
	SLDataSource audioSrc = {&loc_bufq, &format_pcm};

	// configure audio sink
	SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
	SLDataSink audioSnk = {&loc_outmix, NULL};

	// create audio player
	Com_Printf("create audio player");
	const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
	const SLboolean req[1] = {SL_BOOLEAN_TRUE};
	result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
			1, ids, req);
	myassert(SL_RESULT_SUCCESS == result,"CreateAudioPlayer");


	// realize the player
	result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
	myassert(SL_RESULT_SUCCESS == result,"Realize AudioPlayer");

	// get the play interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
	myassert(SL_RESULT_SUCCESS == result,"GetInterface AudioPlayer");

	// get the buffer queue interface
	result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
			&bqPlayerBufferQueue);
	myassert(SL_RESULT_SUCCESS == result,"GetInterface buffer queue");

	// register callback on the buffer queue
	result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
	myassert(SL_RESULT_SUCCESS == result,"RegisterCallback");

	// set the player's state to playing
	result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
	myassert(SL_RESULT_SUCCESS == result,"SetPlayState");



	result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, "\0", 1);
	myassert(SL_RESULT_SUCCESS == result,"Enqueue first buffer");



	snd_inited = 1;
	return 1;
}

int SNDDMA_GetDMAPos( void ) {
	struct count_info count;

	if ( !snd_inited ) {
		return 0;
	}

	pthread_mutex_lock(&dma_mutex);

	//LOGI("SNDDMA_GetDMAPos");
	return dmapos;
}

void SNDDMA_Shutdown( void ) {
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
 */
void SNDDMA_Submit( void ) {
	//LOGI("SNDDMA_Submit");
	 pthread_mutex_unlock(&dma_mutex);
}

void SNDDMA_BeginPainting( void ) {
	//LOGI("SNDDMA_BeginPainting");
	//pthread_mutex_lock(&dma_mutex);
}
