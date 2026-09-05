#pragma once
#include "core/CommandDispatcher.h"
#include "pipeline/PacketContext.h"
#include "pipeline/Pipeline.h"
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

enum class Category
{
    COMBAT,
    MOVEMENT,
    PLAYER,
    VISUAL,
    WORLD,
    EXPLOIT,
    NETWORK,
    MISC
};

inline std::string_view categoryToString(Category cat) noexcept
{
    switch (cat)
    {
    case Category::COMBAT:   return "Combat";
    case Category::MOVEMENT: return "Movement";
    case Category::PLAYER:   return "Player";
    case Category::VISUAL:   return "Visual";
    case Category::WORLD:    return "World";
    case Category::EXPLOIT:  return "Exploit";
    case Category::NETWORK:  return "Network";
    case Category::MISC:     return "Misc";
    default:                 return "Unknown";
    }
}

class Module
{
public:
    explicit Module(
        std::string_view name,
        Category category,
        bool enabled = true,
        std::string_view description = "")
        : m_name(name)
        , m_category(category)
        , m_enabled(enabled)
        , m_description(description) {}

    virtual ~Module()
    {
        for (Pipeline::Handle h : m_pipelineHandles)
        {
            Pipeline::get().off(h);
        }
        m_pipelineHandles.clear();

        CommandDispatcher::get().unregisterModuleCommands(m_name);
    }

    Module(const Module&)            = delete;
    Module& operator=(const Module&) = delete;

    [[nodiscard]] const std::string& name() const noexcept
    {
        return m_name;
    }

    [[nodiscard]] Category category() const noexcept
    {
        return m_category;
    }

    [[nodiscard]] const std::string& description() const noexcept
    {
        return m_description;
    }

    [[nodiscard]] bool isEnabled() const noexcept
    {
        return m_enabled.load(std::memory_order_relaxed);
    }

    [[nodiscard]] const std::atomic<bool>* enabledGate() const noexcept
    {
        return &m_enabled;
    }

    void setEnabled(bool v)
    {
        bool expected = !v;
        if (m_enabled.compare_exchange_strong(expected, v))
        {
            if (v)
            {
                onEnable();
            }
            else
            {
                onDisable();
            }
        }
    }

    void toggle()
    {
        bool cur = m_enabled.load(std::memory_order_relaxed);
        setEnabled(!cur);
    }

    // Lifecycle hooks
    virtual void onLoad() {}
    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onUnload() {}
    virtual void onTick() {}

    // Type-safe packet listener automatically gated by this module's enabled state
    template<typename T, typename Fn>
    Pipeline::Handle listen(Fn&& fn)
    {
        Pipeline::Handle h = Pipeline::get().on<T>(std::forward<Fn>(fn), &m_enabled);
        m_pipelineHandles.push_back(h);
        return h;
    }

    // PacketID listener automatically gated by this module's enabled state
    Pipeline::Handle listen(SDK::PacketID id, PacketHandler fn)
    {
        Pipeline::Handle h = Pipeline::get().on(id, std::move(fn), &m_enabled);
        m_pipelineHandles.push_back(h);
        return h;
    }

    // Client double semicolon (;;) command registration helper
    void registerCommand(
        std::string_view cmdName,
        std::string_view desc,
        CommandCallback callback,
        CommandFlags flags = CommandFlags::None)
    {
        CommandDispatcher::get().registerCommand(
            cmdName,
            desc,
            std::move(callback),
            &m_enabled,
            m_name,
            flags
        );
    }

private:
    std::string                   m_name;
    Category                      m_category;
    std::atomic<bool>             m_enabled{true};
    std::string                   m_description;
    std::vector<Pipeline::Handle> m_pipelineHandles;
};
