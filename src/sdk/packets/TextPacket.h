#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace SDK
{

enum class TextPacketType : uint8_t
{
    RAW = 0,
    CHAT = 1,
    TRANSLATION = 2,
    POPUP = 3,
    JUKEBOX_POPUP = 4,
    TIP = 5,
    SYSTEM_MESSAGE = 6,
    WHISPER = 7,
    ANNOUNCEMENT = 8,
    OBJECT_WHISPER = 9,
    TEXT_OBJECT = 10,
    TEXT_OBJECT_ANNOUNCEMENT = 11,
};

class TextPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::TEXT;

    uint8_t                pad0[8];          // +0x30
    SafeString             xuid;             // +0x38
    SafeString             platformChatId;   // +0x58
    PODVector<SafeString>  parameters;       // +0x78
    uint8_t                pad1[16];         // +0x90
    TextPacketType         type;             // +0xA0
    bool                   needsTranslation; // +0xA1
    uint8_t                pad2[6];          // +0xA2
    SafeString             sourceName;       // +0xA8
    SafeString             message;          // +0xC8
    uint8_t                variantIndex = 0; // +0xE8 (0=monostate/raw, 1=chat message, 2=params)
    uint8_t                unmapped[127];    // +0xE9

    // Resolves text content with fallbacks (message -> sourceName -> parameters[0])
    std::string getMessage() const
    {
        std::string out;
        safeReadTextMessage(this, out);
        return out;
    }

    // Updates text content in whichever field this packet type uses so the game renders it
    void setMessage(std::string_view newMsg)
    {
        if (type == TextPacketType::CHAT || type == TextPacketType::TEXT_OBJECT)
        {
            message = newMsg;
            variantIndex = 1;
        }
        else if (type == TextPacketType::RAW || type == TextPacketType::POPUP ||
                 type == TextPacketType::JUKEBOX_POPUP || type == TextPacketType::TIP ||
                 type == TextPacketType::SYSTEM_MESSAGE || type == TextPacketType::TRANSLATION ||
                 !sourceName.empty())
        {
            sourceName = newMsg;
        }
        else
        {
            message = newMsg;
        }
    }

    TextPacket() = default;
    virtual ~TextPacket() = default;

    PacketID getID() const override { return PacketID::TEXT; }
};

static_assert(offsetof(TextPacket, xuid)             == 0x38, "xuid offset mismatch");
static_assert(offsetof(TextPacket, platformChatId)   == 0x58, "platformChatId offset mismatch");
static_assert(offsetof(TextPacket, parameters)       == 0x78, "parameters offset mismatch");
static_assert(offsetof(TextPacket, type)             == 0xA0, "type offset mismatch");
static_assert(offsetof(TextPacket, needsTranslation) == 0xA1, "needsTranslation offset mismatch");
static_assert(offsetof(TextPacket, sourceName)       == 0xA8, "sourceName offset mismatch");
static_assert(offsetof(TextPacket, message)          == 0xC8, "message offset mismatch");
static_assert(offsetof(TextPacket, variantIndex)     == 0xE8, "variantIndex offset mismatch");

} // namespace SDK
