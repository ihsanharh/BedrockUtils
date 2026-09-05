#pragma once
#include "sdk/DataItem.h"
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>

namespace SDK
{

class AddActorPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::ADD_ACTOR;

    int64_t uniqueEntityId;  // +0x30
    int64_t runtimeEntityId; // +0x38
    SafeString identifier;   // +0x40
    float pos[3];            // +0x60
    float motion[3];         // +0x6C
    float rotation[2];       // +0x78
    float headRotation;      // +0x80
    float bodyRotation;      // +0x84

    uint8_t attributes[24];          // +0x88 POD buffer for std::vector<AttributeData>
    SynchedActorData entityData;     // +0xA0
    uint8_t entityLinks[24];         // +0xB8 POD buffer for std::vector<EntityLink>
    uint8_t entityPropertiesPad[64]; // +0xD0

    virtual ~AddActorPacket() = default;

    PacketID getID() const override
    {
        return PacketID::ADD_ACTOR;
    }
};

static_assert(offsetof(AddActorPacket, uniqueEntityId) == 0x30, "uniqueEntityId offset mismatch");
static_assert(offsetof(AddActorPacket, runtimeEntityId) == 0x38, "runtimeEntityId offset mismatch");
static_assert(offsetof(AddActorPacket, identifier) == 0x40, "identifier offset mismatch");
static_assert(offsetof(AddActorPacket, pos) == 0x60, "pos offset mismatch");
static_assert(offsetof(AddActorPacket, motion) == 0x6C, "motion offset mismatch");
static_assert(offsetof(AddActorPacket, rotation) == 0x78, "rotation offset mismatch");
static_assert(offsetof(AddActorPacket, headRotation) == 0x80, "headRotation offset mismatch");
static_assert(offsetof(AddActorPacket, bodyRotation) == 0x84, "bodyRotation offset mismatch");
static_assert(offsetof(AddActorPacket, attributes) == 0x88, "attributes offset mismatch");
static_assert(offsetof(AddActorPacket, entityData) == 0xA0, "entityData offset mismatch");
static_assert(offsetof(AddActorPacket, entityLinks) == 0xB8, "entityLinks offset mismatch");

} // namespace SDK
