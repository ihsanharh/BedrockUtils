#include "pch.h"
#include "Game.h"
#include "Addresses.h"
#include "SafeMem.h"
#include "SafeString.h"
#include "intercept/PacketInterceptor.h"
#include <mutex>
#include <windows.h>

namespace SDK
{

Platform_GameCore* Platform_GameCore::get()
{
    if (!Addresses::g_platformGameCore)
    {
        return nullptr;
    }

    void* winMain = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(Addresses::g_platformGameCore), &winMain, sizeof(winMain)) || !winMain)
    {
        return nullptr;
    }

    void* platform = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(winMain) + 0x8), &platform, sizeof(platform)) || !platform)
    {
        return nullptr;
    }

    return reinterpret_cast<Platform_GameCore*>(platform);
}

MinecraftGame* Platform_GameCore::getMinecraftGame()
{
    void* mcgame = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0x18), &mcgame, sizeof(mcgame)) || !mcgame)
    {
        return nullptr;
    }
    return reinterpret_cast<MinecraftGame*>(mcgame);
}

ClientInstance* MinecraftGame::getPrimaryClientInstance()
{
    const uintptr_t mapAddr = reinterpret_cast<uintptr_t>(this) + 0x938;
    size_t mapSize = 0;
    if (!Memory::read(reinterpret_cast<const void*>(mapAddr + 8), &mapSize, sizeof(mapSize)))
    {
        return nullptr;
    }
    if (mapSize == 0 || mapSize > 16)
    {
        return nullptr;
    }

    using ClientMap = std::map<uint8_t, std::shared_ptr<ClientInstance>>;
    const ClientMap* clients = reinterpret_cast<const ClientMap*>(mapAddr);

    __try
    {
        ClientMap::const_iterator it = clients->find(0);
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

LocalPlayer* ClientInstance::getLocalPlayer()
{
    void** vtable = nullptr;
    if (!Memory::read(this, &vtable, sizeof(vtable)) || !vtable)
    {
        return nullptr;
    }

    void* fnPtr = nullptr;
    if (!Memory::read(&vtable[0x1F], &fnPtr, sizeof(fnPtr)) || !fnPtr)
    {
        return nullptr;
    }

    using Fn = LocalPlayer*(__fastcall*)(ClientInstance*);
    Fn fn = reinterpret_cast<Fn>(fnPtr);

    __try
    {
        return fn(this);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

PacketSender* ClientInstance::packetSender()
{
    // Standard offset 0x1C8 for PacketSender on ClientInstance
    PacketSender* sender = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0x1C8), &sender, sizeof(sender)) || !sender)
    {
        return nullptr;
    }

    void** vtable = nullptr;
    if (!Memory::read(sender, &vtable, sizeof(vtable)) || !vtable)
    {
        return nullptr;
    }

    return sender;
}

MinecraftGame* ClientInstance::getMinecraftGame()
{
    MinecraftGame* mg = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0x1A0), &mg, sizeof(mg)))
    {
        return nullptr;
    }
    return mg;
}

PacketSender* LocalPlayer::packetSender() const
{
    PacketSender* sender = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0x7F8), &sender, sizeof(sender)))
    {
        return nullptr;
    }
    return sender;
}

float* LocalPlayer::getPos() const
{
    StateVectorComponent* stateVector = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0x218), &stateVector, sizeof(stateVector)) || !stateVector)
    {
        return nullptr;
    }

    if (!Memory::isValidReadPtr(stateVector, sizeof(StateVectorComponent)))
    {
        return nullptr;
    }

    return stateVector->pos;
}

std::string LocalPlayer::getName() const
{
    std::string name;
    if (safeReadString(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(this) + 0xBC0), name))
    {
        return name;
    }
    return "";
}

std::string LocalPlayer::getXuid() const
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
    return "";
}

static void* safeCallGetGameInfo(void* (*fn)(void*), void* connector) noexcept
{
    if (!fn || !connector)
    {
        return nullptr;
    }

    __try
    {
        return fn(connector);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static bool readEngineConnectionInfo(ServerConnectionDetails& out)
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
    void* netSys = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(ps) + 0x20), &netSys, sizeof(netSys)) || !netSys)
    {
        return false;
    }

    // NetworkSystem->remoteConnector is at offset 0xF8
    void* composite = nullptr;
    if (!Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(netSys) + 0xF8), &composite, sizeof(composite)) || !composite)
    {
        return false;
    }

    uint8_t* compBytes = reinterpret_cast<uint8_t*>(composite);
    uint8_t* ownerControlBlock = nullptr;
    void* networkSessionOwner = nullptr;
    if (!Memory::read(compBytes + 0x50, &ownerControlBlock, sizeof(ownerControlBlock)) || !ownerControlBlock)
    {
        return false;
    }

    uint8_t ctrlVal = 0;
    if (!Memory::read(ownerControlBlock, &ctrlVal, sizeof(ctrlVal)) || ctrlVal == 0)
    {
        return false;
    }

    if (!Memory::read(compBytes + 0x60, &networkSessionOwner, sizeof(networkSessionOwner)) || !networkSessionOwner)
    {
        return false;
    }

    void* sessionInfo = nullptr;
    Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(networkSessionOwner) + 0x18), &sessionInfo, sizeof(sessionInfo));

    int netherNetVal = 0;
    if (sessionInfo)
    {
        Memory::read(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(sessionInfo) + 0x18), &netherNetVal, sizeof(netherNetVal));
    }
    bool usesNetherNet = (sessionInfo != nullptr && netherNetVal == 2);

    void* activeConnector = nullptr;
    size_t connectorOffset = usesNetherNet ? 0x68 : 0x70;
    if (!Memory::read(compBytes + connectorOffset, &activeConnector, sizeof(activeConnector)) || !activeConnector)
    {
        return false;
    }

    void** vtable = nullptr;
    if (!Memory::read(activeConnector, &vtable, sizeof(vtable)) || !vtable)
    {
        return false;
    }

    void* getGameInfoFnPtr = nullptr;
    if (!Memory::read(&vtable[3], &getGameInfoFnPtr, sizeof(getGameInfoFnPtr)) || !getGameInfoFnPtr)
    {
        return false;
    }

    using GetGameInfoFn = void* (*)(void*);
    GetGameInfoFn fn = reinterpret_cast<GetGameInfoFn>(getGameInfoFnPtr);
    void* gameInfo = safeCallGetGameInfo(fn, activeConnector);
    if (!gameInfo)
    {
        return false;
    }

    uint8_t* infoBytes = reinterpret_cast<uint8_t*>(gameInfo);

    std::string hostIp, unresolvedUrl, creatorName;
    safeReadString(infoBytes + 0x08, hostIp);
    safeReadString(infoBytes + 0x28, unresolvedUrl);
    safeReadString(infoBytes + 0x130, creatorName);

    int port = 0;
    Memory::read(infoBytes + 0x8C, &port, sizeof(port));

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
