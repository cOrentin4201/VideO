// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QThread>

namespace VideO
{
	class VideoDecoder;
	class VideoEncoder;
	class RenderOffScreenThread;

	// Run video encoder on a thread.
	class VideoEncoderThread : public QThread
	{
		Q_OBJECT

	public:

		void initialize(VideoDecoder* videoDecoder, VideoEncoder* videoEncoder, RenderOffScreenThread* renderOffScreenThread);

		void togglePaused();
		bool getIsPaused() const;

	signals:

		void frameProcessed(int frameNumber, int frameSize, double currentTime);
		void encodingFinished();

	protected:

		void run();

	private:

		VideoDecoder* videoDecoder = nullptr;
		VideoEncoder* videoEncoder = nullptr;
		RenderOffScreenThread* renderOffScreenThread = nullptr;

		bool isPaused = false;
	};
}
