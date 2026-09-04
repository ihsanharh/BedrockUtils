#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"

namespace SDK
{

class CommandRequestPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::COMMAND_REQUEST;

    SafeString command; // +0x30

    struct CommandOriginData
    {
        void**     vtable;    // +0x50
        uint8_t    uuid[16];  // +0x58
        SafeString requestId; // +0x68
        int64_t    playerId;  // +0x88
    } origin;

    uint32_t version = 50;   // +0x90
    bool internal = false;   // +0x94

    virtual ~CommandRequestPacket() = default;

    PacketID getID() const override { return PacketID::COMMAND_REQUEST; }
};

static_assert(offsetof(CommandRequestPacket, command) == 0x30, "command offset mismatch");
static_assert(offsetof(CommandRequestPacket, origin)  == 0x50, "origin offset mismatch");
static_assert(offsetof(CommandRequestPacket::CommandOriginData, requestId) == 0x18, "requestId offset mismatch");
static_assert(offsetof(CommandRequestPacket, version) == 0x90, "version offset mismatch");
static_assert(offsetof(CommandRequestPacket, internal) == 0x94, "internal offset mismatch");

} // namespace SDK
