#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace Fufu 
{

	struct RecentProjectEntry
	{
		std::string name;
		std::filesystem::path path; // path to the .fufuproj file
		std::string lastOpened;  
	};

	class RecentProjects
	{
	public:
		static constexpr size_t k_MaxEntries = 10;

		void load(const std::filesystem::path& configDir);
		void save(const std::filesystem::path& configDir) const;

		void push(const RecentProjectEntry& entry);
		void remove(const std::filesystem::path& path);
		void clear();

		const std::vector<RecentProjectEntry>& getEntries() const { return m_Entries; }
		bool isEmpty() const { return m_Entries.empty(); }

	private:
		std::vector<RecentProjectEntry> m_Entries;
		std::filesystem::path m_ConfigDir;
	};

}