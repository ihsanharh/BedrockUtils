#include "PacketInterceptor.h"
#include "pch.h"
#include "pipeline/PacketContext.h"
#include "pipeline/Pipeline.h"
#include "sdk/CrashHandler.h"
#include "sdk/Factory.h"
#include "sdk/Logger.h"
#include "sdk/packets/AddActorPacket.h"
#include "sdk/packets/AddPlayerPacket.h"
#include <MinHook.h>
#include <deque>
#include <mutex>
#include <vector>

static std::vector<std::shared_ptr<SDK::Packet>> s_startupPackets;
static std::deque<std::shared_ptr<SDK::Packet>>  s_heldInbound;
static std::mutex                                s_heldMutex;

// Helper to get packet name from ID
static const char* getPacketName(SDK::PacketID id)
{
    switch (id)
    {
    case SDK::PacketID::NONE:                  return "NONE";
    case SDK::PacketID::PLAY_STATUS:           return "PLAY_STATUS";
    case SDK::PacketID::DISCONNECT:            return "DISCONNECT";
    case SDK::PacketID::TEXT:                  return "TEXT";
    case SDK::PacketID::ADD_PLAYER:            return "ADD_PLAYER";
    case SDK::PacketID::ADD_ACTOR:             return "ADD_ACTOR";
    case SDK::PacketID::REMOVE_ACTOR:          return "REMOVE_ACTOR";
    case SDK::PacketID::MOVE_ACTOR_ABSOLUTE:   return "MOVE_ACTOR_ABSOLUTE";
    case SDK::PacketID::MOVE_PLAYER:           return "MOVE_PLAYER";
    case SDK::PacketID::CHANGE_DIMENSION:      return "CHANGE_DIMENSION";
    case SDK::PacketID::PLAYER_LIST:           return "PLAYER_LIST";
    case SDK::PacketID::AVAILABLE_COMMANDS:    return "AVAILABLE_COMMANDS";
    case SDK::PacketID::COMMAND_REQUEST:       return "COMMAND_REQUEST";
    case SDK::PacketID::COMMAND_OUTPUT:        return "COMMAND_OUTPUT";
    case SDK::PacketID::TRANSFER:              return "TRANSFER";
    case SDK::PacketID::MODAL_FORM_REQUEST:    return "MODAL_FORM_REQUEST";
    case SDK::PacketID::MODAL_FORM_RESPONSE:   return "MODAL_FORM_RESPONSE";
    case SDK::PacketID::MOVE_ACTOR_DELTA:      return "MOVE_ACTOR_DELTA";
    default:                                   return "UNKNOWN";
    }
}

