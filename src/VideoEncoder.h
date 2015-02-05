// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QMutex>
#include <QElapsedTimer>

extern "C"
{
#include <stdint.h>
#include "x264.h"
#include "libswscale/swscale.h"
}

namespace VideO
{
	class VideoDecoder;
	class Settings;
	struct FrameData;
	class Mp4File;

	// Encapsulate the x264 library for encoding video frames.
	class VideoEncoder
	{

	public:

		bool initialize(VideoDecoder* videoDecoder, Settings* settings);
		~VideoEncoder();

		void readFrameData(const FrameData& frameData);
		int encodeFrame();
		void close();

		double getEncodeDuration();

	private:

		QMutex encoderMutex;

		x264_t* encoder = nullptr;
		x264_picture_t* convertedPicture = nullptr;
		SwsContext* swsContext = nullptr;
		Mp4File* mp4File = nullptr;
		int64_t frameNumber = 0;

		QElapsedTimer encodeDurationTimer;
		double encodeDuration = 0.0;
	};
}
