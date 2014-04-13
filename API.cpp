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
#include <stdlib.h>
#include "player.h"
#include "API.h"

extern "C" DLL_PUBLIC OMXApiState *omx_create()
{
	OMXApiState *state = (OMXApiState*)malloc(sizeof(OMXApiState));
	state->avioContext = NULL;
	state->formatContext = NULL;
	state->inputFormat = NULL;
	state->player = new OMXPlayer();
	return state;
}

extern "C" DLL_PUBLIC void omx_destroy(OMXApiState *state)
{
	delete state->player;
	free(state);
}

extern "C" DLL_PUBLIC bool omx_create_context(OMXApiState *state, unsigned char *buffer, int bufferSize, void *opaque,
											int (*readPacket)(void *opaque, uint8_t *buf, int buf_size),
											int64_t (*seek)(void *opaque, int64_t offset, int whence),
											char *filename)
{
	AVProbeData probeData;
	AVInputFormat *format;

	av_register_all();

	state->avioContext = avio_alloc_context(buffer, bufferSize, 0, opaque, readPacket, 0, seek);
	if (!state->avioContext) return false;

	state->formatContext = avformat_alloc_context();
	if (!state->formatContext) return false;
	state->formatContext->pb = state->avioContext;
	state->formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;

	readPacket(opaque, buffer, bufferSize);
	if (seek(0, 0, SEEK_SET) == -1)
	{
		state->avioContext->seekable = 0;
	}

	probeData.buf = buffer;
	probeData.buf_size = bufferSize;
	probeData.filename = filename;
	format = av_probe_input_format(&probeData, 1);
	if (!format)
	{
		return false;
	}
	state->formatContext->iformat = format;

	return true;
}

extern "C" DLL_PUBLIC bool omx_play(OMXApiState *state, char *file)
{
	if (!state->player->Open(file))
		return false;
	return state->player->Play();
}

extern "C" DLL_PUBLIC bool omx_play_context(OMXApiState *state)
{
	avformat_open_input(&state->formatContext, "", 0, 0);
	if (!state->player->OpenContext(state->formatContext))
		return false;
	return state->player->Play();
}

extern "C" DLL_PUBLIC void omx_pause(OMXApiState *state)
{
	state->player->Pause();
}

extern "C" DLL_PUBLIC void omx_stop(OMXApiState *state)
{
	state->player->Stop();
}

extern "C" DLL_PUBLIC void omx_set_passthrough(OMXApiState *state, bool value)
{
	state->player->SetAudioPassthrough(value);
}

extern "C" DLL_PUBLIC bool omx_get_passthrough(OMXApiState *state)
{
	return state->player->GetAudioPassthrough();
}