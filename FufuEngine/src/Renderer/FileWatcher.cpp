#include "depch.h"
#include "Renderer/FileWatcher.h"

namespace Fufu
{

void FileWatcher::watch(const std::filesystem::path& absPath, std::function<void()> callback)
{
    std::string key = absPath.string();

    auto it = m_Entries.find(key);
    if (it == m_Entries.end())
    {
        Entry e;
        e.path = absPath;
        e.lastWriteTime = std::filesystem::exists(absPath)
            ? std::filesystem::last_write_time(absPath)
            : std::filesystem::file_time_type{};
        e.callbacks.push_back(std::move(callback));
        m_Entries.emplace(std::move(key), std::move(e));
    }
    else
    {
        it->second.callbacks.push_back(std::move(callback));
    }
}

int FileWatcher::poll()
{
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - m_LastPoll).count();
    if (elapsed < m_PollIntervalMs)
        return 0;
    m_LastPoll = now;

    int changed = 0;
    for (auto& [key, entry] : m_Entries)
    {
        if (!std::filesystem::exists(entry.path))
            continue;

        auto mtime = std::filesystem::last_write_time(entry.path);
        if (mtime != entry.lastWriteTime)
        {
            entry.lastWriteTime = mtime;
            ++changed;
            FUFU_INFO("FileWatcher: '{}' changed, reloading...", entry.path.filename().string());
            for (auto& cb : entry.callbacks)
                cb();
        }
    }
    return changed;
}

} // namespace Fufu
