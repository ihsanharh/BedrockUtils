#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>
#include <vector>

namespace SDK
{

class PlayerListPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::PLAYER_LIST;

    struct Entry
    {
        uint8_t    action;               // +0x00 (0 = ADD, 1 = REMOVE)
        uint8_t    pad[7];               // +0x01
        uint8_t    uuid[16];             // +0x08
        int64_t    entityId;             // +0x18
        SafeString name;                 // +0x20
        SafeString xuid;                 // +0x40
        SafeString platformOnlineId;     // +0x60
        uint32_t   buildPlatform;        // +0x80
        uint8_t    pad2[52];             // +0x84 padding for skin data, etc.

        std::string getUuidString() const { return SDK::uuidToString(uuid); }
    };

    std::vector<Entry> entries;          // +0x30
    uint8_t            unmapped[128];    // +0x48

    virtual ~PlayerListPacket() = default;
    
    PacketID getID() const override { return PacketID::PLAYER_LIST; }
};

static_assert(offsetof(PlayerListPacket, entries) == 0x30, "entries offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, action)           == 0x00, "action offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, uuid)             == 0x08, "uuid offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, entityId)         == 0x18, "entityId offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, name)             == 0x20, "name offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, xuid)             == 0x40, "xuid offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, platformOnlineId) == 0x60, "platformOnlineId offset mismatch");
static_assert(offsetof(PlayerListPacket::Entry, buildPlatform)    == 0x80, "buildPlatform offset mismatch");

} // namespace SDK
