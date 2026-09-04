#pragma once
#include "PacketContext.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include <shared_mutex>

using PacketHandler = std::function<void(PacketContext&)>;

inline constexpr SDK::PacketID kAnyPacket = SDK::PacketID::NONE;

class Pipeline
{
public:
    using Handle = uint64_t;

    // Register packet callback, returns handle for unregistering
    Handle on(SDK::PacketID id, PacketHandler fn, const std::atomic<bool>* enabledGate = nullptr);

    template<typename T, typename Fn>
    Handle on(Fn&& fn, const std::atomic<bool>* enabledGate = nullptr)
    {
        return on(T::ID, [fn = std::forward<Fn>(fn)](PacketContext& rawCtx)
        {
            TypedPacketContext<T> typedCtx{
                rawCtx.dir,
                rawCtx.as<T>(),
                rawCtx.dropped,
                rawCtx.dropReason
            };
            fn(typedCtx);
            if (typedCtx.dropped)
            {
                rawCtx.drop(typedCtx.dropReason);
            }
        }, enabledGate);
    }

    void off(Handle h);
    void clear();
    bool process(PacketContext& ctx);

    static Pipeline& get()
    {
        static Pipeline inst;
        return inst;
    }

private:
    struct Entry
    {
        Handle handle;
        SDK::PacketID id;
        PacketHandler fn;
        const std::atomic<bool>* enabledGate = nullptr;
    };

    mutable std::shared_mutex m_mutex;
    std::array<std::vector<Entry>, 256> m_handlersById{};
    std::vector<Entry> m_anyEntries;
    std::array<std::atomic<uint32_t>, 256> m_handlerCount{};
    std::atomic<uint32_t> m_anyCount{0};
    Handle m_next = 1;
};
