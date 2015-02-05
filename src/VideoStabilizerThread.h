// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QThread>
#include <QFile>

namespace VideO
{
	class VideoDecoder;
	class VideoStabilizer;
	class Settings;

	class VideoStabilizerThread : public QThread
	{
		Q_OBJECT

	public:

		bool initialize(VideoDecoder* videoDecoder, VideoStabilizer* videoStabilizer, Settings* settings);

		void togglePaused();
		bool getIsPaused() const;

	signals:

		void frameProcessed(int frameNumber, double currentTime);
		void processingFinished();

	protected:

		void run();

	private:

		VideoDecoder* videoDecoder = nullptr;
		VideoStabilizer* videoStabilizer = nullptr;

		QFile outputFile;

		bool isPaused = false;
	};
}
