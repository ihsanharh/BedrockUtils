#pragma once
#include "intercept/SigScan.h"
#include "sdk/Logger.h"
#include <cstdint>
#include <format>

namespace Addresses
{

inline uintptr_t g_createPacket = 0;
inline uintptr_t g_platformGameCore = 0;

inline bool init()
{
    // signatures referenced from Latite
    g_createPacket = SigScan::find("Minecraft.Windows.exe", "56 48 83 EC ? 48 89 CE 81 FA");
    if (g_createPacket)
    {
        SDK::Log::log("[Scan] g_createPacket      @ {:#x}", g_createPacket);
    }
    else
    {
        SDK::Log::log("[Scan] g_createPacket      NOT FOUND");
    }

    if (uintptr_t raw = SigScan::find("Minecraft.Windows.exe", "4C 89 3D ? ? ? ? 4D 85 FF"))
    {
        g_platformGameCore = SigScan::resolveRel32(raw, 3, 7);
        SDK::Log::log("[Scan] g_platformGameCore  @ {:#x}", g_platformGameCore);
    }
    else
    {
        SDK::Log::log("[Scan] g_platformGameCore  NOT FOUND");
    }

    return g_createPacket != 0 && g_platformGameCore != 0;
}

} // namespace Addresses
