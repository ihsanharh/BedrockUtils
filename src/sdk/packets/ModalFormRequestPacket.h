#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>

namespace SDK
{

class ModalFormRequestPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::MODAL_FORM_REQUEST;

    uint32_t formId;       // +0x30
    uint8_t pad[4];        // +0x34
    SafeString formData;   // +0x38 (JSON string)
    uint8_t unmapped[128]; // +0x58

    virtual ~ModalFormRequestPacket() = default;

    PacketID getID() const override
    {
        return PacketID::MODAL_FORM_REQUEST;
    }
};

static_assert(offsetof(ModalFormRequestPacket, formId) == 0x30, "formId offset mismatch");
static_assert(offsetof(ModalFormRequestPacket, formData) == 0x38, "formData offset mismatch");

} // namespace SDK
