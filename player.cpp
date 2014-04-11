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

/*
 * 
 *      Copyright (C) 2012 Edgar Hucek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <string.h>

#define AV_NOWARN_DEPRECATED

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
#include <iostream>
#include <utility>

#include "version.h"
#include "API.h"
#include "player.h"

static float get_display_aspect_ratio(HDMI_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case HDMI_ASPECT_4_3:   display_aspect = 4.0/3.0;   break;
    case HDMI_ASPECT_14_9:  display_aspect = 14.0/9.0;  break;
    case HDMI_ASPECT_16_9:  display_aspect = 16.0/9.0;  break;
    case HDMI_ASPECT_5_4:   display_aspect = 5.0/4.0;   break;
    case HDMI_ASPECT_16_10: display_aspect = 16.0/10.0; break;
    case HDMI_ASPECT_15_9:  display_aspect = 15.0/9.0;  break;
    case HDMI_ASPECT_64_27: display_aspect = 64.0/27.0; break;
    default:                display_aspect = 16.0/9.0;  break;
  }
  return display_aspect;
}

static float get_display_aspect_ratio(SDTV_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case SDTV_ASPECT_4_3:  display_aspect = 4.0/3.0;  break;
    case SDTV_ASPECT_14_9: display_aspect = 14.0/9.0; break;
    case SDTV_ASPECT_16_9: display_aspect = 16.0/9.0; break;
    default:               display_aspect = 4.0/3.0;  break;
  }
  return display_aspect;
}

OMXPlayer::OMXPlayer()
{
	_omxPrepped = false;
	_stopPlayback = false;
	_pausePlayback = false;
	_isPlaying = false;
	_hasExited = false;

	_audioStreamCount = 0;
	_videoStreamCount = 0;
	_audioOutput = "";
	_audioPassthrough = false;
	_displayAspect = 0.0f;
}

OMXPlayer::~OMXPlayer()
{
	TeardownOmx();
}

void OMXPlayer::PrepareOmx()
{
	_RBP.Initialize();
	_OMX.Initialize();

	_omxClock = new OMXClock();
	_omxReader = new OMXReader();
	_omxPlayerVideo = new OMXPlayerVideo();
	_omxPlayerAudio = new OMXPlayerAudio();

	_omxPrepped = true;
}

void OMXPlayer::TeardownOmx()
{
	if (_omxPrepped)
	{
		delete _omxPlayerAudio;
		delete _omxPlayerVideo;
		delete _omxReader;
		delete _omxClock;
	
		_OMX.Deinitialize();
		_RBP.Deinitialize();

		_omxPrepped = false;
	}
}

bool OMXPlayer::InitalizePlayback()
{
	CRect destRect = {0,0,0,0};
	bool deinterlace = false;
	bool hdmiClockSync = false;
	bool threadPlayer = true;
	float videoQueueSize = 0.0f; //  use default
	float videoFifoSize = 0.0f;  //  use default
	std::string audioDeviceName = "";
	float audioQueueSize = 0.0f; //  use default
	float audioFifoSize = 0.0f;  //  use default
	long initialVolume = 0;
	int useHardwareAudio = false;
	bool boostOnDownmix = false;

	_videoStreamCount = _omxReader->VideoStreamCount();
	_audioStreamCount = _omxReader->AudioStreamCount();

	if (!HasVideo() && !HasAudio())
		//  file has no streams, can not play
		return false;

	if (!_omxClock->OMXInitialize(HasVideo(), HasAudio()))
		//  failed to initalize the OMX media clock
		return false;

	//  read "hints", ie: stream codec and format information
	_omxReader->GetHints(OMXSTREAM_AUDIO, _audioHints);
	_omxReader->GetHints(OMXSTREAM_VIDEO, _videoHints);
	/*
		support for selecting audio stream before starting playback
		if (audioIndex != -1)
			player->omxReader->SetActiveStream(OMXSTREAM_AUDIO, audioIndex);
	*/
	_displayAspect = DetectAspectRatio();
	if (HasVideo())
	{
		if (!_omxPlayerVideo->Open(_videoHints, _omxClock, destRect, deinterlace ? 1 : 0,
                                         hdmiClockSync, threadPlayer, _displayAspect, videoQueueSize, videoFifoSize))
			//  failed to open video playback
			return false;
	}

	audioDeviceName = GetAudioOutputDevice();

	//  disable audio passthrough if the HDMI receiver doesn't support AC3 or DTS
	if ((_audioHints.codec == CODEC_ID_AC3 || _audioHints.codec == CODEC_ID_EAC3) &&
		_bcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) != 0)
		SetAudioPassthrough(false);
	if (_audioHints.codec == CODEC_ID_DTS &&
		_bcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) != 0)
		SetAudioPassthrough(false);

	if (HasAudio())
	{
		std::cout << "Opening " << audioDeviceName;
		if (!_omxPlayerAudio->Open(_audioHints, _omxClock, _omxReader, audioDeviceName, 
											GetAudioPassthrough(), initialVolume, useHardwareAudio,
											boostOnDownmix, threadPlayer, audioQueueSize, audioFifoSize))
			//  failed to open audio playback
			return false;
	}

	return true;
}

