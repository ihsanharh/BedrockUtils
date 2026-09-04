#pragma once
#include "sdk/Packet.h"
#include <cstdint>

namespace SDK
{

// MovePlayerPacket (0x13)
// Inbound packet sent by the server to update a player entity's position and rotation.
class MovePlayerPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::MOVE_PLAYER;

    int64_t runtimeEntityId; // +0x30
    float   pos[3];          // +0x38 (X, Y, Z)
    float   pitch;           // +0x44
    float   yaw;             // +0x48
    float   headYaw;         // +0x4C
    uint8_t mode;            // +0x50
    bool    onGround;        // +0x51
    uint8_t pad[6];          // +0x52
    int64_t ridingRuntimeId; // +0x58
    int32_t cause;           // +0x60
    int32_t sourceEntityType;// +0x64
    int64_t tick;            // +0x68

    virtual ~MovePlayerPacket() = default;

    PacketID getID() const override { return PacketID::MOVE_PLAYER; }
};

static_assert(offsetof(MovePlayerPacket, runtimeEntityId) == 0x30, "runtimeEntityId offset mismatch");
static_assert(offsetof(MovePlayerPacket, pos)             == 0x38, "pos offset mismatch");

} // namespace SDK
