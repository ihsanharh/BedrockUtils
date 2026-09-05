#include "pch.h"
#include "Game.h"
#include "Addresses.h"
#include "SafeString.h"
#include "intercept/PacketInterceptor.h"
#include <mutex>
#include <windows.h>

namespace SDK
{

Platform_GameCore* Platform_GameCore::get()
{
    __try
    {
        if (!Addresses::g_platformGameCore)
        {
            return nullptr;
        }

        void* winMain = *reinterpret_cast<void**>(Addresses::g_platformGameCore);
        if (!winMain)
        {
            return nullptr;
        }

        void* platform = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(winMain) + 0x8);
        if (!platform)
        {
            return nullptr;
        }

        return reinterpret_cast<Platform_GameCore*>(platform);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

MinecraftGame* Platform_GameCore::getMinecraftGame()
{
    __try
    {
        void* mcgame = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(this) + 0x18);
        if (!mcgame)
        {
            return nullptr;
        }

        return reinterpret_cast<MinecraftGame*>(mcgame);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

ClientInstance* MinecraftGame::getPrimaryClientInstance()
{
    __try
    {
        const uintptr_t mapAddr = reinterpret_cast<uintptr_t>(this) + 0x938;
        const size_t mapSize = *reinterpret_cast<const size_t*>(mapAddr + 8);
        if (mapSize == 0 || mapSize > 16)
        {
            return nullptr;
        }

        using ClientMap = std::map<uint8_t, std::shared_ptr<ClientInstance>>;
        ClientMap* clients = reinterpret_cast<ClientMap*>(mapAddr);

        ClientMap::iterator it = clients->find(0);
        if (it != clients->end() && it->second)
        {
            return it->second.get();
        }

        return nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

ClientInstance* ClientInstance::get()
{
    __try
    {
        Platform_GameCore* platform = Platform_GameCore::get();
        if (!platform)
        {
            return nullptr;
        }

        MinecraftGame* mcgame = platform->getMinecraftGame();
        if (!mcgame)
        {
            return nullptr;
        }

        return mcgame->getPrimaryClientInstance();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

LocalPlayer* ClientInstance::getLocalPlayer()
{
    __try
    {
        void** vtable = *reinterpret_cast<void***>(this);
        if (!vtable)
        {
            return nullptr;
        }

        using Fn = LocalPlayer*(__fastcall*)(ClientInstance*);
        Fn fn = reinterpret_cast<Fn>(vtable[0x1F]);
        return fn ? fn(this) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

PacketSender* ClientInstance::packetSender()
{
    __try
    {
        // Standard offset 0x1C8 for PacketSender on ClientInstance
        PacketSender* sender = *reinterpret_cast<PacketSender* const*>(reinterpret_cast<uintptr_t>(this) + 0x1C8);
        if (sender)
        {
            void** vtable = *reinterpret_cast<void***>(sender);
            if (vtable)
            {
                return sender;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

MinecraftGame* ClientInstance::getMinecraftGame()
{
    __try
    {
        MinecraftGame* mg = *reinterpret_cast<MinecraftGame* const*>(reinterpret_cast<uintptr_t>(this) + 0x1A0);
        return mg;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

PacketSender* LocalPlayer::packetSender() const
{
    __try
    {
        PacketSender* sender = *reinterpret_cast<PacketSender* const*>(reinterpret_cast<uintptr_t>(this) + 0x7F8);
        return sender;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

float* LocalPlayer::getPos() const
{
    __try
    {
        StateVectorComponent** stateVector = reinterpret_cast<StateVectorComponent**>(reinterpret_cast<uintptr_t>(this) + 0x218);
        if (stateVector && *stateVector)
        {
            return (*stateVector)->pos;
        }
        return nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

std::string LocalPlayer::getName() const
{
    __try
    {
        std::string name;
        if (safeReadString(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0xBC0), name))
        {
            return name;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return "";
}

std::string LocalPlayer::getXuid() const
{
    __try
    {
        ClientInstance* ci = ClientInstance::get();
        if (ci)
        {
            MinecraftGame* mg = ci->getMinecraftGame();
            if (mg)
            {
                std::string xuid;
                if (safeReadString(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(mg) + 0x250), xuid))
                {
                    return xuid;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return "";
}

static bool readEngineConnectionInfo(ServerConnectionDetails& out)
{
    __try
    {
        ClientInstance* ci = ClientInstance::get();
        if (!ci)
        {
            return false;
        }

        PacketSender* ps = ci->packetSender();
        if (!ps && ci->getLocalPlayer())
        {
            ps = ci->getLocalPlayer()->packetSender();
        }
        if (!ps)
        {
            return false;
        }

        // PacketSender->networkSystem is at offset 0x20
        uint8_t* psBytes = reinterpret_cast<uint8_t*>(ps);
        void* netSys = *reinterpret_cast<void**>(psBytes + 0x20);
        if (!netSys)
        {
            return false;
        }

        // NetworkSystem->remoteConnector is at offset 0xF8
        uint8_t* netSysBytes = reinterpret_cast<uint8_t*>(netSys);
        void* composite = *reinterpret_cast<void**>(netSysBytes + 0xF8);
        if (!composite)
        {
            return false;
        }

        uint8_t* compBytes = reinterpret_cast<uint8_t*>(composite);
        uint8_t* ownerControlBlock = *reinterpret_cast<uint8_t**>(compBytes + 0x50);
        void* networkSessionOwner = *reinterpret_cast<void**>(compBytes + 0x60);
        if (!ownerControlBlock || !*ownerControlBlock || !networkSessionOwner)
        {
            return false;
        }

        uint8_t* sessOwnerBytes = reinterpret_cast<uint8_t*>(networkSessionOwner);
        void* sessionInfo = *reinterpret_cast<void**>(sessOwnerBytes + 0x18);
        bool usesNetherNet = sessionInfo && *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(sessionInfo) + 0x18) == 2;

        void* activeConnector = usesNetherNet ? *reinterpret_cast<void**>(compBytes + 0x68)
                                             : *reinterpret_cast<void**>(compBytes + 0x70);
        if (!activeConnector)
        {
            return false;
        }

        void** vtable = *reinterpret_cast<void***>(activeConnector);
        if (!vtable || !vtable[3])
        {
            return false;
        }

        using GetGameInfoFn = void* (*)(void*);
        GetGameInfoFn fn = reinterpret_cast<GetGameInfoFn>(vtable[3]);
        void* gameInfo = fn(activeConnector);
        if (!gameInfo)
        {
            return false;
        }

        uint8_t* infoBytes = reinterpret_cast<uint8_t*>(gameInfo);

        std::string hostIp, unresolvedUrl, creatorName;
        safeReadString(infoBytes + 0x08, hostIp);
        safeReadString(infoBytes + 0x28, unresolvedUrl);
        safeReadString(infoBytes + 0x130, creatorName);
        int port = *reinterpret_cast<int*>(infoBytes + 0x8C);

        if (!hostIp.empty())
        {
            out.hostIp = std::move(hostIp);
        }
        if (!unresolvedUrl.empty())
        {
            out.unresolvedUrl = std::move(unresolvedUrl);
        }
        if (!creatorName.empty())
        {
            out.creatorName = std::move(creatorName);
        }
        if (port > 0)
        {
            out.port = port;
        }

        return !out.hostIp.empty() || !out.unresolvedUrl.empty() || !out.creatorName.empty();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

namespace Game
{

bool getConnectionDetails(ServerConnectionDetails& out)
{
    out.isConnected = isConnected();
    if (!readEngineConnectionInfo(out))
    {
        out.hostIp = PacketInterceptor::get().getConnectedServer();
        out.unresolvedUrl = PacketInterceptor::get().getTransferHost();
    }
    return out.isConnected;
}

ServerConnectionDetails connection()
{
    ServerConnectionDetails details;
    getConnectionDetails(details);
    return details;
}

bool isConnected()
{
    return (SDK::Game::player() != nullptr) && PacketInterceptor::get().hasActiveSession();
}

bool isServer(std::string_view pattern)
{
    if (pattern.empty() || !isConnected())
    {
        return false;
    }
    return connection().matches(pattern);
}

static std::string s_cachedPlayerName;
static std::string s_cachedPlayerXuid;
static std::mutex  s_credMutex;

void setCachedCredentials(std::string_view name, std::string_view xuid)
{
    std::lock_guard<std::mutex> lk(s_credMutex);
    if (!name.empty())
    {
        s_cachedPlayerName = std::string(name);
    }
    if (!xuid.empty())
    {
        s_cachedPlayerXuid = std::string(xuid);
    }
}

std::string getLocalPlayerName()
{
    LocalPlayer* lp = player();
    if (lp)
    {
        std::string name = lp->getName();
        if (!name.empty())
        {
            return name;
        }
    }

    std::lock_guard<std::mutex> lk(s_credMutex);
    return s_cachedPlayerName;
}

std::string getLocalPlayerXuid()
{
    LocalPlayer* lp = player();
    if (lp)
    {
        std::string xuid = lp->getXuid();
        if (!xuid.empty())
        {
            return xuid;
        }
    }

    std::lock_guard<std::mutex> lk(s_credMutex);
    return s_cachedPlayerXuid;
}

} // namespace Game

bool ServerConnectionDetails::matches(std::string_view pattern) const
{
    if (!isConnected || pattern.empty())
    {
        return false;
    }
    std::string pat = SDK::toLower(pattern);
    return SDK::toLower(unresolvedUrl).find(pat) != std::string::npos ||
           SDK::toLower(hostIp).find(pat) != std::string::npos ||
           SDK::toLower(creatorName).find(pat) != std::string::npos;
}

} // namespace SDK
