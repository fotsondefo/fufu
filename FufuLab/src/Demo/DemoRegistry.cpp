#include "Demo/DemoRegistry.h"
#include <stdexcept>

namespace FufuLab
{
    DemoRegistry& DemoRegistry::get()
    {
        static DemoRegistry s_Instance;
        return s_Instance;
    }

    void DemoRegistry::add(const std::string& category, const std::string& name, Factory factory)
    {
        m_Entries.push_back({ name, category, std::move(factory) });
    }

    std::unique_ptr<IDemo> DemoRegistry::create(const std::string& name) const
    {
        for (const auto& entry : m_Entries)
        {
            if (entry.name == name)
                return entry.factory();
        }
        throw std::runtime_error("FufuLab: demo not found: " + name);
    }
}
