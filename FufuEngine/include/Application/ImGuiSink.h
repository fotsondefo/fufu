#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <vector>
#include <mutex>
#include <memory>

namespace Fufu
{

struct LogEntry
{
    spdlog::level::level_enum level;
    std::string               text; // formatted: "[HH:MM:SS] message"
};

// Thread-safe spdlog sink that accumulates log entries in a ring buffer.
// The ImGui LogPanel reads from this sink on the main thread to render them.
// Max capacity: 1024 entries — oldest are dropped silently when full.
template<typename Mutex>
class ImGuiSink final : public spdlog::sinks::base_sink<Mutex>
{
public:
    static constexpr std::size_t k_Capacity = 1024;

    // Global singleton accessed by LogPanel.
    static std::shared_ptr<ImGuiSink<Mutex>>& instance()
    {
        static std::shared_ptr<ImGuiSink<Mutex>> s_Instance;
        return s_Instance;
    }

    const std::vector<LogEntry>& entries() const { return m_Entries; }
    bool hasNew() const { return m_HasNew; }
    void clearNew()     { m_HasNew = false; }
    void clear()        { m_Entries.clear(); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t buf;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, buf);

        if (m_Entries.size() >= k_Capacity)
            m_Entries.erase(m_Entries.begin());

        m_Entries.push_back({ msg.level, fmt::to_string(buf) });
        m_HasNew = true;
    }
    void flush_() override {}

private:
    std::vector<LogEntry> m_Entries;
    bool                  m_HasNew = false;
};

using ImGuiSink_mt = ImGuiSink<std::mutex>;
using ImGuiSink_st = ImGuiSink<spdlog::details::null_mutex>;

} // namespace Fufu
