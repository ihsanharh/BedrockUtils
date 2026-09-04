#pragma once
#include "sdk/Packet.h"
#include <cstdint>

namespace SDK
{

// MoveActorDeltaPacket (0x6F)
// Inbound packet sent by the server to update an actor's delta or absolute position.
class MoveActorDeltaPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::MOVE_ACTOR_DELTA;

    int64_t  runtimeEntityId; // +0x30
    uint16_t flags;           // +0x38
    uint8_t  pad[2];          // +0x3A
    float    pos[3];          // +0x3C (X, Y, Z)
    uint8_t  pitch;           // +0x48
    uint8_t  yaw;             // +0x49
    uint8_t  headYaw;         // +0x4A
    uint8_t  pad2[5];         // +0x4B
    uint8_t  unmapped[128];   // +0x50

    virtual ~MoveActorDeltaPacket() = default;

    PacketID getID() const override { return PacketID::MOVE_ACTOR_DELTA; }
};

static_assert(offsetof(MoveActorDeltaPacket, runtimeEntityId) == 0x30, "runtimeEntityId offset mismatch");
static_assert(offsetof(MoveActorDeltaPacket, flags)           == 0x38, "flags offset mismatch");
static_assert(offsetof(MoveActorDeltaPacket, pos)             == 0x3C, "pos offset mismatch");
static_assert(offsetof(MoveActorDeltaPacket, pitch)           == 0x48, "pitch offset mismatch");
static_assert(offsetof(MoveActorDeltaPacket, yaw)             == 0x49, "yaw offset mismatch");
static_assert(offsetof(MoveActorDeltaPacket, headYaw)         == 0x4A, "headYaw offset mismatch");

} // namespace SDK
