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

#include "OMXVideo.h"
#include "OMXAudioCodecOMX.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"

#define DLL_PUBLIC __attribute__ ((visibility ("default")))

#ifndef OMX_API_DEFINED
struct OMXPlayerApi {
	AVIOContext *avioContext;
	AVFormatContext *formatContext;
	AVInputFormat *inputFormat;
	unsigned char *buffer;
	int bufferSize;
	void *opaque;
	int (*readPacket)(void *opaque, uint8_t *buf, int buf_size);
	int64_t (*seek)(void *opaque, int64_t offset, int whence);
	
	bool stopPlayback;
	OMXReader *omxReader;
	OMXClock *omxClock;
	OMXPlayerVideo *omxPlayerVideo;
	OMXPlayerAudio *omxPlayerAudio;
};
#define OMX_API_DEFINED
#endif

extern "C" DLL_PUBLIC OMXPlayerApi *omx_create(unsigned char *buffer, int bufferSize, void *opaque,
											int (*readPacket)(void *opaque, uint8_t *buf, int buf_size),
											int64_t (*seek)(void *opaque, int64_t offset, int whence));

extern "C" DLL_PUBLIC void omx_destroy(OMXPlayerApi *player);

extern "C" DLL_PUBLIC AVInputFormat *omx_probe_format(OMXPlayerApi *player, char *filename);

extern "C" DLL_PUBLIC void omx_play(OMXPlayerApi *player);

extern "C" DLL_PUBLIC void omx_pause(OMXPlayerApi *player);

extern "C" DLL_PUBLIC void omx_stop(OMXPlayerApi *player);