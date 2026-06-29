#pragma once

#include "Project.h"
#include "RecentProjects.h"

namespace Fufu 
{

	class ProjectManager
	{
	public:
		ProjectManager() = default;
		~ProjectManager() = default;

		void init(const std::filesystem::path& appConfigDir);
		void shutdown();

		std::shared_ptr<Project> createProject(const std::filesystem::path& directory, const std::string& name);

		std::shared_ptr<Project> openProject(const std::filesystem::path& projFilePath);

		void saveCurrentProject();
		void closeProject();

		bool hasProject() const { return m_CurrentProject != nullptr; }

		Project& getCurrentProject()
		{
			FUFU_ASSERT(m_CurrentProject, "No project open");
			return *m_CurrentProject;
		}

		std::shared_ptr<Project> getCurrentProjectPtr() const
		{
			return m_CurrentProject;
		}

		const RecentProjects& getRecentProjects() const { return m_RecentProjects; }

	private:
		std::shared_ptr<Project> m_CurrentProject;
		RecentProjects m_RecentProjects;
		std::filesystem::path m_AppConfigDir;
	};

}