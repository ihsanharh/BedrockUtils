#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <windows.h>

namespace SDK::Memory
{

// Pure POD leaf function: MSVC /EHsc allows __try here with ZERO compiler warnings/errors
// because there are no local C++ objects requiring stack unwinding.
inline bool read(const void* src, void* dst, size_t size) noexcept
{
    if (!src || !dst || size == 0)
    {
        return false;
    }

    __try
    {
        std::memcpy(dst, src, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

template<typename T>
inline bool read(uintptr_t address, T& out) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for raw memory read");
    return read(reinterpret_cast<const void*>(address), &out, sizeof(T));
}

template<typename T>
inline T readValue(uintptr_t address, T defaultVal = {}) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for raw memory read");
    T out = defaultVal;
    read(address, out);
    return out;
}

inline bool isValidReadPtr(const void* ptr, size_t size = 1) noexcept
{
    if (!ptr || size == 0)
    {
        return false;
    }

    uint8_t dummy = 0;
    if (!read(ptr, &dummy, 1))
    {
        return false;
    }

    if (size > 1)
    {
        return read(reinterpret_cast<const uint8_t*>(ptr) + size - 1, &dummy, 1);
    }

    return true;
}

} // namespace SDK::Memory
