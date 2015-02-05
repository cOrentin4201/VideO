// Copyright © 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <QElapsedTimer>

namespace VideO
{
	class VideoWindow;
	class Renderer;
	class VideoDecoder;
	class VideoDecoderThread;
	class VideoStabilizer;
	class RouteManager;
	class RenderOnScreenThread;
	class Renderer;
	class Settings;

	enum class ScrollMode { None, Map, Video };

	struct RepeatHandler
	{
		QElapsedTimer firstRepeatTimer;
		QElapsedTimer repeatTimer;
		bool hasBeenReleased = true;
	};

	// Read user input and act accordingly.
	class InputHandler
	{

	public:

		void initialize(VideoWindow* videoWindow, Renderer* renderer, VideoDecoder* videoDecoder, VideoDecoderThread* videoDecoderThread, VideoStabilizer* videoStabilizer, RouteManager* routeManager, RenderOnScreenThread* renderOnScreenThread, Settings* settings);
		void handleInput(double frameTime);

		ScrollMode getScrollMode() const;

	private:

		bool keyIsDownWithRepeat(int key, RepeatHandler& repeatHandler);

		VideoWindow* videoWindow = nullptr;
		Renderer* renderer = nullptr;
		VideoDecoder* videoDecoder = nullptr;
		VideoDecoderThread* videoDecoderThread = nullptr;
		VideoStabilizer* videoStabilizer = nullptr;
		RouteManager* routeManager = nullptr;
		RenderOnScreenThread* renderOnScreenThread = nullptr;
		Settings* settings = nullptr;

		ScrollMode scrollMode = ScrollMode::None;

		const int firstRepeatDelay = 800;
		const int repeatDelay = 50;

		RepeatHandler seekBackwardRepeatHandler;
		RepeatHandler seekForwardRepeatHandler;
		RepeatHandler advanceOneFrameRepeatHandler;
	};
}
