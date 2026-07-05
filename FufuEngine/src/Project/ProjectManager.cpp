#include "depch.h"
#include "Project/ProjectManager.h"

namespace Fufu 
{

	void ProjectManager::init(const std::filesystem::path& appConfigDir)
	{
		m_AppConfigDir = appConfigDir;
		std::filesystem::create_directories(appConfigDir);
		m_RecentProjects.load(appConfigDir);
		FUFU_INFO("ProjectManager initialized � {} recent projects", m_RecentProjects.getEntries().size());
	}

	void ProjectManager::shutdown()
	{
		closeProject();
	}

	std::shared_ptr<Project> ProjectManager::createProject(const std::filesystem::path& directory, const std::string& name)
	{
		closeProject();

		auto project = Project::create(directory, name);
		if (!project) return nullptr;

		project->init();
		m_CurrentProject = project;

		RecentProjectEntry entry;
		entry.name = name;
		entry.path = project->getInfo().projectFilePath;
		m_RecentProjects.push(entry);

		FUFU_INFO("Project created: '{}'", name);
		return m_CurrentProject;
	}

	std::shared_ptr<Project> ProjectManager::openProject(const std::filesystem::path& projFilePath)
	{
		closeProject();

		auto project = Project::load(projFilePath);
		if (!project)
		{
			FUFU_ERROR("Failed to open project: '{}'", projFilePath.string());
			m_RecentProjects.remove(projFilePath);
			m_RecentProjects.save(m_AppConfigDir);

			return nullptr;
		}

		project->init();
		m_CurrentProject = project;

		RecentProjectEntry entry;
		entry.name = project->getName();
		entry.path = projFilePath;
		m_RecentProjects.push(entry);

		FUFU_INFO("Project opened: '{}'", project->getName());

		return m_CurrentProject;
	}

	void ProjectManager::saveCurrentProject()
	{
		if (!m_CurrentProject) 
			return;

		m_CurrentProject->save();
	}

	void ProjectManager::closeProject()
	{
		if (!m_CurrentProject) return;

		// Sans ça, une scène créée pendant la session (ex: "+ New Scene") mais
		// jamais sauvegardée manuellement disparaissait purement et simplement
		// à la fermeture — plus aucune trace ni sur disque ni dans le .fufuproj.
		m_CurrentProject->saveAllLoadedScenes();

		m_CurrentProject->getSceneManager().unloadAll();
		m_CurrentProject->getAssetManager().unloadAll();
		m_CurrentProject.reset();

		FUFU_INFO("Project closed");
	}

}