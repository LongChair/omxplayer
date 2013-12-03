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
#include "KeyConfig.h"

#include <string>
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

void play(OMXPlayerApi *player)
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
					while(!player->stopPlayback)
					{
						if (!packet)
						{
							packet = player->omxReader->Read();
						}
						
						if (packet)
							sendEos = false;
							
						if (packet && player->omxClock->OMXIsPaused())
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
}