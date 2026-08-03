#pragma once

#include <entt/entt.hpp>
#include "Project/Entity.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/EnvironmentSettings.h"

namespace Fufu
{

	class Scene
	{
	public:
		Scene() = default;
		explicit Scene(const std::string& name) : m_Name(name) {}
		~Scene() = default;

		// Entity creation / destruction
		Entity createEntity(const std::string& tag = "Entity");
		void   destroyEntity(Entity entity);

		// Hierarchy
		void setParent(Entity child, Entity parent);
		void removeParent(Entity child);

		// Interate on the component of a type 
		template<typename... Components, typename Func>
		void each(Func&& func)
		{
			auto view = m_Registry.view<Components...>();
			view.each(std::forward<Func>(func));
		}

		Entity getPrimaryCamera();

		const std::string& getName() const { return m_Name; }
		void               setName(const std::string& name) { m_Name = name; }

		entt::registry& getRegistry() { return m_Registry; }

		// Render settings specific to THIS scene (technique, AA, exposure...):
		// saved/loaded with the .fufuscene file (see SceneSerializer), and pushed
		// into the Renderer when the scene becomes active (see
		// EditorState::syncToActiveScene on the FufuStudio side).
		RenderSettings&       getRenderSettings()       { return m_RenderSettings; }
		const RenderSettings& getRenderSettings() const { return m_RenderSettings; }

		// Environment (skybox) of this scene: read directly by Renderer every
		// frame (no mirroring like RenderSettings, since Renderer::renderScene
		// already receives the Scene as a parameter).
		EnvironmentSettings&       getEnvironment()       { return m_Environment; }
		const EnvironmentSettings& getEnvironment() const { return m_Environment; }

		// Counter bumped on every content mutation that requires a GPU re-upload
		// (ECS structure via createEntity/destroyEntity/setParent/removeParent
		// and addComponent/removeComponent — see EntityImpl.h —, or explicitly
		// by commands that edit a component/mesh in place). Compared by
		// Renderer::sceneNeedsUpdate against a cached value to avoid rebuilding
		// all GPU geometry (BVH included) every frame when nothing has changed.
		void     markDirty() { ++m_Version; }
		uint32_t getVersion() const { return m_Version; }

	private:
		entt::registry m_Registry;
		std::string    m_Name = "Untitled";
		RenderSettings m_RenderSettings;
		EnvironmentSettings m_Environment;
		uint32_t m_Version = 0;

		friend class Entity;
		friend class SceneSerializer;
	};

}

// Deferred inclusion: at this point Scene is fully defined,
// so EntityImpl.h can implement Entity's template methods.
#include "Project/EntityImpl.h"