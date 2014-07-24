// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#pragma once

#include <memory>

#include <QWindow>
#include <QOpenGLContext>

namespace OrientView
{
	class VideoWindow : public QWindow
	{
		Q_OBJECT

	public:

		explicit VideoWindow(QWindow* parent = 0);
		~VideoWindow();

		bool initialize();
		void shutdown();

		QOpenGLContext* getContext() const;

	signals:

		void closing();

	protected:

		bool event(QEvent* event);

	private:

		std::unique_ptr<QOpenGLContext> context = nullptr;
	};
}