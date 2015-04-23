// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include <cstdint>

#include <QtGlobal>

extern "C"
{
#include "x264.h"
#include "lsmash.h"
}

#include "Mp4File.h"

#define H264_NALU_LENGTH_SIZE 4
#define LOG_IF_ERR(cond, ...) if(cond) { qWarning(__VA_ARGS__); }
#define RETURN_IF_ERR(cond, ...) if(cond) { qWarning(__VA_ARGS__); return false; }

namespace VideO
{
	struct Mp4Handle
	{
		lsmash_root_t* root;
		lsmash_video_summary_t* summary;
		uint32_t movieTimescale;
		uint32_t videoTimescale;
		uint32_t track;
		uint32_t sampleEntry;
		uint64_t timeIncrement;
		int64_t startOffset;
		uint64_t firstCts;
		size_t seiSize;
		uint8_t* seiBuffer;
		int frameNumber;
		int64_t initDelta;
		lsmash_file_parameters_t fileParameters;
	};
}

using namespace VideO;

bool Mp4File::open(const QString& fileName)
{
	mp4Handle = (Mp4Handle*)calloc(1, sizeof(Mp4Handle));
	RETURN_IF_ERR(!mp4Handle, "Failed to allocate memory for muxer information");

	mp4Handle->root = lsmash_create_root();
	RETURN_IF_ERR(!mp4Handle->root, "Failed to create root");

	RETURN_IF_ERR(lsmash_open_file(fileName.toUtf8().constData(), 0, &mp4Handle->fileParameters) < 0, "Failed to open an output file");

	mp4Handle->summary = (lsmash_video_summary_t*)lsmash_create_summary(LSMASH_SUMMARY_TYPE_VIDEO);
	RETURN_IF_ERR(!mp4Handle->summary, "Failed to allocate memory for summary information of video");

	mp4Handle->summary->sample_type = ISOM_CODEC_TYPE_AVC1_VIDEO;

	return true;
}

bool Mp4File::setParameters(x264_param_t* param)
{
	uint64_t mediaTimescale = (uint64_t)param->i_timebase_den;
	mp4Handle->timeIncrement = (uint64_t)param->i_timebase_num;
	RETURN_IF_ERR(mediaTimescale > UINT32_MAX, "MP4 media timescale exceeds maximum");

	lsmash_brand_type brands[3] = { ISOM_BRAND_TYPE_MP42, ISOM_BRAND_TYPE_MP41, ISOM_BRAND_TYPE_ISOM };
	
	lsmash_file_parameters_t* fileParameters = &mp4Handle->fileParameters;
	fileParameters->major_brand = brands[0];
	fileParameters->brands = brands;
	fileParameters->brand_count = 3;
	fileParameters->minor_version = 0;
	RETURN_IF_ERR(!lsmash_set_file(mp4Handle->root, fileParameters), "Failed to add an output file into a ROOT");

	lsmash_movie_parameters_t movieParameters;
	lsmash_initialize_movie_parameters(&movieParameters);
	RETURN_IF_ERR(lsmash_set_movie_parameters(mp4Handle->root, &movieParameters), "Failed to set movie parameters");

	mp4Handle->movieTimescale = lsmash_get_movie_timescale(mp4Handle->root);
	RETURN_IF_ERR(!mp4Handle->movieTimescale, "Movie timescale is broken");

	mp4Handle->track = lsmash_create_track(mp4Handle->root, ISOM_MEDIA_HANDLER_TYPE_VIDEO_TRACK);
	RETURN_IF_ERR(!mp4Handle->track, "Failed to create a video track");

	mp4Handle->summary->width = param->i_width;
	mp4Handle->summary->height = param->i_height;
	mp4Handle->summary->color.primaries_index = param->vui.i_colorprim;
	mp4Handle->summary->color.transfer_index = param->vui.i_transfer;
	mp4Handle->summary->color.matrix_index = param->vui.i_colmatrix >= 0 ? param->vui.i_colmatrix : ISOM_MATRIX_INDEX_UNSPECIFIED;
	mp4Handle->summary->color.full_range = param->vui.b_fullrange >= 0 ? param->vui.b_fullrange : 0;

	lsmash_track_parameters_t trackParameters;
	lsmash_initialize_track_parameters(&trackParameters);
	trackParameters.mode = (lsmash_track_mode)(ISOM_TRACK_ENABLED | ISOM_TRACK_IN_MOVIE | ISOM_TRACK_IN_PREVIEW);
	trackParameters.display_width = param->i_width << 16;
	trackParameters.display_height = param->i_height << 16;
	RETURN_IF_ERR(lsmash_set_track_parameters(mp4Handle->root, mp4Handle->track, &trackParameters), "Failed to set track parameters for video");

	lsmash_media_parameters_t mediaParameters;
	lsmash_initialize_media_parameters(&mediaParameters);
	mediaParameters.timescale = mediaTimescale;
	mediaParameters.media_handler_name = (char*)"VideO";
	RETURN_IF_ERR(lsmash_set_media_parameters(mp4Handle->root, mp4Handle->track, &mediaParameters), "Failed to set media parameters for video");

	mp4Handle->videoTimescale = lsmash_get_media_timescale(mp4Handle->root, mp4Handle->track);
	RETURN_IF_ERR(!mp4Handle->videoTimescale, "Media timescale for video is broken");

	return true;
}

