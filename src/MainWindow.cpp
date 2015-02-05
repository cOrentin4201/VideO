// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include <stdexcept>

#include <QFileDialog>
#include <QtGui>
#include <QMessageBox>
#include <QColorDialog>
#include <QProgressDialog>

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Settings.h"
#include "VideoWindow.h"
#include "EncodeWindow.h"
#include "StabilizeWindow.h"
#include "VideoDecoder.h"
#include "VideoEncoder.h"
#include "QuickRouteReader.h"
#include "MapImageReader.h"
#include "VideoStabilizer.h"
#include "InputHandler.h"
#include "RouteManager.h"
#include "Renderer.h"
#include "VideoDecoderThread.h"
#include "RenderOnScreenThread.h"
#include "RenderOffScreenThread.h"
#include "VideoEncoderThread.h"
#include "VideoStabilizerThread.h"

using namespace VideO;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	resize(700, 500);

	settings = new Settings();
	
	ui->comboBoxRouteManagerViewMode->view()->setMinimumSize(200, 0);
}

MainWindow::~MainWindow()
{
	if (settings != nullptr)
	{
		delete settings;
		settings = nullptr;
	}

	if (ui != nullptr)
	{
		delete ui;
		ui = nullptr;
	}
}

void MainWindow::readSettingsFromLocal()
{
	QSettings localSettings;
	settings->readFromQSettings(&localSettings);
	settings->writeToUI(ui);
}

void MainWindow::writeSettingsToLocal()
{
	QSettings localSettings;
	settings->readFromUI(ui);
	settings->writeToQSettings(&localSettings);
}

void MainWindow::readSettingsFromIniFile(const QString& fileName)
{
	QSettings iniFileSettings(fileName, QSettings::IniFormat);
	settings->readFromQSettings(&iniFileSettings);
	settings->writeToUI(ui);
}

void MainWindow::writeSettingsToIniFile(const QString& fileName)
{
	QSettings iniFileSettings(fileName, QSettings::IniFormat);
	settings->readFromUI(ui);
	settings->writeToQSettings(&iniFileSettings);
}


void MainWindow::on_actionLoadSettings_triggered()
{
	QFileDialog fileDialog(this);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setWindowTitle(tr("Load VideO settings"));
	fileDialog.setNameFilter(tr("VideO settings files (*.vdo)"));

	if (fileDialog.exec())
		readSettingsFromIniFile(fileDialog.selectedFiles().at(0));
}

void MainWindow::on_actionSaveSettings_triggered()
{
	QFileDialog fileDialog(this);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setWindowTitle(tr("Save VideO settings"));
	fileDialog.setNameFilter(tr("VideO settings files (*.vdo)"));
	fileDialog.setDefaultSuffix(tr("vdo"));
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);

	if (fileDialog.exec())
		writeSettingsToIniFile(fileDialog.selectedFiles().at(0));
}

