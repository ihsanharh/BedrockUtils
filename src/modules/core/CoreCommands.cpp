#include "CoreCommands.h"
#include "pch.h"
#include "core/AppCore.h"
#include "core/CommandDispatcher.h"
#include "modules/ModuleRegistry.h"
#include "intercept/PacketInterceptor.h"
#include "sdk/Chat.h"
#include "sdk/CommandManager.h"
#include "sdk/Factory.h"
#include "sdk/Game.h"
#include "sdk/Logger.h"
#include "sdk/SafeString.h"
#include "sdk/packets/AddActorPacket.h"
#include "sdk/packets/TextPacket.h"
#include "services/PacketDumper.h"
#include "services/PlayerTracker.h"
#include <atomic>
#include <format>

REGISTER_MODULE(CoreCommands);

CoreCommands::CoreCommands()
    : Module("core_commands", Category::MISC, true, "Provides built-in client commands") {}

void CoreCommands::onLoad()
{
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

    // Ping command
    CommandCallback pingCb = [](const CommandArgs&)
    {
        SDK::Chat::notify("Pong!");
    };
    registerCommand("ping", "Check client connection to mod", pingCb);

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

    // Toggle command
    CommandCallback toggleCb = [](const CommandArgs& args)
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
    };
    registerCommand("toggle", "Toggle a module on or off", toggleCb);

    // Player detail query command
    CommandCallback playerCb = [](const CommandArgs& args)
    {
        std::string_view pfx = CommandDispatcher::prefix();
        if (args.empty())
        {
            SDK::Chat::warn(std::format("Usage: {}player <name>", pfx));
            return;
        }

        std::string_view targetName = args.get(0);
        std::shared_ptr<TrackedPlayer> p = PlayerTracker::get().findPlayerByName(targetName);
        if (!p)
        {
            SDK::Chat::error(std::format("Player '§f{}§c' not found in PlayerTracker.", targetName));
            return;
        }

        SDK::Chat::send("§6§m----------------------------------------§r");
        SDK::Chat::send(std::format("§6§l[Player Info]§r §e§l{}§r", p->name.empty() ? "(Unknown)" : p->name));
        SDK::Chat::send(std::format("§7Name: §f{}", p->name.empty() ? "-" : p->name));

        if (p->isSpawned)
        {
            float dist = p->distanceToLocalPlayer();
            SDK::Chat::send(std::format("§7Status: §a§lIn-World (Rendered)§r §7| Platform: §b{}§r", p->getPlatformName()));
            SDK::Chat::send(std::format("§7Position: §f({:.1f}, {:.1f}, {:.1f}) §7(§e{:.1f}m away§7)", p->pos[0], p->pos[1], p->pos[2], dist));
            SDK::Chat::send(std::format("§7Runtime ID: §a{} §7| Unique ID: §b{}", p->runtimeEntityId, p->uniqueEntityId));
        }
        else
        {
            SDK::Chat::send(std::format("§7Status: §c§lOut of Sight (Tab Roster)§r §7| Platform: §b{}§r", p->getPlatformName()));
            SDK::Chat::send(std::format("§7Last Coords: §f({:.1f}, {:.1f}, {:.1f})", p->pos[0], p->pos[1], p->pos[2]));
            SDK::Chat::send(std::format("§7Unique ID: §b{}", p->uniqueEntityId));
        }

        if (!p->nametag.empty())
        {
            SDK::Chat::send(std::format("§7Nametag: §f{}", p->nametag));
        }
        if (!p->xuid.empty())
        {
            SDK::Chat::send(std::format("§7XUID: §f{}", p->xuid));
        }
        if (!p->platformOnlineId.empty())
        {
            SDK::Chat::send(std::format("§7Platform Online ID: §f{}", p->platformOnlineId));
        }
        if (!p->deviceId.empty())
        {
            SDK::Chat::send(std::format("§7Device ID: §f{}", p->deviceId));
        }
        SDK::Chat::send(std::format("§7UUID: §f{}", p->uuid));
        SDK::Chat::send("§6§m----------------------------------------§r");
    };
    registerCommand("player", "Show detailed info for a player", playerCb);

    // Players dump command
    CommandCallback playersCb = [&playerCb](const CommandArgs& args)
    {
        if (!args.empty())
        {
            playerCb(args);
            return;
        }

        std::vector<std::shared_ptr<TrackedPlayer>> players = PlayerTracker::get().getAllPlayerPtrs();
        size_t totalCount = players.size();
        size_t worldCount = PlayerTracker::get().getWorldPlayerCount();

        SDK::Log::log("\n====================================================================================================================================================================================================");
        SDK::Log::log("                                                              PLAYER TRACKER DUMP  —  Total: {}  |  In-World (Spawned): {}", totalCount, worldCount);
        SDK::Log::log("====================================================================================================================================================================================================");
        SDK::Log::log(" {:<3} | {:<16} | {:<7} | {:<12} | {:<36} | {:<10} | {:<24} | {:<7} | {:<16} | {}",
            "#", "Name", "Spawned", "Runtime ID", "UUID", "Platform", "Position (X, Y, Z)", "Dist", "XUID", "Nametag");
        SDK::Log::log("-----+------------------+---------+--------------+--------------------------------------+------------+--------------------------+---------+------------------+---------------------------------");

        for (size_t i = 0; i < players.size(); ++i)
        {
            const std::shared_ptr<TrackedPlayer>& p = players[i];
            if (!p)
            {
                continue;
            }

            std::string posStr = std::format("({:6.1f},{:6.1f},{:6.1f})", p->pos[0], p->pos[1], p->pos[2]);
            std::string distStr = p->isSpawned ? std::format("{:5.1f}m", p->distanceToLocalPlayer()) : "  ---  ";
            std::string spawnStr = p->isSpawned ? "YES" : "NO";
            std::string tagStr = !p->nametag.empty() ? p->nametag : "-";

            SDK::Log::log(" {:<3} | {:<16} | {:<7} | {:<12} | {:<36} | {:<10} | {:<24} | {:<7} | {:<16} | {}",
                i + 1,
                p->name.empty() ? "(none)" : p->name,
                spawnStr,
                p->runtimeEntityId,
                p->uuid.empty() ? "-" : p->uuid,
                p->getPlatformName(),
                posStr,
                distStr,
                p->xuid.empty() ? "-" : p->xuid,
                tagStr);
        }

        SDK::Log::log("====================================================================================================================================================================================================\n");

        SDK::Chat::notify(std::format("Dumped §f{}§7 player(s) (§f{}§7 in 3D world) to log file.", totalCount, worldCount));

        std::vector<std::shared_ptr<TrackedPlayer>> worldPlayers = PlayerTracker::get().getWorldPlayers();
        if (worldPlayers.empty())
        {
            SDK::Chat::send(" §8- §7No players actively rendered in 3D world.");
        }
        else
        {
            for (const std::shared_ptr<TrackedPlayer>& wp : worldPlayers)
            {
                if (wp)
                {
                    float dist = wp->distanceToLocalPlayer();
                    SDK::Chat::send(std::format(" §8- §f{} §7(§e{:.1f}m§7) | R-ID: §a{}§7 | U-ID: §b{}§7 | §e{}",
                        wp->name.empty() ? "(none)" : wp->name,
                        dist,
                        wp->runtimeEntityId,
                        wp->uniqueEntityId,
                        wp->getPlatformName()));
                }
            }
        }
    };
    registerCommand("players", "Dump all tracked players to log and show summary", playersCb);


    // Chat command: test chat message with author name
    CommandCallback chatCb = [](const CommandArgs& args)
    {
        std::string author = "BedrockBot";
        std::string message = "This is a test chat message with an author name!";
        if (!args.empty())
        {
            author = std::string(args.get(0));
            if (args.size() > 1)
            {
                message.clear();
                for (size_t i = 1; i < args.size(); ++i)
                {
                    if (i > 1)
                    {
                        message += " ";
                    }
                    message += std::string(args.get(i));
                }
            }
        }
        SDK::Chat::sendChat(author, message);
    };
    registerCommand("chat", "Test chat message with author name", chatCb);

    // Spawn test sheep entity
    CommandCallback spawnCb = [](const CommandArgs&)
    {
        SDK::LocalPlayer* player = SDK::Game::player();
        if (!player)
        {
            SDK::Chat::error("LocalPlayer not available.");
            return;
        }
        float* pos = player->getPos();
        if (!pos)
        {
            SDK::Chat::error("Player position not available.");
            return;
        }

        std::shared_ptr<SDK::Packet> reply = SDK::Factory::createPacket(SDK::PacketID::ADD_ACTOR);
        if (!reply)
        {
            SDK::Chat::error("Failed to create ADD_ACTOR packet.");
            return;
        }

        SDK::AddActorPacket* pkt = static_cast<SDK::AddActorPacket*>(reply.get());

        static std::atomic<int64_t> s_nextSheepId{0x900000};
        int64_t eid = s_nextSheepId.fetch_add(1);

        pkt->uniqueEntityId = eid;
        pkt->runtimeEntityId = static_cast<uint64_t>(eid);
        pkt->identifier = "minecraft:sheep";
        pkt->pos[0] = pos[0];
        pkt->pos[1] = pos[1] - 1.0f;
        pkt->pos[2] = pos[2];
        pkt->motion[0] = 0;
        pkt->motion[1] = 0;
        pkt->motion[2] = 0;
        pkt->rotation[0] = 0;
        pkt->rotation[1] = 0;
        pkt->headRotation = 0;
        pkt->bodyRotation = 0;

        PacketInterceptor::get().injectInbound(reply);
        SDK::Chat::success("Client-side sheep spawned!");
    };
    registerCommand("spawn", "Spawn client-side test sheep at current position", spawnCb);

    // Spawn floating text entity
    CommandCallback textCb = [](const CommandArgs& args)
    {
        SDK::LocalPlayer* player = SDK::Game::player();
        if (!player)
        {
            SDK::Chat::error("LocalPlayer not available.");
            return;
        }
        float* pos = player->getPos();
        if (!pos)
        {
            SDK::Chat::error("Player position not available.");
            return;
        }

        std::string displayText = "§d[BedrockUtils Floating Text]§r";
        if (!args.empty())
        {
            displayText.clear();
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0)
                {
                    displayText += " ";
                }
                displayText += std::string(args.get(i));
            }
        }

        std::shared_ptr<SDK::Packet> reply = SDK::Factory::createPacket(SDK::PacketID::ADD_ACTOR);
        if (!reply)
        {
            SDK::Chat::error("Failed to create ADD_ACTOR packet.");
            return;
        }

        SDK::AddActorPacket* pkt = static_cast<SDK::AddActorPacket*>(reply.get());

        static std::atomic<int64_t> s_nextTextId{0x800000};
        int64_t eid = s_nextTextId.fetch_add(1);

        pkt->uniqueEntityId = eid;
        pkt->runtimeEntityId = static_cast<uint64_t>(eid);
        pkt->identifier = "minecraft:armor_stand";
        pkt->pos[0] = pos[0];
        pkt->pos[1] = pos[1] - 0.5f;
        pkt->pos[2] = pos[2];
        pkt->motion[0] = 0;
        pkt->motion[1] = 0;
        pkt->motion[2] = 0;
        pkt->rotation[0] = 0;
        pkt->rotation[1] = 0;
        pkt->headRotation = 0;
        pkt->bodyRotation = 0;

        pkt->entityData.items.clear();
        pkt->entityData.set(0, static_cast<int64_t>((1ULL << 14) | (1ULL << 15))); // DATA_FLAGS (CAN_SHOW_NAME | ALWAYS_SHOW_NAME)
        pkt->entityData.set(4, displayText);                                        // DATA_NAMETAG
        pkt->entityData.set(38, 0.0f);                                               // DATA_SCALE (Scale 0 = invisible)
        pkt->entityData.set(80, static_cast<uint8_t>(1));                            // DATA_NAMETAG_ALWAYS_SHOW
        pkt->entityData.set(81, static_cast<uint8_t>(1));

        PacketInterceptor::get().injectInbound(reply);
        SDK::Chat::success(std::format("Floating text spawned: {}", displayText));
    };
    registerCommand("floatingtext", "Spawn client-side floating text armor stand", textCb);

    // Try server command via CommandManager
    CommandCallback tryCmdCb = [](const CommandArgs& args)
    {
        std::string commandStr;
        if (args.empty())
        {
            commandStr = "/help";
        }
        else
        {
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0)
                {
                    commandStr += " ";
                }
                commandStr += std::string(args.get(i));
            }
            if (!commandStr.starts_with("/"))
            {
                commandStr = "/" + commandStr;
            }
        }

        SDK::Log::log("[CoreCommands] Executing server command via CommandManager: \"{}\"", commandStr);
        SDK::Chat::notify(std::format("Executing server command: §f{}", commandStr));

        SDK::CommandRequest req;
        req.command = commandStr;
        req.silent = true;
        req.timeoutMs = 3000;
        req.batchDebounceMs = 150;

        req.onComplete = [commandStr](const SDK::CommandResult& res)
        {
            if (res.isForm())
            {
                SDK::Log::log("[CommandManager] Captured Form #{} (len={}): {}", res.form->id, res.form->json.size(), res.form->json);
                SDK::Chat::success(std::format("Captured Form #{} (len={})", res.form->id, res.form->json.size()));
            }
            else if (res.isText())
            {
                SDK::Log::log("[CommandManager] Received {} text line(s):", res.lines.size());
                for (const std::string& line : res.lines)
                {
                    SDK::Log::log("  > {}", line);
                    SDK::Chat::send(std::format("§b[Captured]§r {}", line));
                }
            }
            else if (res.isTimeout())
            {
                SDK::Log::log("[CommandManager] Command \"{}\" timed out.", commandStr);
                SDK::Chat::warn("Command timed out (no server response).");
            }
            else
            {
                SDK::Log::log("[CommandManager] Command error executing \"{}\".", commandStr);
                SDK::Chat::error("Command execution error.");
            }
        };

        SDK::CommandManager::get().execute(std::move(req));
    };
    registerCommand("trycmd", "Execute server command via CommandManager and capture response", tryCmdCb);

    // Dump command
    CommandCallback dumpCb = [](const CommandArgs& args)
    {
        std::string_view pfx = CommandDispatcher::prefix();
        if (args.empty())
        {
            SDK::Chat::warn(std::format("Usage: {}dump <in|out|any> <packetId> [bytes]", pfx));
            SDK::Chat::warn(std::format("Examples: §f{}dump in 1§e, §f{}dump out 9§e, §f{}dump text§e, §f{}dump cancel", pfx, pfx, pfx, pfx));
            return;
        }

        std::string_view first = args.get(0);
        std::string firstLower = SDK::toLower(first);
        if (firstLower == "cancel" || firstLower == "stop")
        {
            SDK::PacketDumper::get().disarm();
            SDK::Chat::notify("Packet dumper disarmed.");
            return;
        }

        if (firstLower == "status")
        {
            if (SDK::PacketDumper::get().isArmed())
            {
                uint8_t tid = SDK::PacketDumper::get().targetPacketId();
                PacketDirection tdir = SDK::PacketDumper::get().targetDirection();
                std::string dirStr = (tdir == PacketDirection::Inbound) ? "INBOUND" : "OUTBOUND";
                std::string name = SDK::getPacketName(static_cast<SDK::PacketID>(tid));
                SDK::Chat::notify(std::format("Packet dumper is §aARMED§7 for {} packet §e0x{:02x} ({})§7.", dirStr, tid, name));
            }
            else
            {
                SDK::Chat::notify("Packet dumper is currently §cDISARMED§7.");
            }
            return;
        }

        PacketDirection dir = PacketDirection::Inbound;
        bool anyDir = false;
        size_t idArgIdx = 0;

        if (firstLower == "in" || firstLower == "inbound")
        {
            dir = PacketDirection::Inbound;
            idArgIdx = 1;
        }
        else if (firstLower == "out" || firstLower == "outbound")
        {
            dir = PacketDirection::Outbound;
            idArgIdx = 1;
        }
        else if (firstLower == "any" || firstLower == "all")
        {
            anyDir = true;
            idArgIdx = 1;
        }

        if (idArgIdx >= args.size())
        {
            SDK::Chat::warn(std::format("Missing packet ID. Usage: {}dump <in|out|any> <packetId> [bytes]", pfx));
            return;
        }

        std::string_view idToken = args.get(idArgIdx);
        uint8_t packetId = 0;
        std::string packetName;
        if (!SDK::PacketDumper::parsePacketId(idToken, packetId, packetName))
        {
            SDK::Chat::error(std::format("Unknown packet ID or name: '§f{}§c'.", idToken));
            return;
        }

        size_t byteCount = 384;
        if (args.size() > idArgIdx + 1)
        {
            std::string_view byteToken = args.get(idArgIdx + 1);
            try
            {
                unsigned long parsedBytes = std::stoul(std::string(byteToken), nullptr, 10);
                if (parsedBytes > 0)
                {
                    byteCount = static_cast<size_t>(parsedBytes);
                }
            }
            catch (const std::exception&) {}
        }

        SDK::PacketDumper::get().arm(dir, packetId, byteCount, anyDir);

        std::string dirStr = anyDir ? "ANY-DIRECTION" : ((dir == PacketDirection::Inbound) ? "INBOUND" : "OUTBOUND");
        SDK::Chat::notify(std::format("Packet dumper §aARMED§7 for {} packet §e0x{:02x} ({})§7 ({} bytes).",
            dirStr, packetId, packetName, byteCount));
        SDK::Chat::notify("Waiting for next matching packet to dump to §fbutils_dumper.log§7...");
    };
    registerCommand("dump", "Capture and dump packet memory to butils_dumper.log", dumpCb);

    // Eject command
    CommandCallback ejectCb = [](const CommandArgs&)
    {
        SDK::Chat::warn("Ejecting...");
        AppCore::requestEject();
    };
    registerCommand("eject", "Safely uninject and unload BedrockUtils", ejectCb);
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
