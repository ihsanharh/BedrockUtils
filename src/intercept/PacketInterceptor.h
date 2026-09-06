#pragma once
#include "sdk/Packet.h"
#include "sdk/PacketSender.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <vector>

class PacketInterceptor
{
public:
    using HandleFn = void (*)(void*, void*, void*, std::shared_ptr<SDK::Packet>&);
    using SendFn = void (*)(SDK::PacketSender*, SDK::Packet*);

    // Hook all packet dispatcher handle() functions via MinHook (called once at startup).
    bool install();

    // Hook LoopbackPacketSender::sendToServer via MinHook (called once).
    // Reads vtable[2] (sendToServer) from the provided sender instance.
    bool hookSender(SDK::PacketSender* sender);

    // Returns true after hookSender() has completed successfully.
    [[nodiscard]] bool isOutboundHooked() const noexcept
    {
        return m_outboundHooked.load(std::memory_order_relaxed);
    }

    // Disable and remove all MinHook hooks, then drain in-flight trap executions.
    void uninstall();

    // Inject an inbound packet into the queue (thread-safe).
    // Packet is dispatched on the live network thread when trapInbound runs.
    void injectInbound(std::shared_ptr<SDK::Packet> pkt);

    // Dispatch all queued inbound packets on the live network thread.
    void flushInbound(void* liveNetId = nullptr, void* liveCb = nullptr);
    void dispatchInboundDirect(std::shared_ptr<SDK::Packet> pkt, void* liveNetId = nullptr, void* liveCb = nullptr);

    static PacketInterceptor& get()
    {
        static PacketInterceptor inst;
        return inst;
    }

    [[nodiscard]] bool hasActiveSession() const noexcept
    {
        return m_lastNetId.load(std::memory_order_relaxed) != nullptr &&
               m_lastCb.load(std::memory_order_relaxed) != nullptr;
    }

    [[nodiscard]] bool hasClientHandler() const noexcept
    {
        return m_clientCb.load(std::memory_order_acquire) != nullptr &&
               m_clientVtable.load(std::memory_order_acquire) != nullptr;
    }

    // Unified DataItem vtable access
    [[nodiscard]] void* getDataItemVtable(uint8_t type) const noexcept
    {
        return type < 16 ? m_dataItemVtables[type] : nullptr;
    }

    void harvestDataItems(const void* synchedActorDataVec);

    [[nodiscard]] std::string getConnectedServer() const;
    [[nodiscard]] std::string getTransferHost() const;
    void setConnectedServer(std::string addr);
    void setTransferHost(std::string host);
    void resetSession();

private:
    void* m_dataItemVtables[16] = {};
    std::atomic<bool> m_dataItemsHarvested{false};

    static void trapInbound(void* self, void* netId, void* cb, std::shared_ptr<SDK::Packet>& pkt);
    static void trapSendToServer(SDK::PacketSender* sender, SDK::Packet* pkt);

    static constexpr std::size_t kMaxId = 512;

    // Counts trap functions currently executing — used to safely drain before uninstall.
    std::atomic<uint32_t> m_activeTraps{0};
    std::atomic<bool> m_uninstalled{false};

    // Inbound hooks: maps original handle() fn address → MinHook trampoline.
    // The trampoline calls the original function without going through our hook.
    mutable std::shared_mutex m_inboundMutex;
    std::unordered_map<void*, HandleFn> m_inboundTrampolines;
    std::array<HandleFn, kMaxId> m_byIdTrampolines{};

    // Outbound trampoline for sendToServer (vtable[2]) — set once by hookSender()
    static inline std::atomic<SendFn> s_origSendToServer{nullptr};
    std::atomic<bool> m_outboundHooked{false};

    // Stored dispatchers and session state for inbound injection.
    std::array<void*, kMaxId> m_dispatchers{};
    std::atomic<void*> m_lastNetId{nullptr};
    std::atomic<void*> m_lastCheckedNetId{nullptr};
    std::atomic<void*> m_lastCb{nullptr};
    std::atomic<void*> m_clientCb{nullptr};
    std::atomic<void*> m_clientVtable{nullptr};
    alignas(16) uint8_t m_savedNetIdBuffer[256]{};

    // Thread-safe queue for inbound packets injected from non-tick threads.
    std::mutex m_inboundQueueMutex;
    std::atomic<size_t> m_inboundQueueCount{0};
    std::vector<std::shared_ptr<SDK::Packet>> m_inboundQueue;

    mutable std::shared_mutex m_netInfoMutex;
    std::string m_connectedServer;
    std::string m_transferHost;
};
