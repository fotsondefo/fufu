#pragma once

#include "Layer.h"
#include <vector>

namespace Fufu
{
	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack();

		void pushLayer(Layer* layer);
		void pushOverlay(Layer* overlay);
		void popLayer(Layer* layer);
		void popOverlay(Layer* overlay);

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }
		std::vector<Layer*>::const_iterator begin() const { return m_Layers.begin(); }
		std::vector<Layer*>::const_iterator end() const { return m_Layers.end(); }

	private:
		std::vector<Layer*> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};
}