static std::string formatNetId(void* netId)
{
    if (!netId)
    {
        return "";
    }

    __try
    {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(netId);
        uint16_t family = *reinterpret_cast<const uint16_t*>(p);
        if (family == 2) // AF_INET
        {
            uint16_t port = (static_cast<uint16_t>(p[2]) << 8) | static_cast<uint16_t>(p[3]);
            return std::format("{}.{}.{}.{}:{}", p[4], p[5], p[6], p[7], port);
        }
        else if (family == 23) // AF_INET6
        {
            uint16_t port = (static_cast<uint16_t>(p[2]) << 8) | static_cast<uint16_t>(p[3]);
            return std::format("[IPv6]:{}", port);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return "";
}

// RAII counter: increment on entry, decrement on exit.
struct TrapGuard
{
    std::atomic<uint32_t>& m_counter;

    explicit TrapGuard(std::atomic<uint32_t>& c)
        : m_counter(c)
    {
        m_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    ~TrapGuard()
    {
        m_counter.fetch_sub(1, std::memory_order_acq_rel);
    }
};

// ---------------------------------------------------------------------------
// Inbound trap
// ---------------------------------------------------------------------------

void PacketInterceptor::trapInbound(void* self, void* netId, void* cb, std::shared_ptr<SDK::Packet>& pkt)
{
    PacketInterceptor& inter = PacketInterceptor::get();
    TrapGuard guard(inter.m_activeTraps);

    SDK::PacketID pktId = SDK::PacketID::NONE;
    if (pkt)
    {
        __try
        {
            pktId = pkt->getID();
            SDK::Crash::g_lastInboundPacketId.store(static_cast<uint32_t>(pktId));
            SDK::Crash::g_lastCheckpoint.store("trapInbound: got packet ID");
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            SDK::Log::log("[Inbound] Exception reading pkt->getID()!");
        }
    }

    const std::size_t idx = static_cast<std::size_t>(
        static_cast<std::underlying_type_t<SDK::PacketID>>(pktId));

    // Fast O(1) trampoline lookup without mutex contention on hot packet path
    HandleFn trampoline = (idx < kMaxId) ? inter.m_byIdTrampolines[idx] : nullptr;
    if (!trampoline)
    {
        void** vtable   = *reinterpret_cast<void***>(self);
        void*  origAddr = vtable ? vtable[1] : nullptr;
        if (origAddr)
        {
            std::shared_lock lock(inter.m_inboundMutex);
            std::unordered_map<void*, HandleFn>::const_iterator it = inter.m_inboundTrampolines.find(origAddr);
            if (it != inter.m_inboundTrampolines.end())
            {
                trampoline = it->second;
            }
        }
    }

    if (inter.m_uninstalled.load(std::memory_order_acquire))
    {
        if (trampoline)
        {
            trampoline(self, netId, cb, pkt);
        }
        return;
    }

    if (!pkt)
    {
        if (trampoline)
        {
            trampoline(self, netId, cb, pkt);
        }
        return;
    }

    bool shouldDrop = false;
    PacketContext ctx{PacketDirection::Inbound, pkt.get()};

    __try
    {
        inter.m_lastCb.store(cb, std::memory_order_relaxed);

        // S2C packets that are ONLY ever received by ClientNetworkHandler
        const bool isS2CClientOnly = (
            pktId == SDK::PacketID::ADD_ACTOR ||
            pktId == SDK::PacketID::ADD_PLAYER ||
            pktId == SDK::PacketID::REMOVE_ACTOR ||
            pktId == SDK::PacketID::MOVE_ACTOR_DELTA ||
            pktId == SDK::PacketID::MOVE_ACTOR_ABSOLUTE ||
            pktId == SDK::PacketID::PLAYER_LIST ||
            pktId == SDK::PacketID::PLAY_STATUS ||
            pktId == SDK::PacketID::CHANGE_DIMENSION ||
            pktId == SDK::PacketID::AVAILABLE_COMMANDS
        );

        if (cb && isS2CClientOnly)
        {
            void* vt = *reinterpret_cast<void**>(cb);
            inter.m_clientVtable.store(vt, std::memory_order_release);
            inter.m_clientCb.store(cb, std::memory_order_release);
        }

        if (cb && pktId == SDK::PacketID::TEXT)
        {
            void* vt = *reinterpret_cast<void**>(cb);
            void* clientVt = inter.m_clientVtable.load(std::memory_order_acquire);
            if (!clientVt || vt == clientVt)
            {
                inter.m_clientCb.store(cb, std::memory_order_release);
            }
        }

        if (idx < kMaxId && self)
        {
            inter.m_dispatchers[idx] = self;
        }

        // Network connection state tracking and logging (cached by pointer to avoid overhead on every packet)
        if (netId && netId != inter.m_lastCheckedNetId.load(std::memory_order_relaxed))
        {
            inter.m_lastCheckedNetId.store(netId, std::memory_order_relaxed);
            __try
            {
                std::memcpy(inter.m_savedNetIdBuffer, netId, 160);
                inter.m_lastNetId.store(inter.m_savedNetIdBuffer, std::memory_order_release);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                inter.m_lastNetId.store(netId, std::memory_order_relaxed);
            }

            std::string curAddr = formatNetId(netId);
            if (!curAddr.empty() && curAddr != inter.getConnectedServer())
            {
                {
                    std::unique_lock lock(inter.m_netInfoMutex);
                    inter.m_connectedServer = curAddr;
                    inter.m_transferHost.clear();
                }
                SDK::Log::log("[Network] Connected to server: {}", curAddr);
            }
        }

        if (pktId == SDK::PacketID::DISCONNECT)
        {
            std::string oldAddr = inter.getConnectedServer();
            SDK::Log::log("[Network] Disconnected from server: {}", oldAddr.empty() ? "Server" : oldAddr);
            inter.resetSession();
        }
        else if (pktId == SDK::PacketID::TRANSFER)
        {
            std::string host;
            const uint8_t* base = reinterpret_cast<const uint8_t*>(pkt.get());
            for (size_t off = 0x30; off <= 0x50; off += 8)
            {
                if (SDK::safeReadString(base + off, host) && !host.empty())
                {
                    break;
                }
            }
            inter.setTransferHost(host);
            SDK::Log::log("[Network] Server Transferring to: {}", host.empty() ? "new host" : host);
        }

        // Flush any queued injected packets on this live network thread
        if (netId && cb)
        {
            inter.flushInbound(netId, cb);
        }

        // Harvest DataItem vtables from entity spawn packets (once only).
        if (!inter.m_dataItemsHarvested.load(std::memory_order_relaxed))
        {
            if (pktId == SDK::PacketID::ADD_ACTOR)
            {
                SDK::Crash::g_lastCheckpoint.store("trapInbound: harvestDataItems ADD_ACTOR");
                SDK::AddActorPacket* addActor = static_cast<SDK::AddActorPacket*>(pkt.get());
                if (addActor)
                {
                    inter.harvestDataItems(&addActor->entityData.items);
                }
            }
            else if (pktId == SDK::PacketID::ADD_PLAYER)
            {
                SDK::Crash::g_lastCheckpoint.store("trapInbound: harvestDataItems ADD_PLAYER");
                SDK::AddPlayerPacket* addPlayer = static_cast<SDK::AddPlayerPacket*>(pkt.get());
                if (addPlayer)
                {
                    inter.harvestDataItems(&addPlayer->entityData.items);
                }
            }
        }

        // Feed into the pipeline.
        SDK::Crash::g_lastCheckpoint.store("trapInbound: Pipeline::process");
        Pipeline::get().process(ctx);
        shouldDrop = ctx.dropped;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[Inbound] Exception during packet processing for ID=0x{:02x}!", static_cast<uint8_t>(pktId));
    }

    if (shouldDrop)
    {
        if (!ctx.dropReason.empty())
        {
            SDK::Log::log("[Inbound] [0x{:02x}] {:20s} DROPPED ({})", static_cast<uint8_t>(pktId), getPacketName(pktId), ctx.dropReason);
        }
        else
        {
            SDK::Log::log("[Inbound] [0x{:02x}] {:20s} DROPPED", static_cast<uint8_t>(pktId), getPacketName(pktId));
        }
        return;
    }

    // Always call through to the original dispatcher.
    if (trampoline)
    {
        SDK::Crash::g_lastCheckpoint.store("trapInbound: calling trampoline");
        __try
        {
            trampoline(self, netId, cb, pkt);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            SDK::Log::log("[Inbound] Exception in dispatcher trampoline for ID=0x{:02x}!", static_cast<uint8_t>(pktId));
        }
        SDK::Crash::g_lastCheckpoint.store("trapInbound: trampoline complete");
    }
}

// ---------------------------------------------------------------------------
// Outbound trap (sendToServer vtable[2])
// ---------------------------------------------------------------------------

static bool processOutbound(SDK::Packet* pkt)
{
    static thread_local bool t_isProcessing = false;
    if (t_isProcessing || !pkt)
    {
        return false;
    }

    t_isProcessing = true;
    bool shouldDrop = false;
    PacketContext ctx{PacketDirection::Outbound, pkt};

    __try
    {
        SDK::PacketID pktId = pkt->getID();
        SDK::Crash::g_lastOutboundPacketId.store(static_cast<uint32_t>(pktId));
        SDK::Crash::g_lastCheckpoint.store("processOutbound: Pipeline::process");

        uint8_t rawId = static_cast<uint8_t>(pktId);
        Pipeline::get().process(ctx);
        shouldDrop = ctx.dropped;
        if (shouldDrop)
        {
            if (!ctx.dropReason.empty())
            {
                SDK::Log::log("[Outbound] [0x{:02x}] {:20s} DROPPED ({})", rawId, getPacketName(pktId), ctx.dropReason);
            }
            else
            {
                SDK::Log::log("[Outbound] [0x{:02x}] {:20s} DROPPED", rawId, getPacketName(pktId));
            }
        }
        else if (pktId == SDK::PacketID::COMMAND_REQUEST || pktId == SDK::PacketID::TEXT)
        {
            SDK::Log::log("[Outbound] [0x{:02x}] {:20s} PASSED to server", rawId, getPacketName(pktId));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[Outbound] Exception in processOutbound!");
    }

    t_isProcessing = false;
    return shouldDrop;
}

void PacketInterceptor::trapSendToServer(SDK::PacketSender* sender, SDK::Packet* pkt)
{
    PacketInterceptor& inter = PacketInterceptor::get();
    if (inter.m_uninstalled.load(std::memory_order_acquire))
    {
        if (sender && pkt && s_origSendToServer)
        {
            s_origSendToServer(sender, pkt);
        }
        return;
    }

    if (!sender || !pkt || processOutbound(pkt))
    {
        return;
    }

    TrapGuard guard(inter.m_activeTraps);
    SDK::Crash::g_lastCheckpoint.store("trapSendToServer: calling s_origSendToServer");
    __try
    {
        if (s_origSendToServer)
        {
            s_origSendToServer(sender, pkt);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[Outbound] Exception in s_origSendToServer!");
    }
    SDK::Crash::g_lastCheckpoint.store("trapSendToServer: complete");
}

// ---------------------------------------------------------------------------
// install() — hook all packet dispatcher handle() functions
// ---------------------------------------------------------------------------

bool PacketInterceptor::install()
{
    m_uninstalled.store(false, std::memory_order_seq_cst);
    m_outboundHooked = false;
    s_origSendToServer = nullptr;

    constexpr uint32_t kScanPackets = 200;

    MH_Initialize();

    SDK::Log::log("[PacketInterceptor] Installing inbound hooks via MinHook (scan count={})...", kScanPackets);

    std::unique_lock<std::shared_mutex> lk(m_inboundMutex);

    s_startupPackets.clear();
    s_startupPackets.reserve(kScanPackets);

    uint32_t hookedCount = 0;
    for (uint32_t i = 1; i < kScanPackets; ++i)
    {
        std::shared_ptr<SDK::Packet> pkt = SDK::Factory::createPacket(static_cast<SDK::PacketID>(i));
        if (!pkt || !pkt->handler)
        {
            continue;
        }
        if (i < kMaxId)
        {
            m_dispatchers[i] = pkt->handler;
        }
        s_startupPackets.push_back(pkt);

        void** vtable = *reinterpret_cast<void***>(pkt->handler);
        if (!vtable)
        {
            continue;
        }

        void* origAddr = vtable[1];
        if (!origAddr)
        {
            continue;
        }

        // Only create one hook per unique handle() implementation.
        if (m_inboundTrampolines.find(origAddr) == m_inboundTrampolines.end())
        {
            HandleFn trampoline = nullptr;
            MH_STATUS status = MH_CreateHook(
                origAddr,
                reinterpret_cast<void*>(&trapInbound),
                reinterpret_cast<void**>(&trampoline)
            );

            if (status != MH_OK)
            {
                SDK::Log::log("[PacketInterceptor] MH_CreateHook failed for ID={} status={}", i, static_cast<int>(status));
                continue;
            }

            m_inboundTrampolines[origAddr] = trampoline;
            ++hookedCount;
        }

        if (i < kMaxId)
        {
            m_byIdTrampolines[i] = m_inboundTrampolines[origAddr];
        }
    }

    MH_EnableHook(MH_ALL_HOOKS);
    SDK::Log::log("[PacketInterceptor] All inbound hooks enabled ({} unique handlers hooked).", hookedCount);

    return true;
}

// ---------------------------------------------------------------------------
// hookSender() — hook outbound sendToServer (vtable[2]) once
// ---------------------------------------------------------------------------

bool PacketInterceptor::hookSender(SDK::PacketSender* sender)
{
    if (!sender || m_outboundHooked)
    {
        return m_outboundHooked;
    }

    __try
    {
        uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(sender);
        if (!vtable)
        {
            SDK::Log::log("[PacketInterceptor] hookSender: bad vtable pointer");
            return false;
        }

        // In LoopbackPacketSender vtable:
        // [0] ~PacketSender()
        // [1] send(Packet*) - short thunk (calls sendToServer)
        // [2] sendToServer(Packet*) - target function for all outbound packets
        void* sendFn = reinterpret_cast<void*>(vtable[1]);
        void* sendToSrvFn = reinterpret_cast<void*>(vtable[2]);
        SDK::Log::log("[PacketInterceptor] PacketSender vtable: send @ {:#x}, sendToServer @ {:#x}",
            reinterpret_cast<uintptr_t>(sendFn), reinterpret_cast<uintptr_t>(sendToSrvFn));

        if (!sendToSrvFn)
        {
            SDK::Log::log("[PacketInterceptor] hookSender: null vtable slot [2]");
            return false;
        }

        MH_STATUS s = MH_CreateHook(
            sendToSrvFn,
            reinterpret_cast<void*>(&trapSendToServer),
            reinterpret_cast<void**>(&s_origSendToServer)
        );

        if (s == MH_OK)
        {
            MH_EnableHook(sendToSrvFn);
            SDK::Log::log("[PacketInterceptor] Hooked sendToServer @ {:#x}", reinterpret_cast<uintptr_t>(sendToSrvFn));
            m_outboundHooked = true;
            return true;
        }
        else if (s == MH_ERROR_ALREADY_CREATED)
        {
            m_outboundHooked = true;
            return true;
        }
        else
        {
            SDK::Log::log("[PacketInterceptor] hookSender: MH_CreateHook failed with status={}", static_cast<int>(s));
            return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[PacketInterceptor] hookSender: exception in hookSender!");
        return false;
    }
}

// ---------------------------------------------------------------------------
// uninstall() — disable/remove all hooks and drain in-flight executions
// ---------------------------------------------------------------------------

void PacketInterceptor::uninstall()
{
    m_uninstalled.store(true, std::memory_order_seq_cst);
    MH_DisableHook(MH_ALL_HOOKS);

    for (int i = 0; i < 20 && m_activeTraps.load(std::memory_order_acquire) > 0; ++i)
    {
        ::Sleep(10);
    }
    ::Sleep(20); // Margin for CPU instruction pipelines and returning frames

    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    m_lastNetId.store(nullptr, std::memory_order_relaxed);
    m_lastCb.store(nullptr, std::memory_order_relaxed);
    m_clientCb.store(nullptr, std::memory_order_relaxed);
    m_clientVtable.store(nullptr, std::memory_order_relaxed);
    m_outboundHooked = false;
    s_origSendToServer = nullptr;

    {
        std::lock_guard<std::mutex> lk(s_heldMutex);
        s_heldInbound.clear();
    }
    s_startupPackets.clear();

    {
        std::unique_lock<std::shared_mutex> lk(m_inboundMutex);
        m_inboundTrampolines.clear();
        m_byIdTrampolines.fill(nullptr);
        m_dispatchers.fill(nullptr);
    }

    SDK::Log::log("[PacketInterceptor] uninstall: all hooks removed");
}

// ---------------------------------------------------------------------------
// Inbound injection queue
// ---------------------------------------------------------------------------

void PacketInterceptor::injectInbound(std::shared_ptr<SDK::Packet> pkt)
{
    if (!pkt)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lk(m_inboundQueueMutex);
        m_inboundQueue.push_back(std::move(pkt));
    }
    m_inboundQueueCount.fetch_add(1, std::memory_order_release);
}

void PacketInterceptor::flushInbound(void* liveNetId, void* liveCb)
{
    if (m_inboundQueueCount.load(std::memory_order_acquire) == 0)
    {
        return;
    }

    static thread_local bool t_isFlushing = false;
    if (t_isFlushing)
    {
        return;
    }
    t_isFlushing = true;

    std::vector<std::shared_ptr<SDK::Packet>> toFlush;
    {
        std::lock_guard<std::mutex> lk(m_inboundQueueMutex);
        if (!m_inboundQueue.empty())
        {
            toFlush.swap(m_inboundQueue);
            m_inboundQueueCount.store(0, std::memory_order_release);
        }
    }

    for (std::shared_ptr<SDK::Packet>& pkt : toFlush)
    {
        dispatchInboundDirect(std::move(pkt), liveNetId, liveCb);
    }

    t_isFlushing = false;
}

void PacketInterceptor::dispatchInboundDirect(std::shared_ptr<SDK::Packet> pkt, void* liveNetId, void* liveCb)
{
    if (!pkt)
    {
        return;
    }

    __try
    {
        PacketInterceptor& inter = PacketInterceptor::get();
        const std::size_t idx = static_cast<std::size_t>(
            static_cast<std::underlying_type_t<SDK::PacketID>>(pkt->getID()));

        void* dispatcher = (idx < kMaxId) ? inter.m_dispatchers[idx] : nullptr;
        if (!dispatcher && pkt->handler)
        {
            dispatcher = pkt->handler;
        }
        if (!dispatcher)
        {
            SDK::Log::log("[Inbound Direct] Failed: no dispatcher for ID=0x{:02x}", idx);
            return;
        }

        void* clientCb = inter.m_clientCb.load(std::memory_order_acquire);
        void* clientVt = inter.m_clientVtable.load(std::memory_order_acquire);
        if (!clientCb || !clientVt)
        {
            return;
        }

        void* cb = nullptr;
        if (liveCb && *reinterpret_cast<void**>(liveCb) == clientVt)
        {
            cb = liveCb;
        }
        else if (*reinterpret_cast<void**>(clientCb) == clientVt)
        {
            cb = clientCb;
        }
        else
        {
            return;
        }

        void* netId = liveNetId ? liveNetId : inter.m_lastNetId.load(std::memory_order_relaxed);
        if (!netId)
        {
            return;
        }

        // Keep packet alive in a ring buffer so Minecraft's UI tasks don't experience a use-after-free
        {
            std::lock_guard<std::mutex> lk(s_heldMutex);
            s_heldInbound.push_back(pkt);
            if (s_heldInbound.size() > 64)
            {
                s_heldInbound.pop_front();
            }
        }

        HandleFn trampoline = (idx < kMaxId) ? inter.m_byIdTrampolines[idx] : nullptr;

        if (trampoline)
        {
            __try
            {
                trampoline(dispatcher, netId, cb, pkt);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                SDK::Log::log("[Inbound Direct] Exception in trampoline for packet 0x{:02x}!", idx);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[Inbound Direct] Exception in dispatchInboundDirect!");
    }
}

// ---------------------------------------------------------------------------
// DataItem vtable harvesting
// ---------------------------------------------------------------------------

void PacketInterceptor::harvestDataItems(const void* synchedActorDataVec)
{
    if (m_dataItemsHarvested.load(std::memory_order_relaxed) || !synchedActorDataVec)
    {
        return;
    }

    __try
    {
        const uintptr_t* vec = reinterpret_cast<const uintptr_t*>(synchedActorDataVec);
        if (!vec[0] || !vec[1] || vec[1] <= vec[0])
        {
            return;
        }

        const uintptr_t* elements = reinterpret_cast<const uintptr_t*>(vec[0]);
        size_t bytes = vec[1] - vec[0];
        if (bytes > 8192)
        {
            return;
        }

        size_t count = bytes / 8;
        for (size_t i = 0; i < count; ++i)
        {
            uint8_t* obj = reinterpret_cast<uint8_t*>(elements[i]);
            if (obj > reinterpret_cast<uint8_t*>(0x10000))
            {
                SDK::DataItem* item = reinterpret_cast<SDK::DataItem*>(obj);
                if (item->type < 16 && !m_dataItemVtables[item->type])
                {
                    m_dataItemVtables[item->type] = item->vtable;
                    SDK::Log::log("[PacketInterceptor] Harvested DataItem type {} vtable = {:#x}",
                        item->type, reinterpret_cast<uintptr_t>(item->vtable));
                }
            }
        }

        uint32_t harvestedCount = 0;
        for (size_t i = 0; i < 16; ++i)
        {
            if (m_dataItemVtables[i])
            {
                ++harvestedCount;
            }
        }
        if (harvestedCount >= 6)
        {
            m_dataItemsHarvested.store(true, std::memory_order_release);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

std::string PacketInterceptor::getConnectedServer() const
{
    std::shared_lock lock(m_netInfoMutex);
    return m_connectedServer;
}

std::string PacketInterceptor::getTransferHost() const
{
    std::shared_lock lock(m_netInfoMutex);
    return m_transferHost;
}

void PacketInterceptor::setConnectedServer(std::string addr)
{
    std::unique_lock lock(m_netInfoMutex);
    m_connectedServer = std::move(addr);
}

void PacketInterceptor::setTransferHost(std::string host)
{
    std::unique_lock lock(m_netInfoMutex);
    m_transferHost = std::move(host);
}

void PacketInterceptor::resetSession()
{
    m_lastNetId.store(nullptr, std::memory_order_relaxed);
    m_lastCheckedNetId.store(nullptr, std::memory_order_relaxed);
    m_lastCb.store(nullptr, std::memory_order_relaxed);
    m_clientCb.store(nullptr, std::memory_order_relaxed);
    m_clientVtable.store(nullptr, std::memory_order_relaxed);
    m_inboundQueueCount.store(0, std::memory_order_relaxed);
    std::memset(m_savedNetIdBuffer, 0, sizeof(m_savedNetIdBuffer));
    {
        std::unique_lock lock(m_netInfoMutex);
        m_connectedServer.clear();
        m_transferHost.clear();
    }
}
