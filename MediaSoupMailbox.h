#pragma once

#include "MediaSoupTransceiver.h"

/**
* MediaSoupMailbox
*/

class MediaSoupMailbox
{
public:
	// 10ms frame
	struct SoupSendAudioFrame
	{
		std::vector<int16_t> audio_data;
		int numFrames = 0;
		int numChannels = 0;
		int bytesPerSample = 0;
		int samples_per_sec = 0;
	};
public:
	~MediaSoupMailbox();

	rtc::scoped_refptr<webrtc::I420Buffer> getProducerFrameBuffer(const int width, const int height);

public:
	// Receive
	void push_received_videoFrame(std::unique_ptr<webrtc::VideoFrame> ptr);
	void pop_receieved_videoFrames(std::unique_ptr<webrtc::VideoFrame>& output);

public:
	// Outgoing
	void push_outgoing_videoFrame(rtc::scoped_refptr<webrtc::I420Buffer>);
	void pop_outgoing_videoFrames(std::vector<rtc::scoped_refptr<webrtc::I420Buffer>>& output);

	void push_outgoing_audioFrame(const uint8_t** data, const int frames);
	void pop_outgoing_audioFrames(std::vector<std::unique_ptr<SoupSendAudioFrame>>& output);

	void assignOutgoingAudioParams(const audio_format audioformat, const speaker_layout speakerLayout, const int bytesPerSample, const int numChannels, const int samples_per_sec);
	void assignOutgoingVolume(const float vol) { m_volume = vol; }

private:
	// Receive
	std::mutex m_mtx_received_video;	
	std::unique_ptr<webrtc::VideoFrame> m_received_video_frame;

private:
	// Outgoing
	std::mutex m_mtx_outgoing_video;
	std::mutex m_mtx_outgoing_audio;

	std::vector<rtc::scoped_refptr<webrtc::I420Buffer>> m_outgoing_video_data;
	std::vector<std::string> m_outgoing_audio_data;

	int m_obs_bytesPerSample = 0;
	int m_obs_numChannels = 0;
	int m_obs_samples_per_sec = 0;
	int m_obs_numFrames = 0;

	float m_volume = 0;

	audio_format m_obs_audioformat = AUDIO_FORMAT_UNKNOWN;
	speaker_layout m_obs_speakerLayout = SPEAKERS_UNKNOWN;

	audio_resampler_t *m_to_float_resampler = nullptr;
	audio_resampler_t *m_from_float_to_mediasoup_resampler = nullptr;
	
	rtc::scoped_refptr<webrtc::I420Buffer> m_producerFrameBuffer;
};
