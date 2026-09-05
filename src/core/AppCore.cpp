#include "AppCore.h"
#include "pch.h"
#include "CommandDispatcher.h"
#include "intercept/PacketInterceptor.h"
#include "pipeline/Pipeline.h"
#include "intercept/SigScan.h"
#include "modules/ModuleRegistry.h"
#include "sdk/Addresses.h"
#include "sdk/Async.h"
#include "sdk/Chat.h"
#include "sdk/CommandManager.h"
#include "sdk/CrashHandler.h"
#include "sdk/Game.h"
#include "services/PlayerTracker.h"
#include <atomic>
#include <format>
#include <iostream>

namespace AppCore
{

static bool g_wasInWorld = false;
static bool g_hasAnnouncedInGame = false;
static bool g_hasLoggedConnection = false;
static std::string g_lastConnectedHost;
static std::atomic<bool> g_ejectRequested{false};

void requestEject()
{
    g_ejectRequested.store(true);
}

bool shouldEject()
{
    return g_ejectRequested.load();
}

void start()
{
    g_ejectRequested.store(false);
    g_wasInWorld = false;
    g_hasAnnouncedInGame = false;
    g_hasLoggedConnection = false;
    g_lastConnectedHost.clear();

    SDK::Log::Logger::get().init();
    SDK::Crash::install();
    SDK::Async::init();

    SDK::Log::log("==============================");
    SDK::Log::log("         BedrockUtils         ");
    SDK::Log::log("==============================");

    SDK::Crash::g_lastCheckpoint.store("PacketInterceptor::install");
    PacketInterceptor::get().install();

    SDK::Crash::g_lastCheckpoint.store("CommandDispatcher::init");
    CommandDispatcher::get().init();

    SDK::Crash::g_lastCheckpoint.store("CommandManager::init");
    SDK::CommandManager::get().init();

    SDK::Crash::g_lastCheckpoint.store("PlayerTracker::init");
    PlayerTracker::get().init();

    SDK::Crash::g_lastCheckpoint.store("PacketSender::hookSender");
    if (SDK::PacketSender* s = SDK::Game::sender())
    {
        PacketInterceptor::get().hookSender(s);
    }

    SDK::Crash::g_lastCheckpoint.store("ModuleRegistry::init");
    ModuleRegistry::get().init();

    SDK::Log::log("[AppCore] {} module(s) loaded", ModuleRegistry::get().all().size());
    SDK::Log::log("[AppCore] Press DELETE or type {}eject to eject", CommandDispatcher::prefix());

    SDK::Crash::g_lastCheckpoint.store("AppCore::start complete");

    if (SDK::Game::player())
    {
        SDK::ServerConnectionDetails det = SDK::Game::connection();
        g_hasLoggedConnection = true;
        g_lastConnectedHost = det.hostIp;
        SDK::Log::log("[AppCore] Connection details: Host='{}' URL='{}' Creator='{}' Connected={}",
            det.hostIp, det.unresolvedUrl, det.creatorName, det.isConnected);

        if (PacketInterceptor::get().hasActiveSession())
        {
            g_hasAnnouncedInGame = true;
            SDK::Chat::send(std::format("§a§l[BedrockUtils]§r §7Loaded successfully. Type §f{}modules§7 or §f{}eject§7.",
                CommandDispatcher::prefix(), CommandDispatcher::prefix()));
        }
    }
}

void tick()
{
    SDK::Crash::g_lastCheckpoint.store("AppCore::tick entry");
    __try
    {
        // Outbound hook retry if sender was not ready at start()
        if (!PacketInterceptor::get().isOutboundHooked())
        {
            if (SDK::PacketSender* s = SDK::Game::sender())
            {
                SDK::Crash::g_lastCheckpoint.store("AppCore::tick hookSender");
                PacketInterceptor::get().hookSender(s);
            }
        }

        // Detect server disconnect / return to menu
        bool inWorld = (SDK::Game::player() != nullptr);
        bool isConnected = inWorld && SDK::Game::isConnected();
        if (g_wasInWorld && !inWorld && !isConnected)
        {
            SDK::Log::log("[AppCore] Disconnected from server (returned to menu). Resetting session.");
            SDK::CommandManager::get().shutdown();
            PlayerTracker::get().reset();
            PacketInterceptor::get().resetSession();
            g_hasAnnouncedInGame = false;
            g_hasLoggedConnection = false;
            g_wasInWorld = false;
            g_lastConnectedHost.clear();
        }
        else if (inWorld)
        {
            g_wasInWorld = true;

            // Log connection details on joining a server (or if host changes)
            SDK::ServerConnectionDetails det = SDK::Game::connection();
            if (!g_hasLoggedConnection || (det.isConnected && !det.hostIp.empty() && det.hostIp != g_lastConnectedHost))
            {
                if (det.isConnected || !det.hostIp.empty() || !det.creatorName.empty())
                {
                    g_hasLoggedConnection = true;
                    g_lastConnectedHost = det.hostIp;
                    SDK::Log::log("[AppCore] Connection details: Host='{}' URL='{}' Creator='{}' Connected={}",
                        det.hostIp, det.unresolvedUrl, det.creatorName, det.isConnected);
                }
            }
        }

        // Announce in chat on first in-world tick if injected at menu or rejoined
        if (!g_hasAnnouncedInGame && inWorld && PacketInterceptor::get().hasActiveSession())
        {
            g_hasAnnouncedInGame = true;
            SDK::Chat::send(std::format("§a§l[BedrockUtils]§r §7Loaded successfully. Type §f{}modules§7 or §f{}eject§7.",
                CommandDispatcher::prefix(), CommandDispatcher::prefix()));
        }

        // Tick CommandManager and active modules
        SDK::Crash::g_lastCheckpoint.store("CommandManager::tick");
        SDK::CommandManager::get().tick();

        if (inWorld)
        {
            SDK::Crash::g_lastCheckpoint.store("ModuleRegistry::tick");
            ModuleRegistry::get().tick();
        }

        SDK::Crash::g_lastCheckpoint.store("PacketInterceptor::flushInbound");
        PacketInterceptor::get().flushInbound();

        SDK::Crash::g_lastCheckpoint.store("AppCore::tick complete");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[AppCore] Handled exception in tick() @ checkpoint: {}", SDK::Crash::g_lastCheckpoint.load());
    }
}

void stop()
{
    __try
    {
        g_ejectRequested.store(true);
        SDK::Log::log("[AppCore] Ejecting...");

        if (SDK::Game::player() && PacketInterceptor::get().hasActiveSession())
        {
            SDK::Chat::send("§c§l[BedrockUtils]§r §7Ejecting...");
        }

        Sleep(100);

        // 1. Unload and disable all modules cleanly
        SDK::Log::log("[AppCore] stop: unloading modules...");
        ModuleRegistry::get().clear();

        // 2. Shut down thread pool
        SDK::Log::log("[AppCore] stop: shutting down async thread pool...");
        SDK::Async::shutdown();

        // 3. Flush injected packets
        PacketInterceptor::get().flushInbound();

        // 4. Uninstall hooks
        SDK::Log::log("[AppCore] stop: uninstalling hooks...");
        PacketInterceptor::get().uninstall();

        // 5. Clear registries & services
        SDK::Log::log("[AppCore] stop: clearing registries and services...");
        Pipeline::get().clear();
        CommandDispatcher::get().shutdown();
        SDK::CommandManager::get().shutdown();
        PlayerTracker::get().shutdown();
        PacketInterceptor::get().resetSession();

        // 6. Uninstall crash filter
        SDK::Log::log("[AppCore] stop: uninstalling crash handler...");
        SDK::Crash::uninstall();

        SDK::Log::log("[AppCore] Clean shutdown complete.");
        SDK::Log::Logger::get().close();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[AppCore] Exception in stop() caught by SEH handler!");
        SDK::Log::Logger::get().close();
    }
}

} // namespace AppCore
