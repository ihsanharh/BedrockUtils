#pragma once
#include "sdk/Packet.h"
#include <cstdint>

namespace SDK
{

class ChangeDimensionPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::CHANGE_DIMENSION;

    int32_t dimension;           // +0x30 (0=Overworld, 1=Nether, 2=The End)
    float pos[3];                // +0x34
    bool respawn;                // +0x40
    uint8_t unmapped[128];       // +0x41

    virtual ~ChangeDimensionPacket() = default;

    PacketID getID() const override { return PacketID::CHANGE_DIMENSION; }
};

static_assert(offsetof(ChangeDimensionPacket, dimension) == 0x30, "dimension offset mismatch");
static_assert(offsetof(ChangeDimensionPacket, pos)       == 0x34, "pos offset mismatch");
static_assert(offsetof(ChangeDimensionPacket, respawn)   == 0x40, "respawn offset mismatch");

} // namespace SDK
