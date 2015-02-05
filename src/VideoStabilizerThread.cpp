// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include "VideoStabilizerThread.h"
#include "VideoDecoder.h"
#include "VideoStabilizer.h"
#include "Settings.h"
#include "FrameData.h"

using namespace VideO;

bool VideoStabilizerThread::initialize(VideoDecoder* videoDecoder, VideoStabilizer* videoStabilizer, Settings* settings)
{
	this->videoDecoder = videoDecoder;
	this->videoStabilizer = videoStabilizer;

	return true;
}

void VideoStabilizerThread::togglePaused()
{
	isPaused = !isPaused;
}

bool VideoStabilizerThread::getIsPaused() const
{
	return isPaused;
}

void VideoStabilizerThread::run()
{
	FrameData frameDataGrayscale;

	while (!isInterruptionRequested())
	{
		if (isPaused)
		{
			QThread::msleep(100);
			continue;
		}

		if (videoDecoder->getNextFrame(nullptr, &frameDataGrayscale))
		{
			videoStabilizer->preProcessFrame(frameDataGrayscale, outputFile);
			emit frameProcessed(frameDataGrayscale.cumulativeNumber, videoDecoder->getCurrentTime());
		}
		else if (videoDecoder->getIsFinished())
			break;
	}

	if (outputFile.isOpen())
		outputFile.close();

	emit processingFinished();
}
