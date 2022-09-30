#ifndef _DEBUG

#include "ConnectorFrontApi.h"
#include "MyLogSink.h"
#include "MediaSoupMailbox.h"

#include <third_party/libyuv/include/libyuv.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <media-io/video-frame.h>

#ifndef _WIN32
    #define UNREFERENCED_PARAMETER(a) do { (void)(a); } while (0)
#endif

/**
* Source
*/

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mediasoup-connector", "en-US")
MODULE_EXPORT const char* obs_module_description(void)
{
	return "Streamlabs Join";
}

static const char* msoup_getname(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("MediaSoupConnector");
}

// Create
static void* msoup_create(obs_data_t* settings, obs_source_t* source)
{
	MediaSoupInterface::ObsSourceInfo* data = new MediaSoupInterface::ObsSourceInfo{};
	data->m_obs_source = source;

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void func_load_device(in string input, out string output)", ConnectorFrontApi::func_load_device, data);
	proc_handler_add(ph, "void func_create_send_transport(in string input, out string output)", ConnectorFrontApi::func_create_send_transport, data);
	proc_handler_add(ph, "void func_create_receive_transport(in string input, out string output)", ConnectorFrontApi::func_create_receive_transport, data);
	proc_handler_add(ph, "void func_video_consumer_response(in string input, out string output)", ConnectorFrontApi::func_video_consumer_response, data);
	proc_handler_add(ph, "void func_audio_consumer_response(in string input, out string output)", ConnectorFrontApi::func_audio_consumer_response, data);
	proc_handler_add(ph, "void func_create_audio_producer(in string input, out string output)", ConnectorFrontApi::func_create_audio_producer, data);
	proc_handler_add(ph, "void func_create_video_producer(in string input, out string output)", ConnectorFrontApi::func_create_video_producer, data);
	proc_handler_add(ph, "void func_produce_result(in string input, out string output)", ConnectorFrontApi::func_produce_result, data);
	proc_handler_add(ph, "void func_connect_result(in string input, out string output)", ConnectorFrontApi::func_connect_result, data);
	proc_handler_add(ph, "void func_stop_receiver(in string input, out string output)", ConnectorFrontApi::func_stop_receiver, data);
	proc_handler_add(ph, "void func_stop_sender(in string input, out string output)", ConnectorFrontApi::func_stop_sender, data);
	proc_handler_add(ph, "void func_stop_consumer(in string input, out string output)", ConnectorFrontApi::func_stop_consumer, data);
	proc_handler_add(ph, "void func_stop_producer(in string input, out string output)", ConnectorFrontApi::func_stop_producer, data);

	obs_source_set_audio_active(source, true);

	// Captures webrtc debug msgs
	MyLogSink::instance();
	
	++MediaSoupInterface::instance().m_sourceCounter;
	
	return data;
}

// Destroy
static void msoup_destroy(void* data)
{
	MediaSoupInterface::ObsSourceInfo* sourceInfo = static_cast<MediaSoupInterface::ObsSourceInfo*>(data);
	--MediaSoupInterface::instance().m_sourceCounter;

	if (sourceInfo->m_obs_scene_texture != nullptr)
		gs_texture_destroy(sourceInfo->m_obs_scene_texture);
	
	MediaSoupInterface::instance().getTransceiver()->StopConsumerById(sourceInfo->m_consumer_audio);
	MediaSoupInterface::instance().getTransceiver()->StopConsumerById(sourceInfo->m_consumer_video);

	// We're the last one, final cleanup
	if (MediaSoupInterface::instance().m_sourceCounter <= 0)
		MediaSoupInterface::instance().reset();

	delete sourceInfo;
}

