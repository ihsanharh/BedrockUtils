#pragma once
#include <cstdint>
#include <string_view>
#include <windows.h>

namespace SigScan
{

// scan memory range for ida style pattern (e.g. "48 89 5C 24 ??")
uintptr_t scan(uintptr_t base, size_t size, std::string_view pattern);

// scan module memory by name (e.g. "Minecraft.Windows.exe")
uintptr_t find(const char* moduleName, std::string_view pattern);

// calculate absolute address from rip relative offset
uintptr_t resolveRel32(uintptr_t instrAddr, int relOffset = 1, int instrSize = 5);

} // namespace SigScan
