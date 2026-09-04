#pragma once
#include "sdk/Packet.h"
#include <memory>

namespace SDK
{

// Wraps the game's internal packet factory function.
// The function address is resolved at runtime via SigScan (see Addresses.h).
//
// Returns a game-allocated Packet subclass for the given ID, or nullptr
// if the ID is unknown or the address has not been resolved yet.
namespace Factory
{
    std::shared_ptr<Packet> createPacket(PacketID id);
}

} // namespace SDK
