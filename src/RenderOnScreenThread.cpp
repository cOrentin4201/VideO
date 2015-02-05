// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include <QElapsedTimer>

#include "RenderOnScreenThread.h"
#include "MainWindow.h"
#include "VideoWindow.h"
#include "VideoDecoder.h"
#include "VideoDecoderThread.h"
#include "VideoStabilizer.h"
#include "RouteManager.h"
#include "Renderer.h"
#include "InputHandler.h"
#include "Settings.h"

using namespace VideO;

void RenderOnScreenThread::initialize(MainWindow* mainWindow, VideoWindow* videoWindow, VideoDecoder* videoDecoder, VideoDecoderThread* videoDecoderThread, VideoStabilizer* videoStabilizer, RouteManager* routeManager, Renderer* renderer, InputHandler* inputHandler)
{
	this->mainWindow = mainWindow;
	this->videoWindow = videoWindow;
	this->videoDecoder = videoDecoder;
	this->videoDecoderThread = videoDecoderThread;
	this->videoStabilizer = videoStabilizer;
	this->routeManager = routeManager;
	this->renderer = renderer;
	this->inputHandler = inputHandler;
}

void RenderOnScreenThread::run()
{
	FrameData frameData;
	FrameData frameDataGrayscale;

	QElapsedTimer frameDurationTimer;
	double frameDuration = 30.0;
	double spareTime = 15.0;

	frameDurationTimer.start();

	while (!isInterruptionRequested())
	{
		if (!videoWindow->isExposed())
		{
			QThread::msleep(100);
			continue;
		}

		bool gotFrame = false;

		if (!isPaused || shouldAdvanceOneFrame)
		{
			gotFrame = videoDecoderThread->tryGetNextFrame(frameData, frameDataGrayscale, 0);
			shouldAdvanceOneFrame = false;
		}

		if (gotFrame)
			videoStabilizer->processFrame(frameDataGrayscale);

		videoWindow->getContext()->makeCurrent(videoWindow);
		renderer->startRendering(videoDecoder->getCurrentTime(), frameDuration, videoDecoder->getDecodeDuration(), videoStabilizer->getProcessDuration(), 0.0, spareTime);

		videoDecoder->resetDecodeDuration();
		videoStabilizer->resetProcessDuration();

		if (gotFrame)
		{
			renderer->uploadFrameData(frameData);
			videoDecoderThread->signalFrameRead();
		}

		renderer->renderAll();
		renderer->stopRendering();

		routeManager->update(videoDecoder->getCurrentTime(), frameDuration);
		inputHandler->handleInput(frameDuration);

		if (windowHasBeenResized)
		{
			renderer->windowResized(windowWidth, windowHeight);
			routeManager->windowResized(windowWidth, windowHeight);
			windowHasBeenResized = false;
		}

		videoWindow->getContext()->swapBuffers(videoWindow);

		if (gotFrame)
		{
			spareTime = (frameData.duration - (frameDurationTimer.nsecsElapsed() / 1000.0)) / 1000.0;

			// use combination of normal and spinning wait to sync the frame rate accurately
			while (true)
			{
				int64_t timeToSleep = frameData.duration - (frameDurationTimer.nsecsElapsed() / 1000);

				if (timeToSleep > 2000)
				{
					QThread::msleep(1);
					continue;
				}
				else if (timeToSleep > 0)
					continue;
				else
					break;
			}
		}
		else
			spareTime = 0.0;

		frameDuration = frameDurationTimer.nsecsElapsed() / 1000000.0;
		frameDurationTimer.restart();
	}

	videoWindow->getContext()->doneCurrent();
	videoWindow->getContext()->moveToThread(mainWindow->thread());
}

bool RenderOnScreenThread::getIsPaused()
{
	return isPaused;
}

void RenderOnScreenThread::togglePaused()
{
	isPaused = !isPaused;
	shouldAdvanceOneFrame = false;
}

void RenderOnScreenThread::advanceOneFrame()
{
	shouldAdvanceOneFrame = true;
}

void RenderOnScreenThread::windowResized(int newWidth, int newHeight)
{
	windowWidth = newWidth;
	windowHeight = newHeight;

	windowHasBeenResized = true;
}
