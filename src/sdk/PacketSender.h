#pragma once
#include "Packet.h"

namespace SDK
{

// Abstract PacketSender interface.
//
// Minecraft Bedrock vtable layout (LoopbackPacketSender):
//   [0]  ~PacketSender()
//   [1]  send(Packet*)
//   [2]  sendToServer(Packet*)
//   [3]  sendToClient(networkIdentifier, packet, subClientId)
//
class PacketSender
{
public:
    virtual ~PacketSender() = default;
    virtual void send(Packet* pkt) = 0;
    virtual void sendToServer(Packet* pkt) = 0;
    virtual void sendToClient(const void* networkIdentifier, const Packet* pkt, uint8_t subClientId) = 0;
};

} // namespace SDK
