#pragma once
#include <cstdint>
#include <string>

namespace SDK
{

enum class PacketID : uint8_t
{
    NONE = 0x00,
    PLAY_STATUS = 0x02,
    DISCONNECT = 0x05,
    TEXT = 0x09,
    ADD_PLAYER = 0x0C,
    ADD_ACTOR = 0x0D,
    REMOVE_ACTOR = 0x0E,
    MOVE_ACTOR_ABSOLUTE = 0x12,
    MOVE_PLAYER = 0x13,
    CHANGE_DIMENSION = 0x3D,
    PLAYER_LIST = 0x3F,
    AVAILABLE_COMMANDS = 0x4C,
    COMMAND_REQUEST = 0x4D,
    COMMAND_OUTPUT = 0x4F,
    TRANSFER = 0x55,
    MODAL_FORM_REQUEST = 0x64,
    MODAL_FORM_RESPONSE = 0x65,
    MOVE_ACTOR_DELTA = 0x6F,
    COUNT,
};

[[nodiscard]] constexpr const char* getPacketName(PacketID id) noexcept
{
    switch (id)
    {
    case PacketID::NONE:                return "NONE";
    case PacketID::PLAY_STATUS:         return "PLAY_STATUS";
    case PacketID::DISCONNECT:          return "DISCONNECT";
    case PacketID::TEXT:                return "TEXT";
    case PacketID::ADD_PLAYER:          return "ADD_PLAYER";
    case PacketID::ADD_ACTOR:           return "ADD_ACTOR";
    case PacketID::REMOVE_ACTOR:        return "REMOVE_ACTOR";
    case PacketID::MOVE_ACTOR_ABSOLUTE: return "MOVE_ACTOR_ABSOLUTE";
    case PacketID::MOVE_PLAYER:         return "MOVE_PLAYER";
    case PacketID::CHANGE_DIMENSION:    return "CHANGE_DIMENSION";
    case PacketID::PLAYER_LIST:         return "PLAYER_LIST";
    case PacketID::AVAILABLE_COMMANDS:  return "AVAILABLE_COMMANDS";
    case PacketID::COMMAND_REQUEST:     return "COMMAND_REQUEST";
    case PacketID::COMMAND_OUTPUT:      return "COMMAND_OUTPUT";
    case PacketID::TRANSFER:            return "TRANSFER";
    case PacketID::MODAL_FORM_REQUEST:  return "MODAL_FORM_REQUEST";
    case PacketID::MODAL_FORM_RESPONSE: return "MODAL_FORM_RESPONSE";
    case PacketID::MOVE_ACTOR_DELTA:    return "MOVE_ACTOR_DELTA";
    default:                            return "UNKNOWN";
    }
}

// base packet layout in minecraft bedrock
class Packet
{
public:
    int32_t priority = 2;        // +0x08
    int32_t reliability = 1;     // +0x0C
    uint8_t subClientId = 0;     // +0x10
    bool isHandled = false;      // +0x11
    void* unknown = nullptr;     // +0x18
    void*** handler = nullptr;   // +0x20 points to dispatcher vtable (slot 1 is handle)
    int32_t compressibility = 0; // +0x28

    virtual ~Packet() = default;

    [[nodiscard]] virtual PacketID getID() const
    {
        return PacketID::NONE;
    }

    [[nodiscard]] virtual std::string getName() const
    {
        return "Packet";
    }

    virtual void write(void*) {}
    virtual void readExtended(void*) {}

    [[nodiscard]] virtual bool allowBatch()
    {
        return false;
    }

    virtual void _read(void*) {}
};

} // namespace SDK
