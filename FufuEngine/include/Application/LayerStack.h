#pragma once

#include "ILayer.h"
#include <vector>

namespace Fufu
{
	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack();

		void pushLayer(ILayer* layer);
		void popLayer(ILayer* layer);

		std::vector<ILayer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<ILayer*>::iterator end() { return m_Layers.end(); }
		std::vector<ILayer*>::const_iterator begin() const { return m_Layers.begin(); }
		std::vector<ILayer*>::const_iterator end() const { return m_Layers.end(); }

	private:
		std::vector<ILayer*> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};
}