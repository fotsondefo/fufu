#pragma once

#include "Application/Layer.h"
#include "Project/Scene.h"
#include "RuntimeConfig.h"

namespace FufuRuntime 
{

	class RuntimeLayer : public Fufu::Layer
	{
	public:
		explicit RuntimeLayer(const RuntimeConfig& config)
			: Fufu::Layer("RuntimeLayer"), m_Config(config) {}

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