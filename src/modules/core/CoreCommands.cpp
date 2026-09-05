#include "CoreCommands.h"
#include "pch.h"
#include "core/AppCore.h"
#include "core/CommandDispatcher.h"
#include "modules/ModuleRegistry.h"
#include "sdk/Chat.h"
#include "sdk/Logger.h"
#include <format>

REGISTER_MODULE(CoreCommands);

CoreCommands::CoreCommands()
    : Module("core_commands", Category::MISC, true, "Provides built-in client dot commands") {}

void CoreCommands::onLoad()
{
    std::string_view pfx = CommandDispatcher::prefix();

    // Help command
    CommandCallback helpCb = [](const CommandArgs&)
    {
        std::vector<RegisteredCommand> cmds = CommandDispatcher::get().getCommands();
        SDK::Chat::send(std::format("§a§l[BedrockUtils]§r §7Available Client Commands (§f{}§7):", cmds.size()));

        std::string_view pfx = CommandDispatcher::prefix();
        for (const RegisteredCommand& c : cmds)
        {
            std::string desc = c.description.empty() ? "" : std::format(" §8- §7{}", c.description);
            SDK::Chat::send(std::format(" §e{}{}{}", pfx, c.name, desc));
        }
    };
    registerCommand("help", "Show all registered BedrockUtils commands", helpCb);
    registerCommand("commands", std::format("Alias for {}help", pfx), helpCb);
    registerCommand("?", std::format("Alias for {}help", pfx), helpCb);

    // Ping command
    CommandCallback pingCb = [](const CommandArgs&)
    {
        SDK::Chat::notify("Pong!");
    };
    registerCommand("ping", "Check client connection to mod", pingCb);
    registerCommand("p", std::format("Alias for {}ping", pfx), pingCb);

    // Modules command
    CommandCallback modulesCb = [](const CommandArgs&)
    {
        const std::vector<std::unique_ptr<Module>>& allMods = ModuleRegistry::get().all();
        SDK::Chat::send(std::format("§a§l[BedrockUtils]§r §7Loaded Modules (§f{}§7):", allMods.size()));

        for (const std::unique_ptr<Module>& m : allMods)
        {
            if (!m)
            {
                continue;
            }

            std::string status = m->isEnabled() ? "§a[ON]§r" : "§c[OFF]§r";
            std::string cat = std::string(categoryToString(m->category()));
            std::string desc = m->description().empty() ? "" : std::format(" §8- §7{}", m->description());

            SDK::Chat::send(std::format(" {} §f{} §8[§e{}§8]{}", status, m->name(), cat, desc));
        }
    };
    registerCommand("modules", "List all registered modules", modulesCb);
    registerCommand("m", std::format("Alias for {}modules", pfx), modulesCb);

    // Toggle command
    registerCommand("toggle", "Toggle a module on or off", [](const CommandArgs& args)
    {
        std::string_view pfx = CommandDispatcher::prefix();
        if (args.empty())
        {
            SDK::Chat::warn(std::format("Usage: {}toggle <module_name> (e.g. §f{}toggle packet_logger§e)", pfx, pfx));
            return;
        }

        std::string_view target = args.get(0);
        if (Module* m = ModuleRegistry::get().find(target))
        {
            m->toggle();
            std::string status = m->isEnabled() ? "§aEnabled" : "§cDisabled";
            SDK::Chat::notify(std::format("Module '§f{}§7' is now {}", m->name(), status));
        }
        else
        {
            SDK::Chat::error(std::format("Module '§f{}§c' not found. Type §f{}modules§c for list.", target, pfx));
        }
    });

    // Eject command
    registerCommand("eject", "Safely uninject and unload BedrockUtils", [](const CommandArgs&)
    {
        SDK::Chat::warn("Ejecting...");
        AppCore::requestEject();
    });
}

void CoreCommands::onEnable()
{
    SDK::Log::log("[CoreCommands] Enabled");
}

void CoreCommands::onDisable()
{
    SDK::Log::log("[CoreCommands] Disabled");
}

void CoreCommands::onUnload()
{
    SDK::Log::log("[CoreCommands] Unloaded");
}
