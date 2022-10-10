#ifndef _DEBUG

#include "MediaSoupMailbox.h"

#include "common_audio/include/audio_util.h"

/**
* MediaSoupMailbox
*/

MediaSoupMailbox::~MediaSoupMailbox()
{
	if (m_to_float_resampler != nullptr)
		audio_resampler_destroy(m_to_float_resampler);

	if (m_from_float_to_mediasoup_resampler != nullptr)
		audio_resampler_destroy(m_from_float_to_mediasoup_resampler);

	if (m_to_mediasoup_resampler != nullptr)
		audio_resampler_destroy(m_to_mediasoup_resampler);
}

rtc::scoped_refptr<webrtc::I420Buffer> MediaSoupMailbox::getProducerFrameBuffer(const int width, const int height)
{
	if (m_producerFrameBuffer == nullptr || m_producerFrameBuffer->width() != width || m_producerFrameBuffer->height() != height)
		m_producerFrameBuffer = webrtc::I420Buffer::Create(width, height);

	return m_producerFrameBuffer;
}

void MediaSoupMailbox::push_received_videoFrame(std::unique_ptr<webrtc::VideoFrame> ptr)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_video);
	m_received_video_frame = std::move(ptr);
}

void MediaSoupMailbox::pop_receieved_videoFrames(std::unique_ptr<webrtc::VideoFrame> &output)
{
	std::lock_guard<std::mutex> grd(m_mtx_received_video);
	output = std::move(m_received_video_frame);
}

void MediaSoupMailbox::assignOutgoingAudioParams(const audio_format audioformat, const speaker_layout speakerLayout, const int bytesPerSample, const int numChannels,
						 const int samples_per_sec)
{
	if (m_obs_bytesPerSample == bytesPerSample && m_obs_numChannels == numChannels && m_obs_samples_per_sec == samples_per_sec && m_obs_audioformat == audioformat &&
	    m_obs_speakerLayout == speakerLayout)
		return;

	m_outgoing_audio_data.clear();
	m_outgoing_audio_data.resize(numChannels);
	m_obs_numFrames = 0;

	m_obs_bytesPerSample = bytesPerSample;
	m_obs_numChannels = numChannels;
	m_obs_samples_per_sec = samples_per_sec;
	m_obs_audioformat = audioformat;
	m_obs_speakerLayout = speakerLayout;

	if (m_to_float_resampler != nullptr)
		audio_resampler_destroy(m_to_float_resampler);

	if (m_from_float_to_mediasoup_resampler != nullptr)
		audio_resampler_destroy(m_from_float_to_mediasoup_resampler);

	if (m_to_mediasoup_resampler != nullptr)
		audio_resampler_destroy(m_to_mediasoup_resampler);

	struct resample_info from;
	struct resample_info to;

	// Format -> Float
	from.samples_per_sec = m_obs_samples_per_sec;
	from.speakers = speakerLayout;
	from.format = audioformat;

	to.samples_per_sec = m_obs_samples_per_sec;
	to.speakers = speakerLayout;
	to.format = AUDIO_FORMAT_FLOAT;

	m_to_float_resampler = audio_resampler_create(&to, &from);

	// Float -> GetDefaultAudioFormat
	from.samples_per_sec = m_obs_samples_per_sec;
	from.speakers = speakerLayout;
	from.format = AUDIO_FORMAT_FLOAT;

	to.samples_per_sec = m_obs_samples_per_sec;
	to.speakers = speakerLayout;
	to.format = MediaSoupTransceiver::GetDefaultAudioFormat();

	m_from_float_to_mediasoup_resampler = audio_resampler_create(&to, &from);

	// Format -> GetDefaultAudioFormat
	from.samples_per_sec = m_obs_samples_per_sec;
	from.speakers = speakerLayout;
	from.format = audioformat;

	to.samples_per_sec = m_obs_samples_per_sec;
	to.speakers = speakerLayout;
	to.format = MediaSoupTransceiver::GetDefaultAudioFormat();

	m_to_mediasoup_resampler = audio_resampler_create(&to, &from);
}

