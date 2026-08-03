#pragma once

#include <string>
#include "Events/Event.h"

namespace Fufu
{
	class ILayer
	{
	public: 
		explicit ILayer(const std::string& name = "Layer") : m_Name(name) {}
		virtual ~ILayer() = default;

		virtual void onAttach() {}
		virtual void onDetach() {}
		virtual void onUpdate(float /* deltaTime */) {}
		virtual void onEvent(Event& /* e */) {}

		const std::string& getName() const { return m_Name; }

	private:
		std::string m_Name;
	};
}