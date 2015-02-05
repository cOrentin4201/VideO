// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDesktopServices>

#include "StabilizeWindow.h"
#include "VideoDecoder.h"
#include "VideoStabilizerThread.h"
#include "Settings.h"

using namespace VideO;


StabilizeWindow::~StabilizeWindow()
{
	
}

bool StabilizeWindow::initialize(VideoDecoder* videoDecoder, VideoStabilizerThread* videoStabilizerThread)
{
	this->videoStabilizerThread = videoStabilizerThread;

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	resize(10, 10);

	totalFrameCount = videoDecoder->getTotalFrameCount();

	QTime totalVideoDuration = QTime(0, 0, 0, 0).addMSecs(videoDecoder->getTotalDuration() * 1000.0);

	startTime.start();

	return true;
}

void StabilizeWindow::frameProcessed(int frameNumber, double currentTime)
{
	int value = (int)round((double)frameNumber / totalFrameCount * 1000.0);


	int elapsedTimeMs = startTime.elapsed() - totalPauseTime;
	double timePerFrameMs = (double)elapsedTimeMs / frameNumber;
	double framesPerSecond = 1.0 / timePerFrameMs * 1000.0;
	int totalTimeMs = (int)round(timePerFrameMs * totalFrameCount);
	int remainingTimeMs = totalTimeMs - elapsedTimeMs;

	if (remainingTimeMs < 0)
		remainingTimeMs = 0;

	QTime elapsedTime = QTime(0, 0, 0, 0).addMSecs(elapsedTimeMs);
	QTime remainingTime = QTime(0, 0, 0, 0).addMSecs(remainingTimeMs);
	QTime totalTime = QTime(0, 0, 0, 0).addMSecs(totalTimeMs);
	QTime currentVideoTime = QTime(0, 0, 0, 0).addMSecs(currentTime * 1000.0);

}

void StabilizeWindow::processingFinished()
{
	isRunning = false;
}

void StabilizeWindow::on_pushButtonPauseContinue_clicked()
{
	videoStabilizerThread->togglePaused();

	if (videoStabilizerThread->getIsPaused())
	{
		pauseTime.restart();
	}
	else
	{
		totalPauseTime += pauseTime.elapsed();
	}
}

void StabilizeWindow::on_pushButtonStopClose_clicked()
{
	if (isRunning)
	{
		videoStabilizerThread->requestInterruption();
		videoStabilizerThread->wait();
	}
	else
		close();
}

bool StabilizeWindow::event(QEvent* event)
{
	return QDialog::event(event);
}