bool OMXPlayer::ReinitalizeTV()
{
	CRect destRect = {0,0,0,0};
	bool deinterlace = false;
	bool hdmiClockSync = false;
	bool threadPlayer = true;
	float videoQueueSize = 0.0f; //  use default
	float videoFifoSize = 0.0f;  //  use default

	_omxPlayerVideo->Close();
    if(HasVideo() && !_omxPlayerVideo->Open(_videoHints, _omxClock, destRect, deinterlace ? 1 : 0,
                                         hdmiClockSync, threadPlayer, _displayAspect, videoQueueSize, videoFifoSize))
        return false;
	return true;
}

void OMXPlayer::CleanupPlayback()
{
	_omxClock->OMXStop();
	_omxClock->OMXStateIdle();
	_omxClock->OMXStateExecute();

	_omxPlayerVideo->Close();
	_omxPlayerAudio->Close();
	
	_omxReader->Close();
	_omxClock->OMXDeinitialize();
}

float OMXPlayer::DetectAspectRatio()
{
	float displayAspect = 0.0f;

	memset(&_tvState, 0, sizeof(TV_DISPLAY_STATE_T));
	_bcmHost.vc_tv_get_display_state(&_tvState);
	if(_tvState.state & ( VC_HDMI_HDMI | VC_HDMI_DVI ))
	{
		//HDMI or DVI on
		displayAspect = get_display_aspect_ratio((HDMI_ASPECT_T)_tvState.display.hdmi.aspect_ratio);
	}
	else
	{
		//composite on
		displayAspect = get_display_aspect_ratio((SDTV_ASPECT_T)_tvState.display.sdtv.display_options.aspect);
	}
	displayAspect *= (float)_tvState.display.hdmi.height/(float)_tvState.display.hdmi.width;
	return displayAspect;
}

bool OMXPlayer::HasAudio()
{
	//printf("Audio streams: %d\n", _audioStreamCount);
	return _audioStreamCount;
}

bool OMXPlayer::HasVideo()
{
	//printf("Video streams: %d\n", _videoStreamCount);
	return _videoStreamCount;
}

void OMXPlayer::SetAudioOutputDevice(std::string outputDevice)
{
	_audioOutput = outputDevice;
}

std::string OMXPlayer::GetAudioOutputDevice()
{
	if (_audioOutput == OMX_AUDIO_OUT_HDMI ||
		_audioOutput == OMX_AUDIO_OUT_35MM)
		return _audioOutput;
	if (_bcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) == 0)
		return OMX_AUDIO_OUT_HDMI;
	else
		return OMX_AUDIO_OUT_35MM;
}

void OMXPlayer::SetAudioPassthrough(bool usePassthrough)
{
	_audioPassthrough = usePassthrough;
}

bool OMXPlayer::GetAudioPassthrough()
{
	return _audioPassthrough;
}

bool OMXPlayer::Open(std::string filename)
{
	PrepareOmx();
	if (!_omxReader->Open(filename, false))
	{
		TeardownOmx();
		return false;
	}
	return true;
}

bool OMXPlayer::OpenContext(AVFormatContext *context)
{
	PrepareOmx();
	if (!_omxReader->OpenContext(context, true))
	{
		TeardownOmx();
		return false;
	}
	return true;
}