void MediaSoupMailbox::push_outgoing_audioFrame(const uint8_t **data, const int frames)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_audio);

	const int framesPer10ms = m_obs_samples_per_sec / 100;

	// Overflow?
	if (m_obs_numFrames > framesPer10ms * 256) {
		m_obs_numFrames = 0;

		for (auto &itr : m_outgoing_audio_data)
			itr.clear();
	}

	m_obs_numFrames += frames;

	int channelBufferSize = m_obs_bytesPerSample * frames;

	for (int channel = 0; channel < m_obs_numChannels; ++channel)
		m_outgoing_audio_data[channel].append((char *)data[channel], channelBufferSize);
}

void MediaSoupMailbox::pop_outgoing_audioFrames(std::vector<std::unique_ptr<SoupSendAudioFrame>> &output)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_audio);

	if (m_obs_bytesPerSample == 0 || m_obs_numChannels == 0 || m_obs_samples_per_sec == 0 || m_obs_numFrames == 0)
		return;

	const int framesPer10ms = m_obs_samples_per_sec / 100;

	while (m_obs_numFrames > framesPer10ms) {
		m_obs_numFrames -= framesPer10ms;

		std::unique_ptr<SoupSendAudioFrame> ptr = std::make_unique<SoupSendAudioFrame>();
		ptr->numFrames = framesPer10ms;
		ptr->numChannels = m_obs_numChannels;
		ptr->samples_per_sec = m_obs_samples_per_sec;
		ptr->bytesPerSample = sizeof(int16_t);

		// Pluck from the audio buffer and also convert to desired format
		uint8_t *array2d_float_planar_raw[MAX_AV_PLANES];
		uint8_t *array2d_int16_raw[MAX_AV_PLANES];
		uint8_t *array2d_float_raw[MAX_AV_PLANES];

		for (size_t channel = 0; channel < m_obs_numChannels; ++channel) {
			const int bytesFromFloatBuffer = m_obs_bytesPerSample * framesPer10ms;

			array2d_float_planar_raw[channel] = new uint8_t[bytesFromFloatBuffer];

			auto &channelBuffer_Float = m_outgoing_audio_data[channel];
			memcpy(array2d_float_planar_raw[channel], channelBuffer_Float.data(), bytesFromFloatBuffer);
			channelBuffer_Float.erase(channelBuffer_Float.begin(), channelBuffer_Float.begin() + bytesFromFloatBuffer);
		}

		uint32_t numFrames = 0;
		uint64_t tOffset = 0;

		// Avoid redundant work
		if (m_volume == 1.f) {
			// Format -> Mediasoup
			if (audio_resampler_resample(m_to_mediasoup_resampler, array2d_int16_raw, &numFrames, &tOffset, array2d_float_planar_raw, framesPer10ms)) {
				ptr->audio_data.resize(framesPer10ms * m_obs_numChannels);
				webrtc::Interleave((int16_t **)array2d_int16_raw, ptr->numFrames, ptr->numChannels, ptr->audio_data.data());
			}
		}

		// Planar -> Float
		else if (audio_resampler_resample(m_to_float_resampler, array2d_float_raw, &numFrames, &tOffset, array2d_float_planar_raw, framesPer10ms)) {
			// Apply volume
			float *cur = (float *)array2d_float_raw[0];
			float *end = cur + numFrames * m_obs_numChannels;

			while (cur < end)
				*(cur++) *= m_volume;

			// Float -> Mediasoup
			if (audio_resampler_resample(m_from_float_to_mediasoup_resampler, array2d_int16_raw, &numFrames, &tOffset, array2d_float_raw, framesPer10ms)) {
				ptr->audio_data.resize(framesPer10ms * m_obs_numChannels);
				webrtc::Interleave((int16_t **)array2d_int16_raw, ptr->numFrames, ptr->numChannels, ptr->audio_data.data());
			}
		}

		for (size_t channel = 0; channel < m_obs_numChannels; ++channel)
			delete[] array2d_float_planar_raw[channel];

		output.push_back(std::move(ptr));
	}
}

void MediaSoupMailbox::push_outgoing_videoFrame(rtc::scoped_refptr<webrtc::I420Buffer> ptr)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_video);

	// Overflow?
	if (m_outgoing_video_data.size() > 30)
		m_outgoing_video_data.clear();

	m_outgoing_video_data.push_back(ptr);
}

void MediaSoupMailbox::pop_outgoing_videoFrames(std::vector<rtc::scoped_refptr<webrtc::I420Buffer>> &output)
{
	std::lock_guard<std::mutex> grd(m_mtx_outgoing_video);
	m_outgoing_video_data.swap(output);
}

#endif