void MainWindow::on_actionDefaultSettings_triggered()
{
	if (QMessageBox::warning(this, "VideO - Warning", QString("Do you really want reset all settings to defaults?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
	{
		if (settings != nullptr)
		{
			delete settings;
			settings = nullptr;
		}

		settings = new Settings();
		settings->writeToUI(ui);
	}
}

void MainWindow::on_actionPlayVideo_triggered()
{
	this->setCursor(Qt::WaitCursor);

	settings->readFromUI(ui);

	try
	{
		videoDecoder = new VideoDecoder();

		if (!videoDecoder->initialize(settings))
		{
			if (QMessageBox::warning(this, "VideO - Warning", QString("Could not open the video file.\n\nDo you want to continue anyway?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				throw std::runtime_error("Could not initialize video decoder");
		}

		mapImageReader = new MapImageReader();

		if (!mapImageReader->initialize(settings))
		{
			if (QMessageBox::warning(this, "VideO - Warning", QString("Could not read the map image file.\n\nDo you want to continue anyway?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				throw std::runtime_error("Could not initialize map image reader");
		}

		quickRouteReader = new QuickRouteReader();

		if (!quickRouteReader->initialize(mapImageReader, settings))
		{
			if (QMessageBox::warning(this, "VideO - Warning", QString("Could not read the QuickRoute file.\n\nDo you want to continue anyway?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				throw std::runtime_error("Could not initialize QuickRoute reader");
		}
	}
	catch (const std::exception& ex)
	{
		qWarning("%s", ex.what());

		this->setCursor(Qt::ArrowCursor);
		playVideoFinished();

		return;
	}

	try
	{
		videoWindow = new VideoWindow();
		renderer = new Renderer();
		videoStabilizer = new VideoStabilizer();
		inputHandler = new InputHandler();
		routeManager = new RouteManager();
		videoDecoderThread = new VideoDecoderThread();
		renderOnScreenThread = new RenderOnScreenThread();

		videoWindow->show();

		if (!videoWindow->initialize(settings))
			throw std::runtime_error("Could not initialize video window");

		if (!renderer->initialize(videoDecoder, mapImageReader, videoStabilizer, inputHandler, routeManager, settings, false))
			throw std::runtime_error("Could not initialize renderer");

		if (!videoStabilizer->initialize(settings, false))
			throw std::runtime_error("Could not initialize video stabilizer");

		inputHandler->initialize(videoWindow, renderer, videoDecoder, videoDecoderThread, videoStabilizer, routeManager, renderOnScreenThread, settings);

		if (!routeManager->initialize(quickRouteReader, renderer, settings))
			throw std::runtime_error("Could not initialize route manager");

		videoDecoderThread->initialize(videoDecoder);
		renderOnScreenThread->initialize(this, videoWindow, videoDecoder, videoDecoderThread, videoStabilizer, routeManager, renderer, inputHandler);

		connect(videoWindow, &VideoWindow::closing, this, &MainWindow::playVideoFinished);
		connect(videoWindow, &VideoWindow::resizing, renderOnScreenThread, &RenderOnScreenThread::windowResized);

		videoWindow->getContext()->doneCurrent();
		videoWindow->getContext()->moveToThread(renderOnScreenThread);

		videoDecoderThread->start();
		renderOnScreenThread->start();

		this->hide();
	}
	catch (const std::exception& ex)
	{
		qWarning("%s", ex.what());

		videoWindow->close();
		playVideoFinished();

		//QMessageBox::critical(this, "OrientView - Error", QString("%1.\n\nCheck the application log for details.").arg(ex.what()), QMessageBox::Ok);
	}

	this->setCursor(Qt::ArrowCursor);
}

void MainWindow::playVideoFinished()
{
	if (renderOnScreenThread != nullptr)
	{
		renderOnScreenThread->requestInterruption();
		renderOnScreenThread->wait();
		delete renderOnScreenThread;
		renderOnScreenThread = nullptr;
	}

	if (videoDecoderThread != nullptr)
	{
		videoDecoderThread->requestInterruption();
		videoDecoderThread->wait();
		delete videoDecoderThread;
		videoDecoderThread = nullptr;
	}

	if (videoWindow != nullptr && videoWindow->getIsInitialized())
		videoWindow->getContext()->makeCurrent(videoWindow);

	if (routeManager != nullptr)
	{
		delete routeManager;
		routeManager = nullptr;
	}

	
	if (inputHandler != nullptr)
	{
		delete inputHandler;
		inputHandler = nullptr;
	}

	if (videoStabilizer != nullptr)
	{
		delete videoStabilizer;
		videoStabilizer = nullptr;
	}

	if (renderer != nullptr)
	{
		delete renderer;
		renderer = nullptr;
	}

	if (videoWindow != nullptr)
	{
		videoWindow->deleteLater();
		videoWindow = nullptr;
	}

	if (mapImageReader != nullptr)
	{
		delete mapImageReader;
		mapImageReader = nullptr;
	}

	if (quickRouteReader != nullptr)
	{
		delete quickRouteReader;
		quickRouteReader = nullptr;
	}

	if (videoDecoder != nullptr)
	{
		delete videoDecoder;
		videoDecoder = nullptr;
	}

	this->show();
	this->activateWindow();
}

void MainWindow::on_actionEncodeVideo_triggered()
{
	this->setCursor(Qt::WaitCursor);

	settings->readFromUI(ui);

	try
	{
		videoDecoder = new VideoDecoder();

		if (!videoDecoder->initialize(settings))
		{
			if (QMessageBox::warning(this, "VideO - Warning", QString("Could not open the video file.\n\nDo you want to continue anyway?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				throw std::runtime_error("Could not initialize video decoder");
		}

		mapImageReader = new MapImageReader();

		if (!mapImageReader->initialize(settings))
		{
			if (QMessageBox::warning(this, "VideO - Warning", QString("Could not read the map image file.\n\nDo you want to continue anyway?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				throw std::runtime_error("Could not initialize map image reader");
		}

		quickRouteReader = new QuickRouteReader();

		if (!quickRouteReader->initialize(mapImageReader, settings))
		{
			if (QMessageBox::warning(this, "VideO - Warning", QString("Could not read the QuickRoute file.\n\nDo you want to continue anyway?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				throw std::runtime_error("Could not initialize QuickRoute reader");
		}
	}
	catch (const std::exception& ex)
	{
		qWarning("%s", ex.what());

		this->setCursor(Qt::ArrowCursor);
		encodeVideoFinished();

		return;
	}

	try
	{
		encodeWindow = new EncodeWindow(this);
		videoEncoder = new VideoEncoder();
		renderer = new Renderer();
		videoStabilizer = new VideoStabilizer();
		inputHandler = new InputHandler();
		routeManager = new RouteManager();
		videoDecoderThread = new VideoDecoderThread();
		renderOffScreenThread = new RenderOffScreenThread();
		videoEncoderThread = new VideoEncoderThread();

		if (!encodeWindow->initialize(videoDecoder, videoEncoderThread, settings))
			throw std::runtime_error("Could not initialize encode window");

		if (!videoEncoder->initialize(videoDecoder, settings))
			throw std::runtime_error("Could not initialize video encoder");

		if (!renderer->initialize(videoDecoder, mapImageReader, videoStabilizer, inputHandler, routeManager, settings, true))
			throw std::runtime_error("Could not initialize renderer");

		if (!videoStabilizer->initialize(settings, false))
			throw std::runtime_error("Could not initialize video stabilizer");

		
		if (!routeManager->initialize(quickRouteReader, renderer, settings))
			throw std::runtime_error("Could not initialize route manager");

		videoDecoderThread->initialize(videoDecoder);
		renderOffScreenThread->initialize(this, encodeWindow, videoDecoder, videoDecoderThread, videoStabilizer, routeManager, renderer, videoEncoder);
		videoEncoderThread->initialize(videoDecoder, videoEncoder, renderOffScreenThread);

		connect(encodeWindow, &EncodeWindow::closing, this, &MainWindow::encodeVideoFinished);
		connect(videoEncoderThread, &VideoEncoderThread::frameProcessed, encodeWindow, &EncodeWindow::frameProcessed);
		connect(videoEncoderThread, &VideoEncoderThread::encodingFinished, encodeWindow, &EncodeWindow::encodingFinished);

		encodeWindow->setModal(true);
		encodeWindow->show();

		encodeWindow->getContext()->doneCurrent();
		encodeWindow->getContext()->moveToThread(renderOffScreenThread);

		videoDecoderThread->start();
		renderOffScreenThread->start();
		videoEncoderThread->start();
	}
	catch (const std::exception& ex)
	{
		qWarning("%s", ex.what());

		encodeWindow->close();
		encodeVideoFinished();

		//QMessageBox::critical(this, "OrientView - Error", QString("%1.\n\nCheck the application log for details.").arg(ex.what()), QMessageBox::Ok);
	}

	this->setCursor(Qt::ArrowCursor);
}

void MainWindow::encodeVideoFinished()
{
	if (videoEncoderThread != nullptr)
	{
		videoEncoderThread->requestInterruption();
		videoEncoderThread->wait();
		delete videoEncoderThread;
		videoEncoderThread = nullptr;
	}

	if (renderOffScreenThread != nullptr)
	{
		renderOffScreenThread->requestInterruption();
		renderOffScreenThread->wait();
		delete renderOffScreenThread;
		renderOffScreenThread = nullptr;
	}

	if (videoDecoderThread != nullptr)
	{
		videoDecoderThread->requestInterruption();
		videoDecoderThread->wait();
		delete videoDecoderThread;
		videoDecoderThread = nullptr;
	}

	if (encodeWindow != nullptr && encodeWindow->getIsInitialized())
		encodeWindow->getContext()->makeCurrent(encodeWindow->getSurface());

	if (routeManager != nullptr)
	{
		delete routeManager;
		routeManager = nullptr;
	}

	
	if (inputHandler != nullptr)
	{
		delete inputHandler;
		inputHandler = nullptr;
	}

	if (videoStabilizer != nullptr)
	{
		delete videoStabilizer;
		videoStabilizer = nullptr;
	}

	if (renderer != nullptr)
	{
		delete renderer;
		renderer = nullptr;
	}

	if (videoEncoder != nullptr)
	{
		delete videoEncoder;
		videoEncoder = nullptr;
	}

	if (encodeWindow != nullptr)
	{
		encodeWindow->deleteLater();
		encodeWindow = nullptr;
	}

	if (mapImageReader != nullptr)
	{
		delete mapImageReader;
		mapImageReader = nullptr;
	}

	if (quickRouteReader != nullptr)
	{
		delete quickRouteReader;
		quickRouteReader = nullptr;
	}

	if (videoDecoder != nullptr)
	{
		delete videoDecoder;
		videoDecoder = nullptr;
	}
}


void MainWindow::on_actionExit_triggered()
{
	close();
}

void MainWindow::on_pushButtonBrowseMapImageFile_clicked()
{
	QFileDialog fileDialog(this);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setWindowTitle(tr("Select map image file"));
	fileDialog.setNameFilter(tr("Image files (*.jpg *.png *.tiff *.tif);;All files (*.*)"));

	if (fileDialog.exec())
		ui->lineEditMapImageFile->setText(fileDialog.selectedFiles().at(0));
}

void MainWindow::on_pushButtonBrowseQuickRouteJpegFile_clicked()
{
	QFileDialog fileDialog(this);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setWindowTitle(tr("Select QuickRoute JPEG file"));
	fileDialog.setNameFilter(tr("QuickRoute JPEG files (*.jpg);;All files (*.*)"));

	if (fileDialog.exec())
		ui->lineEditQuickRouteJpegFile->setText(fileDialog.selectedFiles().at(0));
}

void MainWindow::on_pushButtonBrowseInputVideoFile_clicked()
{
	QFileDialog fileDialog(this);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setWindowTitle(tr("Select input video file"));
	fileDialog.setNameFilter(tr("Video files (*.mp4 *.avi *.mkv);;All files (*.*)"));

	if (fileDialog.exec())
		ui->lineEditInputVideoFile->setText(fileDialog.selectedFiles().at(0));
}

void MainWindow::on_pushButtonBrowseOutputVideoFile_clicked()
{
	QFileDialog fileDialog(this);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setWindowTitle(tr("Select output video file"));
	fileDialog.setNameFilter(tr("MP4 video files (*.mp4)"));
	fileDialog.setDefaultSuffix(tr("mp4"));
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);

	if (fileDialog.exec())
		ui->lineEditOutputVideoFile->setText(fileDialog.selectedFiles().at(0));
}

void MainWindow::on_pushButtonPickMapBackgroundColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->map.backgroundColor, this, "Pick map background color");

	if (resultColor.isValid())
		settings->map.backgroundColor = resultColor;

	settings->writeToUI(ui);
}

void MainWindow::on_pushButtonPickRouteDiscreetColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->route.discreetColor, this, "Pick route discreet color", QColorDialog::ColorDialogOption::ShowAlphaChannel);

	if (resultColor.isValid())
		settings->route.discreetColor = resultColor;

	settings->writeToUI(ui);
}

void MainWindow::on_pushButtonPickRouteHighlightColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->route.highlightColor, this, "Pick route highlight color", QColorDialog::ColorDialogOption::ShowAlphaChannel);

	if (resultColor.isValid())
		settings->route.highlightColor = resultColor;

	settings->writeToUI(ui);
}

void MainWindow::on_pushButtonPickRouteRunnerColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->route.runnerColor, this, "Pick runner color", QColorDialog::ColorDialogOption::ShowAlphaChannel);

	if (resultColor.isValid())
		settings->route.runnerColor = resultColor;

	settings->writeToUI(ui);
}

void MainWindow::on_pushButtonPickRouteRunnerBorderColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->route.runnerBorderColor, this, "Pick runner border color", QColorDialog::ColorDialogOption::ShowAlphaChannel);

	if (resultColor.isValid())
		settings->route.runnerBorderColor = resultColor;

	settings->writeToUI(ui);
}

void MainWindow::on_pushButtonPickRouteControlBorderColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->route.controlBorderColor, this, "Pick control border color", QColorDialog::ColorDialogOption::ShowAlphaChannel);

	if (resultColor.isValid())
		settings->route.controlBorderColor = resultColor;

	settings->writeToUI(ui);
}

void MainWindow::on_pushButtonPickVideoBackgroundColor_clicked()
{
	QColorDialog colorDialog;
	QColor resultColor = colorDialog.getColor(settings->video.backgroundColor, this, "Pick video background color");

	if (resultColor.isValid())
		settings->video.backgroundColor = resultColor;

	settings->writeToUI(ui);
}


void MainWindow::stabilizeVideoFinished()
{
	if (videoStabilizerThread != nullptr)
	{
		videoStabilizerThread->requestInterruption();
		videoStabilizerThread->wait();
		delete videoStabilizerThread;
		videoStabilizerThread = nullptr;
	}

	if (videoStabilizer != nullptr)
	{
		delete videoStabilizer;
		videoStabilizer = nullptr;
	}

	if (videoDecoder != nullptr)
	{
		delete videoDecoder;
		videoDecoder = nullptr;
	}

	if (stabilizeWindow != nullptr)
	{
		stabilizeWindow->deleteLater();
		stabilizeWindow = nullptr;
	}
}


