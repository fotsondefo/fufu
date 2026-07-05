#pragma once

#include <entt/entt.hpp>

namespace Fufu 
{

	// Forward declaration � �vite l'inclusion circulaire avec Scene.h
	class Scene;

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_Handle(handle), m_Scene(scene) {}

		template<typename T, typename... Args>
		T& addComponent(Args&&... args);

		template<typename T>
		T& getComponent();

		template<typename T>
		const T& getComponent() const;

		template<typename T>
		bool hasComponent() const;

		template<typename T>
		void removeComponent();

		bool isValid() const;

		entt::entity getHandle() const { return m_Handle; }
		Scene*       getScene()  const { return m_Scene; }

		bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return !(*this == other); }
		explicit operator bool() const { return isValid(); }

	private:
		entt::entity m_Handle = entt::null;
		Scene* m_Scene = nullptr;

		friend class Scene;
	};

}