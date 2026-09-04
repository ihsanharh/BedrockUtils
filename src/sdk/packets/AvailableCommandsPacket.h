#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>
#include <string_view>

namespace SDK
{

// AvailableCommandsPacket (0x4C): S2C packet synchronizing available server commands
class AvailableCommandsPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::AVAILABLE_COMMANDS;

    struct CommandData
    {
        SafeString name;               // +0x00
        SafeString description;        // +0x20
        uint8_t    flags;              // +0x40
        uint8_t    permission;         // +0x41
        uint16_t   pad1;               // +0x42
        uint32_t   pad2;               // +0x44
        int32_t    aliasesEnumIndex;   // +0x48
        uint32_t   chainedSubCmdIndex; // +0x4C
        uint8_t    overloads[48];      // +0x50

        std::string_view getName() const { return name.view(); }
        std::string_view getDescription() const { return description.view(); }
    };
    static_assert(sizeof(CommandData) == 0x80, "CommandData size must be 128 bytes (0x80)");
    static_assert(offsetof(CommandData, name) == 0x00, "name offset mismatch");
    static_assert(offsetof(CommandData, description) == 0x20, "description offset mismatch");

    PODVector<SafeString>  enumValues;         // +0x30
    PODVector<SafeString>  postFixes;          // +0x48
    uint8_t                enumsRaw[24];       // +0x60
    uint8_t                subCommandData[24]; // +0x78
    uint8_t                softEnums[24];      // +0x90
    PODVector<CommandData> commands;           // +0xA8

    virtual ~AvailableCommandsPacket() = default;

    PacketID getID() const override { return PacketID::AVAILABLE_COMMANDS; }

    bool hasCommand(std::string_view cmdName) const
    {
        for (const CommandData& cmd : commands)
        {
            if (cmd.name.view() == cmdName)
            {
                return true;
            }
        }
        return false;
    }
};

static_assert(offsetof(AvailableCommandsPacket, enumValues) == 0x30, "enumValues offset mismatch");
static_assert(offsetof(AvailableCommandsPacket, postFixes)  == 0x48, "postFixes offset mismatch");
static_assert(offsetof(AvailableCommandsPacket, enumsRaw)   == 0x60, "enumsRaw offset mismatch");
static_assert(offsetof(AvailableCommandsPacket, commands)   == 0xA8, "commands offset mismatch");

} // namespace SDK
