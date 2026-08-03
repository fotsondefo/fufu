#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>

namespace Fufu
{

// Polls shader and asset files for on-disk changes and fires callbacks.
// Designed for editor-only use — polling is safe at 500ms intervals
// and doesn't need a background thread.
class FileWatcher
{
public:
    FileWatcher() = default;

    // Register `absPath` for change detection. If the file changes on disk,
    // `callback` is invoked from the next poll() call (main thread).
    // Multiple registrations for the same path are merged: the callbacks
    // accumulate (all are called on change). This lets several passes watch
    // a shared vertex shader (e.g. GBuffer.vert) independently.
    void watch(const std::filesystem::path& absPath, std::function<void()> callback);

    // How often to actually stat the files (default 500 ms).
    void setPollIntervalMs(int ms) { m_PollIntervalMs = ms; }

    // Call once per frame (or less). Returns the number of changed files
    // detected this tick.
    int poll();

    void clear() { m_Entries.clear(); }

private:
    struct Entry
    {
        std::filesystem::path              path;
        std::filesystem::file_time_type    lastWriteTime;
        std::vector<std::function<void()>> callbacks;
    };

    std::unordered_map<std::string, Entry> m_Entries; // keyed by canonical path string
    int m_PollIntervalMs = 500;
    std::chrono::steady_clock::time_point m_LastPoll{};
};

} // namespace Fufu
