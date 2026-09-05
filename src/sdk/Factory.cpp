#include "pch.h"
#include "Factory.h"
#include "Addresses.h"
#include "Logger.h"

namespace SDK
{
namespace Factory
{

using CreateFn = std::shared_ptr<Packet>(__fastcall*)(int32_t);

std::shared_ptr<Packet> createPacket(PacketID id)
{
    CreateFn fn = reinterpret_cast<CreateFn>(Addresses::g_createPacket);
    if (!fn)
    {
        SDK::Log::log("[Factory] createPacket: Addresses::g_createPacket is 0!");
        return nullptr;
    }

    int32_t numericId = static_cast<int32_t>(id);
    return fn(numericId);
}

} // namespace Factory
} // namespace SDK
