#pragma once
#include "sdk/Packet.h"
#include <concepts>
#include <string>
#include <string_view>
#include <windows.h>

enum class PacketDirection
{
    Inbound,
    Outbound
};

inline SDK::PacketID safeGetPacketId(const SDK::Packet* p) noexcept
{
    if (!p)
    {
        return SDK::PacketID::NONE;
    }

    __try
    {
        void** vtable = *reinterpret_cast<void***>(const_cast<SDK::Packet*>(p));
        if (!vtable)
        {
            return SDK::PacketID::NONE;
        }

        return p->getID();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return SDK::PacketID::NONE;
    }
}

struct PacketContext
{
    PacketDirection dir = PacketDirection::Inbound;
    SDK::Packet* packet = nullptr;
    bool dropped = false;
    std::string dropReason{};

    // Direction queries
    [[nodiscard]] bool isInbound() const noexcept
    {
        return dir == PacketDirection::Inbound;
    }
    [[nodiscard]] bool isOutbound() const noexcept
    {
        return dir == PacketDirection::Outbound;
    }

    SDK::PacketID id() const noexcept
    {
        return safeGetPacketId(packet);
    }

    void drop(std::string_view reason = "") noexcept
    {
        dropped = true;
        if (!reason.empty())
        {
            dropReason = reason;
        }
    }

    // Type query with static assertion
    template<typename T>
    [[nodiscard]] bool is() const noexcept
    {
        static_assert(requires { T::ID; }, "T must define static constexpr PacketID ID");
        return id() == T::ID;
    }

    // Type-safe cast: returns pointer to T if ID matches, nullptr otherwise
    template<typename T>
    T* as() noexcept
    {
        static_assert(requires { T::ID; }, "T must define static constexpr PacketID ID");
        return (id() == T::ID) ? static_cast<T*>(packet) : nullptr;
    }

    template<typename T>
    const T* as() const noexcept
    {
        static_assert(requires { T::ID; }, "T must define static constexpr PacketID ID");
        return (id() == T::ID) ? static_cast<const T*>(packet) : nullptr;
    }
};

template<typename T>
struct TypedPacketContext
{
    PacketDirection dir = PacketDirection::Inbound;
    T* packet = nullptr;
    bool& dropped;
    std::string& dropReason;

    TypedPacketContext(PacketDirection d, T* p, bool& dr, std::string& reason)
        : dir(d), packet(p), dropped(dr), dropReason(reason) {}

    void drop(std::string_view reason = "") noexcept
    {
        dropped = true;
        if (!reason.empty())
        {
            dropReason = reason;
        }
    }

    T* operator->() noexcept
    {
        return packet;
    }

    const T* operator->() const noexcept
    {
        return packet;
    }
};