// Video Render
static void msoup_video_render(void* data, gs_effect_t* e)
{
	MediaSoupInterface::ObsSourceInfo* sourceInfo = static_cast<MediaSoupInterface::ObsSourceInfo*>(data);
	UNREFERENCED_PARAMETER(e);

	if (!MediaSoupInterface::instance().getTransceiver()->ConsumerReady(sourceInfo->m_consumer_video))
		return;

	//GetConsumerMailbox
	std::unique_ptr<webrtc::VideoFrame> frame;
	auto mailbox = MediaSoupInterface::instance().getTransceiver()->GetConsumerMailbox(sourceInfo->m_consumer_video);

	if (mailbox == nullptr)
		return;

	mailbox->pop_receieved_videoFrames(frame);

	// A new frame arrived, apply it to our cached texture with gs_texture_set_image
	if (frame != nullptr)
		MediaSoupInterface::instance().applyVideoFrameToObsTexture(*frame, *sourceInfo);

	if (sourceInfo->m_obs_scene_texture == nullptr)
		return;

	gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t* tech = gs_effect_get_technique(effect, "Draw");
	gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");
	
	gs_enable_framebuffer_srgb(false);
	gs_enable_blending(false);
	gs_effect_set_texture(image, sourceInfo->m_obs_scene_texture);

	if (gs_technique_begin_pass(tech, 0))
	{
		// Position, center letterbox
		int xPos = 0;
		int yPos = 0;

		if (sourceInfo->m_textureWidth < MediaSoupInterface::instance().getHardObsTextureWidth())
		{
			int diff = MediaSoupInterface::instance().getHardObsTextureWidth() - sourceInfo->m_textureWidth;
			xPos = diff / 2;
		}

		if (sourceInfo->m_textureHeight < MediaSoupInterface::instance().getHardObsTextureHeight())
		{
			int diff = MediaSoupInterface::instance().getHardObsTextureHeight() - sourceInfo->m_textureHeight;
			yPos = diff / 2;
		}

		if (xPos != 0 || yPos != 0)
		{
			gs_matrix_push();
			gs_matrix_translate3f(float(xPos), float(yPos), 0.0f);
		}

		gs_draw_sprite(sourceInfo->m_obs_scene_texture, 0, 0, 0);
		
		if (xPos != 0 || yPos != 0)
			gs_matrix_pop();

		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}

	gs_enable_blending(true);
}

static void msoup_video_tick(void* data, float seconds)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(seconds);
}

static uint32_t msoup_width(void* data)
{
	return uint32_t(MediaSoupInterface::getHardObsTextureWidth());
}

static uint32_t msoup_height(void* data)
{
	return uint32_t(MediaSoupInterface::getHardObsTextureHeight());
}

static obs_properties_t* msoup_properties(void* data)
{
	obs_properties_t* ppts = obs_properties_create();	
	return ppts;
}

static void msoup_update(void* source, obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(source);
	UNREFERENCED_PARAMETER(settings);
}

static void msoup_activate(void* data)
{
	UNREFERENCED_PARAMETER(data);
}

static void msoup_deactivate(void* data)
{
	UNREFERENCED_PARAMETER(data);
}

static void msoup_enum_sources(void* data, obs_source_enum_proc_t cb, void* param)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(cb);
	UNREFERENCED_PARAMETER(param);
}

static void msoup_defaults(obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(settings);
}

/**
* Filter (Audio)
*/

static const char* msoup_faudio_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-audio-filter");
}

// Create
static void* msoup_faudio_create(obs_data_t* settings, obs_source_t* source)
{
	return source;
}

// Destroy
static void msoup_faudio_destroy(void* data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_audio_data* msoup_faudio_filter_audio(void* data, struct obs_audio_data* audio)
{
	auto source = static_cast<obs_source_t*>(data);
	auto parent = obs_filter_get_parent(source);
	auto settings = obs_source_get_settings(source);
	std::string producerId = obs_data_get_string(settings, "producerId");
	obs_data_release(settings);

	if (obs_source_muted(parent))
		return audio;

	if (!MediaSoupInterface::instance().getTransceiver()->ProducerReady(producerId))
		return audio;

	if (auto mailbox = MediaSoupInterface::instance().getTransceiver()->GetProducerMailbox(producerId))
	{
		const struct audio_output_info* aoi = audio_output_get_info(obs_get_audio());
		mailbox->assignOutgoingAudioParams(aoi->format, aoi->speakers, static_cast<int>(get_audio_size(aoi->format, aoi->speakers, 1)), static_cast<int>(audio_output_get_channels(obs_get_audio())), static_cast<int>(audio_output_get_sample_rate(obs_get_audio())));
		mailbox->assignOutgoingVolume(obs_source_get_volume(parent));
		mailbox->push_outgoing_audioFrame((const uint8_t**)audio->data, audio->frames);
	}

	return audio;
}

static obs_properties_t* msoup_faudio_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	UNUSED_PARAMETER(data);
	return props;
}

