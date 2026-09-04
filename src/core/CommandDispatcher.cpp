#include "CommandDispatcher.h"
#include "pch.h"
#include "pipeline/PacketContext.h"
#include "pipeline/Pipeline.h"
#include "sdk/SafeString.h"
#include "sdk/packets/CommandRequestPacket.h"
#include "sdk/packets/TextPacket.h"
#include "sdk/Chat.h"
#include "sdk/Logger.h"
#include <algorithm>
#include <format>
#include <sstream>

void CommandDispatcher::init()
{
    if (m_initialized)
    {
        return;
    }
    m_initialized = true;

    // Outbound slash command interception (Minecraft /cmd requests that start with //)
    Pipeline::get().on<SDK::CommandRequestPacket>([this](TypedPacketContext<SDK::CommandRequestPacket>& ctx)
    {
        if (ctx.dir == PacketDirection::Outbound && ctx.packet)
        {
            handleOutboundCommandRequest(ctx);
        }
    });

    // Outbound chat interception (standard chat messages that start with //)
    Pipeline::get().on<SDK::TextPacket>([this](TypedPacketContext<SDK::TextPacket>& ctx)
    {
        if (ctx.dir == PacketDirection::Outbound && ctx.packet)
        {
            handleOutboundChat(ctx);
        }
    });
}

void CommandDispatcher::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands.clear();
    m_initialized = false;
}

void CommandDispatcher::registerCommand(
    std::string_view name,
    std::string_view description,
    CommandCallback callback,
    const std::atomic<bool>* enabledGate,
    std::string_view owningModuleName,
    CommandFlags flags)
{
    std::string lowerName = SDK::toLower(name);
    while (lowerName.starts_with("/"))
    {
        lowerName.erase(0, 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands[lowerName] = RegisteredCommand{
        .name = lowerName,
        .description = std::string(description),
        .callback = std::move(callback),
        .enabledGate = enabledGate,
        .owningModuleName = std::string(owningModuleName),
        .flags = flags
    };

    SDK::Log::log("[CommandDispatcher] Registered command: //{}", lowerName);
}

void CommandDispatcher::unregisterCommand(std::string_view name)
{
    std::string lowerName = SDK::toLower(name);
    while (lowerName.starts_with("/"))
    {
        lowerName.erase(0, 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands.erase(lowerName);
}

void CommandDispatcher::unregisterModuleCommands(std::string_view moduleName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (std::unordered_map<std::string, RegisteredCommand>::iterator it = m_commands.begin(); it != m_commands.end();)
    {
        if (it->second.owningModuleName == moduleName)
        {
            it = m_commands.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<RegisteredCommand> CommandDispatcher::getCommands() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RegisteredCommand> list;
    list.reserve(m_commands.size());
    for (const std::pair<const std::string, RegisteredCommand>& kv : m_commands)
    {
        list.push_back(kv.second);
    }
    return list;
}

bool CommandDispatcher::isCommandRegistered(std::string_view name) const
{
    std::string lowerName = SDK::toLower(name);
    while (lowerName.starts_with("/"))
    {
        lowerName.erase(0, 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commands.find(lowerName) != m_commands.end();
}

bool CommandDispatcher::dispatch(std::string_view rawInput)
{
    std::string clean = SDK::stripColorCodes(rawInput);
    std::string_view input = SDK::trim(clean);
    if (input.empty())
    {
        return false;
    }

    const bool isExplicitDoubleSlash = input.starts_with("//");
    const bool isSlash = input.starts_with("/");

    // Commands must start with / or // to qualify as a command input
    if (!isSlash && !isExplicitDoubleSlash)
    {
        return false;
    }

    // Strip leading slashes to extract command token (e.g. //help, /help -> help)
    std::string_view stripped = input;
    while (stripped.starts_with("/"))
    {
        stripped.remove_prefix(1);
    }

    std::string inputStr(stripped);
    std::istringstream stream(inputStr);
    std::string cmdName;
    if (!(stream >> cmdName))
    {
        if (isExplicitDoubleSlash)
        {
            SDK::Chat::warn("Type §f//help§e or §f//modules§e for available commands.");
            return true;
        }
        return false;
    }

    std::string lowerCmd = SDK::toLower(cmdName);

    RegisteredCommand cmd;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::unordered_map<std::string, RegisteredCommand>::const_iterator it = m_commands.find(lowerCmd);
        if (it != m_commands.end())
        {
            cmd = it->second;
            found = true;
        }
    }

    // If not found in registered client commands:
    // - If typed with explicit double slash (//cmd), drop it and report unknown client command (prevents leaking to server).
    // - If typed with single slash (/cmd), pass through to the server command system.
    if (!found)
    {
        if (isExplicitDoubleSlash)
        {
            SDK::Log::log("[CommandDispatcher] Unknown double-slash command: //{} (raw: '{}')", lowerCmd, rawInput);
            SDK::Chat::error(std::format("Unknown command '§f//{}§c'. Type §f//help§c for a list of commands.", lowerCmd));
            return true;
        }
        return false;
    }

    SDK::Log::log("[CommandDispatcher] Executing command: //{} (raw: '{}')", lowerCmd, rawInput);

    // Check module enabled state if required by flags
    if (cmd.flags & CommandFlags::RequiresEnabled)
    {
        if (cmd.enabledGate && !cmd.enabledGate->load(std::memory_order_relaxed))
        {
            std::string modDisplay = cmd.owningModuleName.empty() ? cmd.name : cmd.owningModuleName;
            SDK::Chat::error(std::format("Module '§f{}§c' is disabled. Type §f//toggle {}§c to enable.",
                modDisplay, modDisplay));
            return true;
        }
    }

    // Parse arguments
    CommandArgs args;
    args.rawCommand = std::string(rawInput);
    args.commandName = lowerCmd;

    std::string arg;
    while (stream >> arg)
    {
        args.args.push_back(std::move(arg));
    }

    __try
    {
        if (cmd.callback)
        {
            cmd.callback(args);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[CommandDispatcher] Handled exception executing command: //{}", lowerCmd);
        SDK::Chat::error(std::format("An error occurred executing //{}", lowerCmd));
    }

    return true;
}

void CommandDispatcher::handleOutboundCommandRequest(TypedPacketContext<SDK::CommandRequestPacket>& ctx)
{
    if (!ctx.packet)
    {
        return;
    }

    std::string cmdStr = ctx.packet->command.str();
    if (cmdStr.empty())
    {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(ctx.packet);
        for (size_t off = 0x28; off <= 0x48; off += 8)
        {
            if (SDK::safeReadString(base + off, cmdStr) && !cmdStr.empty())
            {
                break;
            }
        }
    }

    SDK::Log::log("[CommandDispatcher] Outbound CommandRequest: '{}'", cmdStr);

    if (dispatch(cmdStr))
    {
        ctx.drop("Executed client command");
    }
}

void CommandDispatcher::handleOutboundChat(TypedPacketContext<SDK::TextPacket>& ctx)
{
    if (!ctx.packet)
    {
        return;
    }

    std::vector<std::string> allTexts;
    SDK::safeReadAllTextMessages(ctx.packet, allTexts);

    for (const std::string& text : allTexts)
    {
        std::string clean = SDK::stripColorCodes(text);
        std::string_view trimmed = SDK::trim(clean);
        if (trimmed.starts_with("//") || trimmed.starts_with("/"))
        {
            SDK::Log::log("[CommandDispatcher] Outbound chat command candidate: '{}'", trimmed);
            if (dispatch(trimmed))
            {
                ctx.drop("Executed client command from chat");
                return;
            }
        }
    }
}
