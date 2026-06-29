#pragma once

#include "Application/ILayer.h"
#include "Project/Scene/Scene.h"
#include "RuntimeConfig.h"

namespace FufuRuntime 
{

	class RuntimeLayer : public Fufu::ILayer
	{
	public:
		explicit RuntimeLayer(const RuntimeConfig& config)
			: Fufu::ILayer("RuntimeLayer"), m_Config(config) {}

		void onAttach() override;
		void onDetach() override;
		void onUpdate(float deltaTime) override;
		void onEvent(Fufu::Event& e) override;

	private:
		bool onKeyPressed(Fufu::KeyPressedEvent& e);

	private:
		RuntimeConfig              m_Config;
		std::shared_ptr<Fufu::Scene> m_Scene;
	};

}