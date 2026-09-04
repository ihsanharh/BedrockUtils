#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SDK
{
    class CommandRequestPacket;
    class TextPacket;
}

template<typename T>
struct TypedPacketContext;

struct CommandArgs
{
    std::string rawCommand;
    std::string commandName;
    std::vector<std::string> args;

    [[nodiscard]] bool empty() const noexcept
    {
        return args.empty();
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return args.size();
    }

    [[nodiscard]] std::string_view get(size_t index) const noexcept
    {
        return index < args.size() ? std::string_view(args[index]) : std::string_view{};
    }
};

enum class CommandFlags : uint8_t
{
    None            = 0,
    RequiresEnabled = 1 << 0
};

inline CommandFlags operator|(CommandFlags a, CommandFlags b) noexcept
{
    return static_cast<CommandFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool operator&(CommandFlags a, CommandFlags b) noexcept
{
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

using CommandCallback = std::function<void(const CommandArgs& args)>;

struct RegisteredCommand
{
    std::string name;
    std::string description;
    CommandCallback callback;
    const std::atomic<bool>* enabledGate = nullptr;
    std::string owningModuleName;
    CommandFlags flags = CommandFlags::None;
};

class CommandDispatcher
{
public:
    static CommandDispatcher& get()
    {
        static CommandDispatcher inst;
        return inst;
    }

    void init();
    void shutdown();

    void registerCommand(
        std::string_view name,
        std::string_view description,
        CommandCallback callback,
        const std::atomic<bool>* enabledGate = nullptr,
        std::string_view owningModuleName = "",
        CommandFlags flags = CommandFlags::None
    );

    void unregisterCommand(std::string_view name);
    void unregisterModuleCommands(std::string_view moduleName);

    [[nodiscard]] std::vector<RegisteredCommand> getCommands() const;

    bool dispatch(std::string_view rawInput);
    [[nodiscard]] bool isCommandRegistered(std::string_view name) const;

private:
    CommandDispatcher() = default;
    ~CommandDispatcher() = default;

    void handleOutboundCommandRequest(TypedPacketContext<SDK::CommandRequestPacket>& ctx);
    void handleOutboundChat(TypedPacketContext<SDK::TextPacket>& ctx);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, RegisteredCommand> m_commands;
    bool m_initialized = false;
};
