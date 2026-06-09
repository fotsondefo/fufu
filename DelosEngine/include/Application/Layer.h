#pragma once

namespace Delos
{
	class Layer
	{
	public: 
		explicit Layer(const std::string& name = "Layer") : m_Name(name) {}
		virtual ~~Layer() = default;

		virtual void onAttach() {}
		virtual void onDetach() {}
		virtual void onUpdate(float deltaTime) {}
		virtual void onEvent(Event& e) {}

		const std::string& getName() const { return m_Name; }

	private:
		std::string m_Name;
	};
}