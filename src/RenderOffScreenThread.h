// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QThread>
#include <QSemaphore>

#include "FrameData.h"

namespace VideO
{
	class MainWindow;
	class EncodeWindow;
	class VideoDecoder;
	class VideoDecoderThread;
	class VideoStabilizer;
	class RouteManager;
	class Renderer;
	class VideoEncoder;

	// Run renderer on a thread and draw to hidden framebuffers.
	class RenderOffScreenThread : public QThread
	{
		Q_OBJECT

	public:

		void initialize(MainWindow* mainWindow, EncodeWindow* encodeWindow, VideoDecoder* videoDecoder, VideoDecoderThread* videoDecoderThread, VideoStabilizer* videoStabilizer, RouteManager* routeManager, Renderer* renderer, VideoEncoder* videoEncoder);
		~RenderOffScreenThread();

		bool tryGetNextFrame(FrameData& frameData, int timeout);
		void signalFrameRead();

	protected:

		void run();

	private:

		MainWindow* mainWindow = nullptr;
		EncodeWindow* encodeWindow = nullptr;
		VideoDecoder* videoDecoder = nullptr;
		VideoDecoderThread* videoDecoderThread = nullptr;
		VideoStabilizer* videoStabilizer = nullptr;
		RouteManager* routeManager = nullptr;
		Renderer* renderer = nullptr;
		VideoEncoder* videoEncoder = nullptr;

		QSemaphore* frameReadSemaphore = nullptr;
		QSemaphore* frameAvailableSemaphore = nullptr;

		FrameData renderedFrameData;
	};
}
