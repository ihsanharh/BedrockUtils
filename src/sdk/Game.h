#pragma once
#include "Addresses.h"
#include "PacketSender.h"
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace SDK
{

struct StateVectorComponent
{
    float pos[3];
    float posOld[3];
    float velocity[3];
};

struct LocalPlayer
{
    PacketSender* packetSender() const; // +0x7F8
    float* getPos() const;              // StateVector at +0x218
    std::string getName() const;        // playerName at +0xBC0
    std::string getXuid() const;        // xuid via MinecraftGame (+0x250)
};

class ClientInstance;

class MinecraftGame
{
public:
    ClientInstance* getPrimaryClientInstance(); // map at +0x938
};

class Platform_GameCore
{
public:
    MinecraftGame* getMinecraftGame(); // mcgame at +0x18
    static Platform_GameCore* get();
};

class ClientInstance
{
public:
    LocalPlayer* getLocalPlayer();      // vtable[0x1F]
    PacketSender* packetSender();       // +0x1C8
    MinecraftGame* getMinecraftGame();  // +0x1A0
    static ClientInstance* get();
};

struct ServerConnectionDetails
{
    std::string hostIp;
    std::string unresolvedUrl;
    std::string creatorName;
    int port = 0;
    bool isConnected = false;

    bool matches(std::string_view pattern) const;
};

namespace Game
{
    inline LocalPlayer* player()
    {
        ClientInstance* ci = ClientInstance::get();
        return ci ? ci->getLocalPlayer() : nullptr;
    }

    inline PacketSender* sender()
    {
        ClientInstance* ci = ClientInstance::get();
        if (!ci)
        {
            return nullptr;
        }

        PacketSender* s = ci->packetSender();
        if (s)
        {
            return s;
        }

        LocalPlayer* lp = ci->getLocalPlayer();
        return lp ? lp->packetSender() : nullptr;
    }

    // Direct game connection info
    bool getConnectionDetails(ServerConnectionDetails& out);
    ServerConnectionDetails connection();

    // Universal server detection helpers for modules
    bool isConnected();
    bool isServer(std::string_view pattern);

    // Player identity helpers
    void setCachedCredentials(std::string_view name, std::string_view xuid);
    std::string getLocalPlayerName();
    std::string getLocalPlayerXuid();
}

} // namespace SDK