bool Mp4File::writeHeaders(x264_nal_t* nal)
{
	uint8_t* sps = nal[0].p_payload + H264_NALU_LENGTH_SIZE;
	uint8_t* pps = nal[1].p_payload + H264_NALU_LENGTH_SIZE;
	uint8_t* sei = nal[2].p_payload;
	uint32_t spsSize = nal[0].i_payload - H264_NALU_LENGTH_SIZE;
	uint32_t ppsSize = nal[1].i_payload - H264_NALU_LENGTH_SIZE;
	size_t seiSize = (size_t)nal[2].i_payload;

	lsmash_codec_specific_t* cs = lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED);
	lsmash_h264_specific_parameters_t* param = (lsmash_h264_specific_parameters_t*)cs->data.structured;
	param->lengthSizeMinusOne = H264_NALU_LENGTH_SIZE - 1;

	RETURN_IF_ERR(lsmash_append_h264_parameter_set(param, H264_PARAMETER_SET_TYPE_SPS, sps, spsSize), "Failed to append SPS");
	RETURN_IF_ERR(lsmash_append_h264_parameter_set(param, H264_PARAMETER_SET_TYPE_PPS, pps, ppsSize), "Failed to append PPS");
	RETURN_IF_ERR(lsmash_add_codec_specific_data((lsmash_summary_t*)mp4Handle->summary, cs), "Failed to add H.264 specific info");

	lsmash_destroy_codec_specific_data(cs);
	cs = lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED);
	lsmash_add_codec_specific_data((lsmash_summary_t*)mp4Handle->summary, cs);
	lsmash_destroy_codec_specific_data(cs);

	mp4Handle->sampleEntry = lsmash_add_sample_entry(mp4Handle->root, mp4Handle->track, mp4Handle->summary);
	RETURN_IF_ERR(!mp4Handle->sampleEntry, "Failed to add sample entry for video");

	mp4Handle->seiBuffer = (uint8_t*)malloc(seiSize);
	RETURN_IF_ERR(!mp4Handle->seiBuffer, "Failed to allocate sei transition buffer");

	memcpy(mp4Handle->seiBuffer, sei, seiSize);
	mp4Handle->seiSize = seiSize;

	return true;
}

bool Mp4File::writeFrame(uint8_t* payload, size_t size, x264_picture_t* picture)
{
	if (!mp4Handle->frameNumber)
	{
		mp4Handle->startOffset = picture->i_dts * -1;
		mp4Handle->firstCts = mp4Handle->startOffset * mp4Handle->timeIncrement;
	}

	lsmash_sample_t* p_sample = lsmash_create_sample((uint32_t)(size + mp4Handle->seiSize));
	RETURN_IF_ERR(!p_sample, "Failed to create a video sample data");

	if (mp4Handle->seiBuffer)
	{
		memcpy(p_sample->data, mp4Handle->seiBuffer, mp4Handle->seiSize);
		free(mp4Handle->seiBuffer);
		mp4Handle->seiBuffer = nullptr;
	}

	memcpy(p_sample->data + mp4Handle->seiSize, payload, size);
	mp4Handle->seiSize = 0;

	p_sample->dts = (picture->i_dts + mp4Handle->startOffset) * mp4Handle->timeIncrement;
	p_sample->cts = (picture->i_pts + mp4Handle->startOffset) * mp4Handle->timeIncrement;
	p_sample->index = mp4Handle->sampleEntry;
	p_sample->prop.ra_flags = picture->b_keyframe ? ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC : ISOM_SAMPLE_RANDOM_ACCESS_FLAG_NONE;

	RETURN_IF_ERR(lsmash_append_sample(mp4Handle->root, mp4Handle->track, p_sample), "Failed to append a video frame");

	mp4Handle->frameNumber++;

	return true;
}

void Mp4File::close(int64_t lastPts)
{
	if (mp4Handle != nullptr)
	{
		if (mp4Handle->root)
		{
			if (mp4Handle->track)
			{
				LOG_IF_ERR(lsmash_flush_pooled_samples(mp4Handle->root, mp4Handle->track, mp4Handle->timeIncrement), "Failed to flush the rest of samples");

				double actualDuration = 0;

				if (mp4Handle->movieTimescale != 0 && mp4Handle->videoTimescale != 0)
					actualDuration = ((double)(lastPts * mp4Handle->timeIncrement) / mp4Handle->videoTimescale) * mp4Handle->movieTimescale;
				else
					qWarning("Timescale is broken");

				lsmash_edit_t edit;
				edit.duration = actualDuration;
				edit.start_time = mp4Handle->firstCts;
				edit.rate = ISOM_EDIT_MODE_NORMAL;
				LOG_IF_ERR(lsmash_create_explicit_timeline_map(mp4Handle->root, mp4Handle->track, edit), "Failed to set timeline map for video");
			}

			LOG_IF_ERR(lsmash_finish_movie(mp4Handle->root, nullptr), "Failed to finish movie");
		}

		lsmash_cleanup_summary((lsmash_summary_t*)mp4Handle->summary);
		lsmash_close_file(&mp4Handle->fileParameters);
		lsmash_destroy_root(mp4Handle->root);

		free(mp4Handle->seiBuffer);
		free(mp4Handle);

		mp4Handle = nullptr;
	}
}
