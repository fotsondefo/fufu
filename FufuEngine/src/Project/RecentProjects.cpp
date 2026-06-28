#include "depch.h"
#include "Fufu/Project/RecentProjects.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <ctime>

using json = nlohmann::json;

namespace Fufu 
{

	static std::string currentISOTime()
	{
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		
		char buf[32];
		std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&time));

		return buf;
	}

	void RecentProjects::load(const std::filesystem::path& configDir)
	{
		m_ConfigDir = configDir;
		std::filesystem::path filePath = configDir / "recent.json";

		std::ifstream file(filePath);
		if (!file.is_open()) return;

		json j;
		try { 
			j = json::parse(file); 
		}
		catch (...) { 
			return; 
		}

		m_Entries.clear();
		for (const auto& e : j)
		{
			RecentProjectEntry entry;
			entry.name = e.value("name", "");
			entry.path = e.value("path", "");
			entry.lastOpened = e.value("lastOpened", "");

			// Keep only the entries for which the file still exists
			if (std::filesystem::exists(entry.path))
				m_Entries.push_back(std::move(entry));
		}
	}

	void RecentProjects::save(const std::filesystem::path& configDir) const
	{
		std::filesystem::create_directories(configDir);
		json j = json::array();

		for (const auto& e : m_Entries)
		{
			json entry;
			entry["name"] = e.name;
			entry["path"] = e.path.string();
			entry["lastOpened"] = e.lastOpened;
			j.push_back(entry);
		}

		std::ofstream file(configDir / "recent.json");
		if (file.is_open())
			file << j.dump(4);
	}

	void RecentProjects::push(const RecentProjectEntry& entry)
	{
		// Remove if it's already there
		remove(entry.path);

		// Put it on the top of the list
		RecentProjectEntry e = entry;
		e.lastOpened = currentISOTime();
		m_Entries.insert(m_Entries.begin(), e);

		// Trunk
		if (m_Entries.size() > k_MaxEntries)
			m_Entries.resize(k_MaxEntries);

		save(m_ConfigDir);
	}

	void RecentProjects::remove(const std::filesystem::path& path)
	{
		m_Entries.erase(
			std::remove_if(m_Entries.begin(), m_Entries.end(),
				[&](const RecentProjectEntry& e) {
			return e.path == path;
			}),
			m_Entries.end()
		);
	}

	void RecentProjects::clear()
	{
		m_Entries.clear();
		save(m_ConfigDir);
	}

}