#pragma once

#include "IDemo.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace FufuLab
{
    // Registre statique de toutes les démos disponibles.
    // Chaque démo s'enregistre via FUFULAB_REGISTER_DEMO à l'init statique.
    class DemoRegistry
    {
    public:
        using Factory = std::function<std::unique_ptr<IDemo>()>;

        struct Entry
        {
            std::string           name;
            std::string           category;  // ex : "Lighting", "AA", "GI"
            Factory               factory;
        };

        static DemoRegistry& get();

        void               add(const std::string& category, const std::string& name, Factory factory);
        std::unique_ptr<IDemo> create(const std::string& name) const;
        const std::vector<Entry>& entries() const { return m_Entries; }

    private:
        std::vector<Entry> m_Entries;
    };

    // Helper pour l'enregistrement automatique via un objet statique
    struct DemoRegistrar
    {
        DemoRegistrar(const char* category, const char* name, DemoRegistry::Factory factory)
        {
            DemoRegistry::get().add(category, name, std::move(factory));
        }
    };
}

// Macro à placer dans le .cpp de chaque démo.
// __COUNTER__ génère un entier unique par unité de traduction : évite le
// problème de token-paste avec un nom qualifié (ex: FufuLab::MyDemo).
#define FUFULAB_DETAIL_CONCAT2(a, b) a##b
#define FUFULAB_DETAIL_CONCAT(a, b)  FUFULAB_DETAIL_CONCAT2(a, b)

#define FUFULAB_REGISTER_DEMO(category, name, DemoClass)                              \
    static ::FufuLab::DemoRegistrar FUFULAB_DETAIL_CONCAT(s_LabDemo_, __COUNTER__)(  \
        category, name,                                                               \
        []() -> std::unique_ptr<::FufuLab::IDemo> {                                  \
            return std::make_unique<DemoClass>();                                     \
        }                                                                             \
    )