bool OMXPlayer::Play()
{
	OMXPacket *packet = NULL;
	bool haveSentEos = false;

	if (!InitalizePlayback())
	{
		CleanupPlayback();
		TeardownOmx();
		return false;
	}

	_stopPlayback = false;
	_pausePlayback = false;
	_isPlaying = true;
	_omxClock->OMXStart(0.0);
	_omxClock->OMXPause();
	_omxClock->OMXStateExecute();
	//  main video playback loop
	while(!_stopPlayback)
	{
		//  read packet
		if (!packet)
		{
			packet = _omxReader->Read();
		}

		//  check if paused
		if (!_pausePlayback && packet && _omxClock->OMXIsPaused())
		{
			_omxClock->OMXResume();
		}

		if (packet)
			haveSentEos = false;

		//  check end of stream conditions
		if (!packet && _omxReader->IsEof())
		{
			//  if media is still playing sleep for a bit then restart the loop
			if ( (HasVideo() && _omxPlayerVideo->GetCached()) ||
				(HasAudio() && _omxPlayerAudio->GetCached()) )
			{
				OMXClock::OMXSleep(10);
				continue;
			}
			
			//  only submit EOS once, unless we get another packet (apparently)
			if (!haveSentEos)
			{
				if (HasVideo())
					_omxPlayerVideo->SubmitEOS();
				if (HasAudio())
					_omxPlayerAudio->SubmitEOS();
				haveSentEos = true;
			}
			if ( (HasVideo() && _omxPlayerVideo->IsEOS()) ||
				(HasAudio() && _omxPlayerAudio->IsEOS()) )
			{
				OMXClock::OMXSleep(10);
				continue;
			}
			break;
		}

		//  process the packet to be played
		if(HasVideo() && packet && _omxReader->IsActive(OMXSTREAM_VIDEO, packet->stream_index))
		{
			if(_omxPlayerVideo->AddPacket(packet))
				packet = NULL;
			else
				OMXClock::OMXSleep(10);
		}
		else if(HasAudio() && packet && packet->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			if(_omxPlayerAudio->AddPacket(packet))
				packet = NULL;
			else
				OMXClock::OMXSleep(10);
		}
		else
		{
			if(packet)
			{
				_omxReader->FreePacket(packet);
				packet = NULL;
			}
		}
	}
	_isPlaying = false;

	if(packet)
	{
		_omxReader->FreePacket(packet);
		packet = NULL;
	}
	CleanupPlayback();
	TeardownOmx();
	return true;
}

void OMXPlayer::Pause()
{
	if (!_isPlaying)
		return;
	if (IsPaused())
    {
		_pausePlayback = false;
        _omxClock->OMXResume();
    }
    else
    {
		_pausePlayback = true;
        _omxClock->OMXPause();
    }
}

void OMXPlayer::Stop()
{
	_stopPlayback = true;
}

bool OMXPlayer::IsPaused()
{
	if (!_isPlaying)
		return false;
	return _omxClock->OMXIsPaused();
}

void OMXPlayer::FlushStreams(double pts)
{
	if(HasVideo())
		_omxPlayerVideo->Flush();

	if(HasAudio())
		_omxPlayerAudio->Flush();

	/*if(m_has_subtitle)
		m_player_subtitles.Flush(pts);*/

  if(pts != DVD_NOPTS_VALUE)
    _omxClock->OMXMediaTime(pts);
}

double OMXPlayer::GetCurrentPosition()
{
	return _omxClock->OMXMediaTime();
}

bool OMXPlayer::SetCurrentPosition(double position)
{
	double newPosition;
	bool leavePaused = IsPaused();

	if (!IsPaused())
		Pause();
	if(_omxReader->SeekTime((int)position, false, &newPosition))
	{
        FlushStreams(newPosition);

		if (!ReinitalizeTV())
		{
			Stop();
			return false;
		}

		_omxClock->OMXMediaTime(newPosition);
	}
	else
	{
		Stop();
		return false;
	}
	if (leavePaused && !IsPaused())
		Pause();
	return true;
}

