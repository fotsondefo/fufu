#pragma once

#include <functional>
#include <vector>
#include <cstdint>

namespace Fufu
{

// Lightweight typed signal — publish/subscribe with no dependency on Qt or boost.
// Each connect() returns an ID that can be used to unsubscribe later.
//
// Usage:
//   Signal<double, double> onTick;
//   uint32_t id = onTick.connect([](double t, double dt) { ... });
//   onTick.emit(1.0, 0.016);
//   onTick.disconnect(id);
template<typename... Args>
class Signal
{
public:
    using Handler = std::function<void(Args...)>;

    uint32_t connect(Handler h)
    {
        uint32_t id = m_NextId++;
        m_Slots.push_back({ id, std::move(h) });
        return id;
    }

    void disconnect(uint32_t id)
    {
        auto it = std::find_if(m_Slots.begin(), m_Slots.end(),
            [id](const Slot& s) { return s.id == id; });
        if (it != m_Slots.end())
            m_Slots.erase(it);
    }

    void emit(Args... args) const
    {
        // Local copy: a handler may call disconnect() during emit()
        auto copy = m_Slots;
        for (auto& s : copy)
            s.handler(args...);
    }

    void disconnectAll() { m_Slots.clear(); }
    bool empty()   const { return m_Slots.empty(); }
    size_t count() const { return m_Slots.size(); }

private:
    struct Slot { uint32_t id; Handler handler; };
    std::vector<Slot> m_Slots;
    uint32_t m_NextId = 1;
};

} // namespace Fufu
