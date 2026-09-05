#pragma once
#include "sdk/Packet.h"
#include <cstdint>

namespace SDK
{

enum class PlayStatusType : int32_t
{
    LOGIN_SUCCESS = 0,
    LOGIN_FAILED_CLIENT_OLD = 1,
    LOGIN_FAILED_SERVER_OLD = 2,
    PLAYER_SPAWN = 3,
    LOGIN_FAILED_INVALID_TENANT = 4,
    LOGIN_FAILED_EDITION_MISMATCH_EDU_TO_VANILLA = 5,
    LOGIN_FAILED_EDITION_MISMATCH_VANILLA_TO_EDU = 6,
    FAILED_SERVER_FULL_SUB_CLIENT = 7,
    EDITOR_TO_VANILLA_MISMATCH = 8,
    VANILLA_TO_EDITOR_MISMATCH = 9
};

class PlayStatusPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::PLAY_STATUS;

    PlayStatusType status; // +0x30
    uint8_t unmapped[128]; // +0x34

    virtual ~PlayStatusPacket() = default;

    PacketID getID() const override
    {
        return PacketID::PLAY_STATUS;
    }
};

static_assert(offsetof(PlayStatusPacket, status) == 0x30, "status offset mismatch");

} // namespace SDK
