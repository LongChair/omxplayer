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

int run(int argc, char *argv[], AVFormatContext *avFormat);
