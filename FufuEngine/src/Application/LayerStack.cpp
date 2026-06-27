#include "depch.h"
#include "Application/LayerStack.h"

namespace Fufu
{
	LayerStack::~LayerStack()
	{
		for (ILayer* layer : m_Layers)
		{
			layer->onDetach();
			delete layer;
		}
	}

	void LayerStack::pushLayer(ILayer* layer)
	{
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		m_LayerInsertIndex++;

		layer->onAttach();
	}

	void LayerStack::popLayer(ILayer* layer)
	{
		auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, layer);

		if (it != m_Layers.begin() + m_LayerInsertIndex)
		{
			layer->onDetach();
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}
}