#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>

namespace SDK
{

// ModalFormResponsePacket (0x65)
// Outbound packet sent by the client when submitting or closing a modal form.
class ModalFormResponsePacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::MODAL_FORM_RESPONSE;

    uint32_t formId;         // +0x30
    uint8_t pad[4];          // +0x34
    SafeString responseData; // +0x38 (JSON string or button index)
    uint8_t unmapped[128];   // +0x58

    virtual ~ModalFormResponsePacket() = default;

    PacketID getID() const override
    {
        return PacketID::MODAL_FORM_RESPONSE;
    }
};

static_assert(offsetof(ModalFormResponsePacket, formId) == 0x30, "formId offset mismatch");
static_assert(offsetof(ModalFormResponsePacket, responseData) == 0x38, "responseData offset mismatch");

} // namespace SDK