static void msoup_faudio_update(void* data, obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

static void msoup_faudio_save(void* data, obs_data_t* settings)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(settings);
}

/**
* Filter (Video Async)
*/

static const char* msoup_fvideo_get_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-video-filter");
}

// Create
static void* msoup_fvideo_create(obs_data_t* settings, obs_source_t* source)
{
	return settings;
}

// Destroy
static void msoup_fvideo_destroy(void* data)
{

}

static obs_properties_t* msoup_fvideo_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	return props;
}

static struct obs_source_frame* msoup_fvideo_filter_video(void* data, struct obs_source_frame* frame)
{
	std::string producerId = obs_data_get_string(static_cast<obs_data_t*>(data), "producerId");

	if (!MediaSoupInterface::instance().getTransceiver()->ProducerReady(producerId))
		return frame;

	auto mailbox = MediaSoupInterface::instance().getTransceiver()->GetProducerMailbox(producerId);

	if (mailbox == nullptr)
		return frame;

	rtc::scoped_refptr<webrtc::I420Buffer> dest = mailbox->getProducerFrameBuffer(frame->width, frame->height);

	switch (frame->format)
	{
	//VIDEO_FORMAT_Y800
	//VIDEO_FORMAT_I40A
	//VIDEO_FORMAT_I42A
	//VIDEO_FORMAT_AYUV
	//VIDEO_FORMAT_YVYU
	case VIDEO_FORMAT_YUY2:
		libyuv::YUY2ToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		break;
	case VIDEO_FORMAT_UYVY:
		libyuv::UYVYToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		break;
	case VIDEO_FORMAT_RGBA:
		libyuv::RGBAToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		break;
	case VIDEO_FORMAT_BGRA:
		libyuv::ARGBToI420(frame->data[0], static_cast<int>(frame->linesize[0]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());	
		break;
	case VIDEO_FORMAT_I422:
		libyuv::I422ToI420(frame->data[0],  static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), frame->data[2],  static_cast<int>(frame->linesize[2]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		break;
	case VIDEO_FORMAT_I444:
		libyuv::I444ToI420(frame->data[0],  static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), frame->data[2],  static_cast<int>(frame->linesize[2]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		break;
	case VIDEO_FORMAT_NV12:
		libyuv::NV12ToI420(frame->data[0], static_cast<int>(frame->linesize[0]), frame->data[1],  static_cast<int>(frame->linesize[1]), dest->MutableDataY(), dest->StrideY(), dest->MutableDataU(), dest->StrideU(), dest->MutableDataV(), dest->StrideV(), dest->width(), dest->height());
		break;
	default:
		return frame;
	}

	if (frame->flip)
		dest = webrtc::I420Buffer::Rotate(*dest, webrtc::VideoRotation::kVideoRotation_180);

	mailbox->push_outgoing_videoFrame(dest);
	return frame;
}

static void msoup_fvideo_update(void* data, obs_data_t* settings)
{
	//
}

static void msoup_fvideo_defaults(obs_data_t* settings)
{
	//
}

/**
* Filter (Video Sync)
*/

struct mediasoup_sync_filter
{
	obs_source_t* source{ nullptr };
	uint32_t width{ 0 };
	uint32_t height{ 0 };
	gs_texrender_t* texrender{ nullptr };
	gs_stagesurf_t* stagesurface{ nullptr };
};

static void msoup_fsvideo_filter_offscreen_render(void* param, uint32_t cx, uint32_t cy);

static const char* msoup_fsvideo_get_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("mediasoup-video-filter-s");
}

// Create
static void* msoup_fsvideo_create(obs_data_t* settings, obs_source_t* source)
{
	mediasoup_sync_filter* vars = new mediasoup_sync_filter{};
	vars->source = source;

	return vars;
}

// Destroy
static void msoup_fsvideo_destroy(void* data)
{
	mediasoup_sync_filter* vars = static_cast<mediasoup_sync_filter*>(data);

	gs_stagesurface_destroy(vars->stagesurface);
	gs_texrender_destroy(vars->texrender);

	delete vars;
}

// Add filter
static void msoup_fsvideo_filter_add(void *data, obs_source_t *source)
{
	mediasoup_sync_filter* vars = static_cast<mediasoup_sync_filter*>(data);

	if (vars)
		obs_add_main_render_callback(msoup_fsvideo_filter_offscreen_render, vars);
}

// Remove filter
static void msoup_fsvideo_filter_remove(void *data, obs_source_t *source)
{
	mediasoup_sync_filter* vars = static_cast<mediasoup_sync_filter*>(data);

	if (vars)
		obs_remove_main_render_callback(msoup_fsvideo_filter_offscreen_render, vars);
}

static obs_properties_t* msoup_fsvideo_properties(void* data)
{
	UNUSED_PARAMETER(data);
	return obs_properties_create();
}

static void msoup_fsvideo_filter_offscreen_render(void* param, uint32_t cx, uint32_t cy)
{
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);

	mediasoup_sync_filter* vars = static_cast<mediasoup_sync_filter*>(param);
	obs_source_t* target = obs_filter_get_parent(vars->source);

	if (target == nullptr)
		return;

	uint32_t width = obs_source_get_base_width(vars->source);
	uint32_t height = obs_source_get_base_height(vars->source);

	if (vars->width != width || vars->height != height)
	{
		// Destroy old
		gs_stagesurface_destroy(vars->stagesurface);
		gs_texrender_destroy(vars->texrender);

		// Create new
		vars->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
		vars->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
		vars->width = width;
		vars->height = height;
	}

	gs_texrender_reset(vars->texrender);

	// Draw the frame onto texrender
	if (gs_texrender_begin(vars->texrender, width, height))
	{
		struct vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(target);

		gs_blend_state_pop();
		gs_texrender_end(vars->texrender);
	}

	obs_source_frame sframe{};
	gs_stage_texture(vars->stagesurface, gs_texrender_get_texture(vars->texrender));
	
	// Get pointer to data from gs
	if (gs_stagesurface_map(vars->stagesurface, &sframe.data[0], &sframe.linesize[0]))
	{
		// Use the data
		sframe.width = vars->width;
		sframe.height = vars->height;
		sframe.flip = false;
		sframe.format = VIDEO_FORMAT_BGRA;
		msoup_fvideo_filter_video(obs_source_get_settings(vars->source), &sframe);

		// Release pointer to data from gs
		gs_stagesurface_unmap(vars->stagesurface);
	}
}

static void msoup_fsvideo_video_render(void* data, gs_effect_t* effect)
{
	UNUSED_PARAMETER(effect);
	mediasoup_sync_filter* vars = static_cast<mediasoup_sync_filter*>(data);
	obs_source_skip_video_filter(vars->source);
}

static void msoup_fsvideo_video_tick(void* data, float seconds)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(seconds);
}

