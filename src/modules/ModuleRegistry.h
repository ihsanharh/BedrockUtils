#pragma once
#include "Module.h"
#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

class ModuleRegistry
{
public:
    using ModuleFactory = std::function<std::unique_ptr<Module>()>;

    static ModuleRegistry& get()
    {
        static ModuleRegistry inst;
        return inst;
    }

    // Static registration for the REGISTER_MODULE macro
    static void registerModuleFactory(std::string_view name, ModuleFactory factory);

    // Initializes all registered modules: creates instances, calls onLoad(), and onEnable() if default-enabled
    void init();

    // Ticks all active modules
    void tick();

    // Shuts down and destroys all modules (calls onUnload() on each)
    void clear();

    [[nodiscard]] Module* find(std::string_view name) const;
    bool toggle(std::string_view name);

    [[nodiscard]] const std::vector<std::unique_ptr<Module>>& all() const noexcept
    {
        return m_modules;
    }

private:
    ModuleRegistry() = default;
    ~ModuleRegistry() = default;

    struct FactoryEntry
    {
        std::string name;
        ModuleFactory factory;
    };

    static std::vector<FactoryEntry>& getRegisteredFactories();

    std::vector<std::unique_ptr<Module>> m_modules;
    bool m_initialized = false;
};

#define REGISTER_MODULE(Type) \
    namespace { \
        static const bool s_reg_##Type = []() { \
            ModuleRegistry::registerModuleFactory(#Type, []() -> std::unique_ptr<Module> { \
                return std::make_unique<Type>(); \
            }); \
            return true; \
        }(); \
    }
