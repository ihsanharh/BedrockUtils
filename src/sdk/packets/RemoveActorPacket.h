#pragma once
#include "sdk/Packet.h"
#include <cstdint>

namespace SDK
{

class RemoveActorPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::REMOVE_ACTOR;

    int64_t uniqueEntityId; // +0x30

    virtual ~RemoveActorPacket() = default;

    PacketID getID() const override { return PacketID::REMOVE_ACTOR; }
};

static_assert(offsetof(RemoveActorPacket, uniqueEntityId) == 0x30, "uniqueEntityId offset mismatch");

} // namespace SDK