static void msoup_fsvideo_update_settings(void* data, obs_data_t* settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

static void msoup_fsvideo_defaults(obs_data_t* settings)
{
	UNUSED_PARAMETER(settings);
}

bool obs_module_load(void)
{
	struct obs_source_info mediasoup_connector	= {};
	mediasoup_connector.id				= "mediasoupconnector";
	mediasoup_connector.type			= OBS_SOURCE_TYPE_INPUT;
	mediasoup_connector.icon_type			= OBS_ICON_TYPE_SLIDESHOW;
	mediasoup_connector.output_flags		= OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_DO_NOT_SELF_MONITOR;
	mediasoup_connector.get_name			= msoup_getname;

	mediasoup_connector.create			= msoup_create;
	mediasoup_connector.destroy			= msoup_destroy;

	mediasoup_connector.update			= msoup_update;
	mediasoup_connector.video_render		= msoup_video_render;
	mediasoup_connector.video_tick			= msoup_video_tick;
	mediasoup_connector.activate			= msoup_activate;

	mediasoup_connector.deactivate			= msoup_deactivate;
	mediasoup_connector.enum_active_sources		= msoup_enum_sources;

	mediasoup_connector.get_width			= msoup_width;
	mediasoup_connector.get_height			= msoup_height;
	mediasoup_connector.get_defaults		= msoup_defaults;
	mediasoup_connector.get_properties		= msoup_properties;

	obs_register_source(&mediasoup_connector);

	// Filter (Audio)
	struct obs_source_info mediasoup_filter_audio	= {};
	mediasoup_filter_audio.id			= "mediasoupconnector_afilter";
	mediasoup_filter_audio.type			= OBS_SOURCE_TYPE_FILTER;
	mediasoup_filter_audio.output_flags		= OBS_SOURCE_AUDIO;
	mediasoup_filter_audio.get_name			= msoup_faudio_name;
	mediasoup_filter_audio.create			= msoup_faudio_create;
	mediasoup_filter_audio.destroy			= msoup_faudio_destroy;
	mediasoup_filter_audio.update			= msoup_faudio_update;
	mediasoup_filter_audio.filter_audio		= msoup_faudio_filter_audio;
	mediasoup_filter_audio.get_properties		= msoup_faudio_properties;
	mediasoup_filter_audio.save			= msoup_faudio_save;
	
	obs_register_source(&mediasoup_filter_audio);
	
	// Filter (Video Async)
	struct obs_source_info mediasoup_filter_video	= {};
	mediasoup_filter_video.id			= "mediasoupconnector_vfilter";
	mediasoup_filter_video.type			= OBS_SOURCE_TYPE_FILTER;
	mediasoup_filter_video.output_flags		= OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
	mediasoup_filter_video.get_name			= msoup_fvideo_get_name;
	mediasoup_filter_video.create			= msoup_fvideo_create;
	mediasoup_filter_video.destroy			= msoup_fvideo_destroy;
	mediasoup_filter_video.update			= msoup_fvideo_update;
	mediasoup_filter_video.get_defaults		= msoup_fvideo_defaults;
	mediasoup_filter_video.get_properties		= msoup_fvideo_properties;
	mediasoup_filter_video.filter_video		= msoup_fvideo_filter_video;

	obs_register_source(&mediasoup_filter_video);
	
	// Filter (Video Sync)
	struct obs_source_info mediasoup_filter_video_s	= {};
	mediasoup_filter_video_s.id			= "mediasoupconnector_vsfilter";
	mediasoup_filter_video_s.type			= OBS_SOURCE_TYPE_FILTER;
	mediasoup_filter_video_s.output_flags		= OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
	mediasoup_filter_video_s.get_name		= msoup_fsvideo_get_name;
	mediasoup_filter_video_s.create			= msoup_fsvideo_create;
	mediasoup_filter_video_s.destroy		= msoup_fsvideo_destroy;
	mediasoup_filter_video_s.update			= msoup_fsvideo_update_settings;
	mediasoup_filter_video_s.get_defaults		= msoup_fsvideo_defaults;
	mediasoup_filter_video_s.get_properties		= msoup_fsvideo_properties,
	mediasoup_filter_video_s.video_tick		= msoup_fsvideo_video_tick;
	mediasoup_filter_video_s.video_render		= msoup_fsvideo_video_render;
	mediasoup_filter_video_s.filter_add		= msoup_fsvideo_filter_add;
	mediasoup_filter_video_s.filter_remove		= msoup_fsvideo_filter_remove;

	obs_register_source(&mediasoup_filter_video_s);
	return true;
}

#endif
