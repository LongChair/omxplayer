// 
// Author: John Carruthers (johnc@frag-labs.com)
// 
// Copyright (C) 2014 John Carruthers
// 
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//  
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//  
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

/**
	Defines a C friendly API to be consumed by languages like C# without
	porting the OMXPlayer class to each language.
*/

extern "C" {
#ifndef HAVE_MMX
#define HAVE_MMX
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __GNUC__
#pragma warning(disable:4244)
#endif
#if (defined USE_EXTERNAL_FFMPEG)
  #if (defined HAVE_LIBAVFORMAT_AVFORMAT_H)
    #include <libavformat/avio.h>
    #include <libavformat/avformat.h>
  #else
    #include <ffmpeg/avio.h>
    #include <ffmpeg/avformat.h>
  #endif
#else
  #include "libavformat/avio.h"
  #include "libavformat/avformat.h"
#endif
}

#include <string>
#include "OMXVideo.h"
#include "OMXAudioCodecOMX.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "player.h"

#define DLL_PUBLIC __attribute__ ((visibility ("default")))

#ifndef _LIBOMX_API_H_
#define _LIBOMX_API_H_
struct OMXApiState {
	AVIOContext *avioContext;
	AVFormatContext *formatContext;
	AVInputFormat *inputFormat;
	OMXPlayer *player;
};
#endif

/**
	Creates a new OMX API state.
 */
extern "C" DLL_PUBLIC OMXApiState *omx_create();

/**
	Destroys an OMX API state.
	@param state OMX API state.
	@param cleanupFFMpeg If ffmpeg contexts should be destroyed.
*/
extern "C" DLL_PUBLIC void omx_destroy(OMXApiState *state, bool cleanupFFmpeg);

/**
	Creates an FFMpeg format context for use with custom read and seek functions.
	The format of the media stream will also be detected.
*/
extern "C" DLL_PUBLIC bool omx_create_context(OMXApiState *state, unsigned char *buffer, int bufferSize, void *opaque,
											int (*readPacket)(void *opaque, uint8_t *buf, int buf_size),
											int64_t (*seek)(void *opaque, int64_t offset, int whence),
											char *filename);

/**
	Start playback of a file.
*/
extern "C" DLL_PUBLIC bool omx_play(OMXApiState *state, char *file);

/**
	Start playback using the previously created FFMpeg context.
*/
extern "C" DLL_PUBLIC bool omx_play_context(OMXApiState *state);

/**
	Pause playback.
*/
extern "C" DLL_PUBLIC void omx_pause(OMXApiState *state);

/**
	Stop playback.
*/
extern "C" DLL_PUBLIC void omx_stop(OMXApiState *state);

/**
	Set the audio passthrough configuration.
*/
extern "C" DLL_PUBLIC void omx_set_passthrough(OMXApiState *state, bool value);

/**
	Get the audio passthrough configuration.
*/
extern "C" DLL_PUBLIC bool omx_get_passthrough(OMXApiState *state);