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

#ifndef _LIBOMX_PLAYER_H_
#define _LIBOMX_PLAYER_H_

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
};

#include "OMXStreamInfo.h"

#include "utils/log.h"

#include "DllAvUtil.h"
#include "DllAvFormat.h"
#include "DllAvFilter.h"
#include "DllAvCodec.h"
#include "linux/RBP.h"

#include "OMXVideo.h"
#include "OMXAudioCodecOMX.h"
#include "utils/PCMRemap.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "OMXPlayerSubtitles.h"
#include "DllOMX.h"
#include "Srt.h"

#include <string>

/**
 * Output audio through the HDMI port.
*/
const std::string OMX_AUDIO_OUT_HDMI = "omx:hdmi";
/**
 * Output audio through the 3.5mm jack.
*/
const std::string OMX_AUDIO_OUT_35MM = "omx:local";

/**
	Encapsulates OpenMax player API.
*/
class OMXPlayer
{
private:
	CRBP _RBP;
	COMXCore _OMX;
	DllBcmHost _bcmHost;
	COMXStreamInfo _audioHints;
	COMXStreamInfo _videoHints;
	TV_DISPLAY_STATE_T _tvState;

	AVIOContext *_avioContext;
	AVFormatContext *_formatContext;
	AVInputFormat *_inputFormat;
	unsigned char *_buffer;
	int _bufferSize;
	void *_opaque;
	int (*_readPacket)(void *opaque, uint8_t *buf, int buf_size);
	int64_t (*_seek)(void *opaque, int64_t offset, int whence);
	
	bool _omxPrepped;
	bool _stopPlayback;
	bool _pausePlayback;
	bool _isPlaying;
	bool _hasExited;
	int _performSeek;

	int _audioStreamCount;
	int _videoStreamCount;

	OMXReader *_omxReader;
	OMXClock *_omxClock;
	OMXPlayerVideo *_omxPlayerVideo;
	OMXPlayerAudio *_omxPlayerAudio;
	float _displayAspect;

	/**
	 * Audio output device.
	*/
	std::string _audioOutput;
	/**
	 * When true audio is not decoded on the rpi.
	*/
	bool _audioPassthrough;

	void PrepareOmx();
	void TeardownOmx();
	float GetDisplayAspectRatio(HDMI_ASPECT_T aspect);
	float GetDisplayAspectRatio(SDTV_ASPECT_T aspect);
	bool InitalizePlayback();
	bool ReinitalizeTV();
	void CleanupPlayback();
	float DetectAspectRatio();

	/**
		Performs a flush on all playing streams.
	*/
	void FlushStreams(double pts);
public:
	OMXPlayer();
	~OMXPlayer();

	/**
		Returns TRUE if the current stream has audio channels.
	*/
	bool HasAudio();

	/**
		Returns TRUE if the current stream has video channels.
	*/
	bool HasVideo();

	/**
		Sets the audio output device.
		Valid values are OMX_AUDIO_OUT_HDMI or OMX_AUDIO_OUT_35MM.
	*/
	void SetAudioOutputDevice(std::string outputDevice);

	/**
		Gets the current audio output device configuration.
	*/
	std::string GetAudioOutputDevice();

	/**
		Sets whether audio data should be output without decoding.
	*/
	void SetAudioPassthrough(bool usePassthrough);

	/**
		Returns TRUE if audio data will be output directly, FALSE if audio will be decoded.
	*/
	bool GetAudioPassthrough();

	/**
		Opens a file for playback.

		@return TRUE on success, FALSE otherwise.
	*/
	bool Open(std::string filename);

	/**
		Opens an FFMpeg format context for playback.

		@return TRUE on success, FALSE otherwise.
	*/
	bool OpenContext(AVFormatContext *context);

	/**
		Begins playback.
	*/
	bool Play();

	/**
		Stops playback.
	*/
	void Stop();

	/**
		Pauses playback.
	*/
	void Pause();

	/**
		Returns true if the player is paused, false otherwise.
	*/
	bool IsPaused();

	/**
		Seeks to the provided position.

		@param position The position to seek to in milliseconds.
		@return TRUE on success, FALSE otherwise.
	*/
	bool SetCurrentPosition(double position);

	/**
		Gets the current position.

		@return The current position in milliseconds.
	*/
	double GetCurrentPosition();
};

#endif