/*void play(OMXPlayerApi *player)
{
	CRBP g_RBP;
	COMXCore g_OMX;
	DllBcmHost bcmHost;
	bool hasVideo = false;
	bool hasAudio = false;
	COMXStreamInfo audioHints;
	COMXStreamInfo videoHints;
	int audioIndex = -1;
	float displayAspect = 0.0f;
	TV_DISPLAY_STATE_T tvState;
	bool deinterlace = false;
	bool hdmiClockSync = false;
	bool threadPlayer = true;
	CRect destRect = {0,0,0,0};
	float videoQueueSize = 0.0f; //  use default
	float videoFifoSize = 0.0f;  //  use default
	float audioQueueSize = 0.0f; //  use default
	float audioFifoSize = 0.0f;  //  use default
	std::string deviceString = "omx:hdmi";  //  audio output?
	bool passthrough = true;
	long initialVolume = 0;
	int useHardwareAudio = false;
	bool boostOnDownmix = false;
	OMXPacket *packet = NULL;
	double startpts = 0;
	
	g_RBP.Initialize();
	g_OMX.Initialize();
	
	player->omxClock = new OMXClock();
	player->omxReader = new OMXReader();
	player->omxPlayerVideo = new OMXPlayerVideo();
	player->omxPlayerAudio = new OMXPlayerAudio();
	
	if (player->omxReader->OpenContext(player->formatContext, false))
	{
		hasVideo = player->omxReader->VideoStreamCount();
		hasAudio = player->omxReader->AudioStreamCount();
		
		if (player->omxClock->OMXInitialize(hasVideo, hasAudio))
		{
			player->omxReader->GetHints(OMXSTREAM_AUDIO, audioHints);
			player->omxReader->GetHints(OMXSTREAM_VIDEO, videoHints);
			
			if (audioIndex != -1)
				player->omxReader->SetActiveStream(OMXSTREAM_AUDIO, audioIndex);
			
			memset(&tvState, 0, sizeof(TV_DISPLAY_STATE_T));
			bcmHost.vc_tv_get_display_state(&tvState);
			if(tvState.state & ( VC_HDMI_HDMI | VC_HDMI_DVI ))
			{
				//HDMI or DVI on
				displayAspect = get_display_aspect_ratio((HDMI_ASPECT_T)tvState.display.hdmi.aspect_ratio);
			}
			else
			{
				//composite on
				displayAspect = get_display_aspect_ratio((SDTV_ASPECT_T)tvState.display.sdtv.display_options.aspect);
			}
			displayAspect *= (float)tvState.display.hdmi.height/(float)tvState.display.hdmi.width;
			
			if(!(hasVideo && !player->omxPlayerVideo->Open(videoHints, player->omxClock, destRect, deinterlace ? 1 : 0,
                                         hdmiClockSync, threadPlayer, displayAspect, videoQueueSize, videoFifoSize)))
			{
				if (deviceString == "")
				{
					if (bcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) == 0)
						deviceString = "omx:hdmi";
					else
						deviceString = "omx:local";
				}
				
				if ((audioHints.codec == CODEC_ID_AC3 || audioHints.codec == CODEC_ID_EAC3) &&
					bcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) != 0)
					passthrough = false;
				if (audioHints.codec == CODEC_ID_DTS &&
					bcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) != 0)
					passthrough = false;
				
				if(!(hasAudio && !player->omxPlayerAudio->Open(audioHints, player->omxClock, player->omxReader, deviceString, 
																passthrough, initialVolume, useHardwareAudio,
																boostOnDownmix, threadPlayer, audioQueueSize, audioFifoSize)))
				{
					bool sendEos = false;
					
					player->omxClock->OMXStart(0.0);
					player->omxClock->OMXPause();
					player->omxClock->OMXStateExecute();
					player->stopPlayback = false;
					player->isPlaying = true;
					while(!player->stopPlayback)
					{
						if (player->performSeek > -1)
						{
							if (player->omxReader->CanSeek())
							{
        							if (player->omxReader->SeekTime(player->performSeek, false, &startpts))
								{
									//  toggle video output off then on
									//  flush streams
								}
						        }
							player->performSeek = -1;
						}

						if (!packet)
						{
							packet = player->omxReader->Read();
						}
						
						if (packet)
							sendEos = false;
							
						if (!player->pausePlayback && packet && player->omxClock->OMXIsPaused())
						{
							player->omxClock->OMXResume();
						}
						
						if (!packet && player->omxReader->IsEof())
						{
							//  if media is still playing sleep for a bit then restart the loop
							if ( (hasVideo && player->omxPlayerVideo->GetCached()) ||
								(hasAudio && player->omxPlayerAudio->GetCached()) )
							{
								OMXClock::OMXSleep(10);
								continue;
							}
							
							if (!sendEos && hasVideo)
								player->omxPlayerVideo->SubmitEOS();
							if (!sendEos && hasAudio)
								player->omxPlayerAudio->SubmitEOS();
							sendEos = true;
							if ( (hasVideo && !player->omxPlayerVideo->IsEOS()) ||
							   (hasAudio && !player->omxPlayerAudio->IsEOS()) )
							{
								OMXClock::OMXSleep(10);
								continue;
							}
							break;
						}
						
						if(hasVideo && packet && player->omxReader->IsActive(OMXSTREAM_VIDEO, packet->stream_index))
						{
							if(player->omxPlayerVideo->AddPacket(packet))
								packet = NULL;
							else
								OMXClock::OMXSleep(10);
						}
						else if(hasAudio && packet && packet->codec_type == AVMEDIA_TYPE_AUDIO)
						{
							if(player->omxPlayerAudio->AddPacket(packet))
								packet = NULL;
							else
								OMXClock::OMXSleep(10);
						}
						else
						{
							if(packet)
							{
								player->omxReader->FreePacket(packet);
								packet = NULL;
							}
						}
					}
					player->isPlaying = false;
				}
			}
		}
	}
	
	player->omxClock->OMXStop();
	player->omxClock->OMXStateIdle();
	player->omxClock->OMXStateExecute();

	player->omxPlayerVideo->Close();
	player->omxPlayerAudio->Close();

	if(packet)
	{
		player->omxReader->FreePacket(packet);
		packet = NULL;
	}

	player->omxReader->Close();

	player->omxClock->OMXDeinitialize();
	
	delete player->omxPlayerAudio;
	delete player->omxPlayerVideo;
	delete player->omxReader;
	delete player->omxClock;
	
	g_OMX.Deinitialize();
	g_RBP.Deinitialize();

	player->hasExited = true;
}*/