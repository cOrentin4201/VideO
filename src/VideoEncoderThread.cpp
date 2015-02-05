// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include "VideoEncoderThread.h"
#include "VideoDecoder.h"
#include "VideoEncoder.h"
#include "RenderOffScreenThread.h"
#include "FrameData.h"

using namespace VideO;

void VideoEncoderThread::initialize(VideoDecoder* videoDecoder, VideoEncoder* videoEncoder, RenderOffScreenThread* renderOffScreenThread)
{
	this->videoDecoder = videoDecoder;
	this->videoEncoder = videoEncoder;
	this->renderOffScreenThread = renderOffScreenThread;
}

void VideoEncoderThread::togglePaused()
{
	isPaused = !isPaused;
}

bool VideoEncoderThread::getIsPaused() const
{
	return isPaused;
}

void VideoEncoderThread::run()
{
	FrameData renderedFrameData;

	while (!isInterruptionRequested())
	{
		if (isPaused)
		{
			QThread::msleep(100);
			continue;
		}

		if (renderOffScreenThread->tryGetNextFrame(renderedFrameData, 100))
		{
			videoEncoder->readFrameData(renderedFrameData);
			renderOffScreenThread->signalFrameRead();
			int frameSize = videoEncoder->encodeFrame();

			emit frameProcessed(renderedFrameData.cumulativeNumber, frameSize, videoDecoder->getCurrentTime());
		}
		else if (videoDecoder->getIsFinished())
			break;
	}

	videoEncoder->close();
	emit encodingFinished();
}
