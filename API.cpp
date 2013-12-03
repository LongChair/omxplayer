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

#include <stdlib.h>
#include "API.h"
#include "player.h"

extern "C" DLL_PUBLIC OMXPlayerApi *omx_create(unsigned char *buffer, int bufferSize, void *opaque,
											int (*readPacket)(void *opaque, uint8_t *buf, int buf_size),
											int64_t (*seek)(void *opaque, int64_t offset, int whence))
{
	OMXPlayerApi *player = (OMXPlayerApi*)malloc(sizeof(OMXPlayerApi));
	
	player->avioContext = avio_alloc_context(buffer, bufferSize, 0, opaque, readPacket, 0, seek);
	player->formatContext = avformat_alloc_context();
	player->formatContext->pb = player->avioContext;
	player->formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
	player->inputFormat = NULL;
	player->buffer = buffer;
	player->bufferSize = bufferSize;
	player->opaque = opaque;
	player->readPacket = readPacket;
	player->seek = seek;
	player->omxReader = NULL;
	player->omxClock = NULL;
	player->omxPlayerVideo = NULL;
	player->omxPlayerAudio = NULL;
	
	return player;
}

extern "C" DLL_PUBLIC void omx_destroy(OMXPlayerApi *player)
{
	//  todo: clean up ffmpeg pointers
	free(player);
}

extern "C" DLL_PUBLIC AVInputFormat *omx_probe_format(OMXPlayerApi *player, char *filename)
{
	AVProbeData probeData;
	
	av_register_all();
	
	player->readPacket(0, player->buffer, player->bufferSize);
	player->seek(0, 0, SEEK_SET);
	
	probeData.buf = player->buffer;
	probeData.buf_size = player->bufferSize;
	probeData.filename = filename;
	player->inputFormat = av_probe_input_format(&probeData, 1);
	if (!player->inputFormat)
	{
		return NULL;
	}
	player->formatContext->iformat = player->inputFormat;
	return player->inputFormat;
}

extern "C" DLL_PUBLIC void omx_play(OMXPlayerApi *player)
{
	avformat_open_input(&player->formatContext, "", 0, 0);
	play(player);
}

extern "C" DLL_PUBLIC void omx_pause(OMXPlayerApi *player)
{
}

extern "C" DLL_PUBLIC void omx_stop(OMXPlayerApi *player)
{
	player->stopPlayback = true;
}