#pragma once
#include "intercept/SigScan.h"
#include "sdk/Logger.h"
#include <cstdint>
#include <format>

namespace Addresses
{

inline uintptr_t createPacket = 0;
inline uintptr_t platformGameCore = 0;

inline bool init()
{
    // signatures referenced from Latite
    createPacket = SigScan::find("Minecraft.Windows.exe", "56 48 83 EC ? 48 89 CE 81 FA");
    if (createPacket)
    {
        SDK::Log::log("[Scan] createPacket      @ {:#x}", createPacket);
    }
    else
    {
        SDK::Log::log("[Scan] createPacket      NOT FOUND");
    }

    if (uintptr_t raw = SigScan::find("Minecraft.Windows.exe", "4C 89 3D ? ? ? ? 4D 85 FF"))
    {
        platformGameCore = SigScan::resolveRel32(raw, 3, 7);
        SDK::Log::log("[Scan] platformGameCore  @ {:#x}", platformGameCore);
    }
    else
    {
        SDK::Log::log("[Scan] platformGameCore  NOT FOUND");
    }

    return createPacket != 0 && platformGameCore != 0;
}

} // namespace Addresses
