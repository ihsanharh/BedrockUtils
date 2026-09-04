#pragma once
#include "sdk/Packet.h"
#include <cstdint>

namespace SDK
{

class MoveActorAbsolutePacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::MOVE_ACTOR_ABSOLUTE;

    int64_t runtimeEntityId; // +0x30
    uint8_t flags;           // +0x38
    uint8_t pad[3];          // +0x39
    float   pos[3];          // +0x3C
    uint8_t pitch;           // +0x48
    uint8_t yaw;             // +0x49
    uint8_t headYaw;         // +0x4A
    uint8_t unmapped[128];   // +0x4B

    virtual ~MoveActorAbsolutePacket() = default;

    PacketID getID() const override { return PacketID::MOVE_ACTOR_ABSOLUTE; }
};

static_assert(offsetof(MoveActorAbsolutePacket, runtimeEntityId) == 0x30, "runtimeEntityId offset mismatch");
static_assert(offsetof(MoveActorAbsolutePacket, flags)           == 0x38, "flags offset mismatch");
static_assert(offsetof(MoveActorAbsolutePacket, pos)             == 0x3C, "pos offset mismatch");
static_assert(offsetof(MoveActorAbsolutePacket, pitch)           == 0x48, "pitch offset mismatch");
static_assert(offsetof(MoveActorAbsolutePacket, yaw)             == 0x49, "yaw offset mismatch");
static_assert(offsetof(MoveActorAbsolutePacket, headYaw)         == 0x4A, "headYaw offset mismatch");

} // namespace SDK
