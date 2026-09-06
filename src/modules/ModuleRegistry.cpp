#include "ModuleRegistry.h"
#include "pch.h"
#include "sdk/Logger.h"
#include <algorithm>
#include <ranges>

std::vector<ModuleRegistry::FactoryEntry>& ModuleRegistry::getRegisteredFactories()
{
    static std::vector<FactoryEntry> s_factories;
    return s_factories;
}

void ModuleRegistry::registerModuleFactory(std::string_view name, ModuleFactory factory)
{
    getRegisteredFactories().push_back(FactoryEntry{
        .name = std::string(name),
        .factory = std::move(factory)
    });
}

void ModuleRegistry::init()
{
    if (m_initialized)
    {
        return;
    }
    m_initialized = true;

    std::vector<FactoryEntry>& factories = getRegisteredFactories();
    m_modules.reserve(factories.size());

    for (const FactoryEntry& entry : factories)
    {
        if (!entry.factory)
        {
            continue;
        }

        std::unique_ptr<Module> mod = entry.factory();
        if (!mod)
        {
            continue;
        }

        SDK::Log::log("[ModuleRegistry] Loading module: {}", mod->name());

        // 1. One-time setup
        mod->onLoad();

        // 2. Activation if enabled by default
        if (mod->isEnabled())
        {
            mod->onEnable();
        }

        m_modules.push_back(std::move(mod));
    }

    SDK::Log::log("[ModuleRegistry] Initialized {} module(s)", m_modules.size());
}

static void safeModuleTick(Module* m) noexcept
{
    __try
    {
        m->onTick();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void ModuleRegistry::tick()
{
    for (const std::unique_ptr<Module>& m : m_modules)
    {
        if (m && m->isEnabled())
        {
            safeModuleTick(m.get());
        }
    }
}

void ModuleRegistry::clear()
{
    for (const std::unique_ptr<Module>& m : m_modules)
    {
        if (!m)
        {
            continue;
        }

        if (m->isEnabled())
        {
            m->onDisable();
        }

        m->onUnload();
    }

    m_modules.clear();
    m_initialized = false;
}

Module* ModuleRegistry::find(std::string_view name) const
{
    std::vector<std::unique_ptr<Module>>::const_iterator it = std::ranges::find_if(
        m_modules,
        [name](const std::unique_ptr<Module>& m)
        {
            return m && m->name() == name;
        }
    );

    return (it != m_modules.end()) ? it->get() : nullptr;
}

bool ModuleRegistry::toggle(std::string_view name)
{
    if (Module* m = find(name))
    {
        m->toggle();
        return true;
    }
    return false;
}
