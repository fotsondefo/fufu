#pragma once

namespace Delos
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

		void run();
		void close();

		void pushLayer(Layer* layer);
	};
}