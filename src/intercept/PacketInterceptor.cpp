#include "PacketInterceptor.h"
#include "pch.h"
#include "pipeline/PacketContext.h"
#include "pipeline/Pipeline.h"
#include "sdk/CrashHandler.h"
#include "sdk/Factory.h"
#include "sdk/Logger.h"
#include "sdk/SafeMem.h"
#include "sdk/packets/AddActorPacket.h"
#include "sdk/packets/AddPlayerPacket.h"
#include <MinHook.h>
#include <deque>
#include <mutex>
#include <vector>

static std::vector<std::shared_ptr<SDK::Packet>> s_startupPackets;
static std::deque<std::shared_ptr<SDK::Packet>>  s_heldInbound;
static std::mutex                                s_heldMutex;

using SDK::getPacketName;

static std::string formatNetId(void* netId)
{
    if (!netId)
    {
        return "";
    }

    uint8_t p[8]{};
    if (!SDK::Memory::read(netId, p, sizeof(p)))
    {
        return "";
    }

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

static void callTrampolineSafe(PacketInterceptor::HandleFn trampoline, void* self, void* netId, void* cb, std::shared_ptr<SDK::Packet>& pkt) noexcept
{
    if (!trampoline)
    {
        return;
    }

    __try
    {
        trampoline(self, netId, cb, pkt);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[PacketInterceptor] Exception in dispatcher trampoline!");
    }
}

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
        pktId = safeGetPacketId(pkt.get());
        if (pktId != SDK::PacketID::NONE)
        {
            SDK::Crash::g_lastInboundPacketId.store(static_cast<uint32_t>(pktId));
            SDK::Crash::g_lastCheckpoint.store("trapInbound: got packet ID");
        }
    }

    const std::size_t idx = static_cast<std::size_t>(
        static_cast<std::underlying_type_t<SDK::PacketID>>(pktId));

    // Fast O(1) trampoline lookup without mutex contention on hot packet path
    HandleFn trampoline = (idx < kMaxId) ? inter.m_byIdTrampolines[idx] : nullptr;
    if (!trampoline && self)
    {
        void** vtable = nullptr;
        if (SDK::Memory::read(self, &vtable, sizeof(vtable)) && vtable)
        {
            void* origAddr = nullptr;
            if (SDK::Memory::read(&vtable[1], &origAddr, sizeof(origAddr)) && origAddr)
            {
                std::shared_lock lock(inter.m_inboundMutex);
                std::unordered_map<void*, HandleFn>::const_iterator it = inter.m_inboundTrampolines.find(origAddr);
                if (it != inter.m_inboundTrampolines.end())
                {
                    trampoline = it->second;
                }
            }
        }
    }

    if (inter.m_uninstalled.load(std::memory_order_acquire))
    {
        callTrampolineSafe(trampoline, self, netId, cb, pkt);
        return;
    }

    if (!pkt)
    {
        callTrampolineSafe(trampoline, self, netId, cb, pkt);
        return;
    }

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
        void* vt = nullptr;
        if (SDK::Memory::read(cb, &vt, sizeof(vt)) && vt)
        {
            inter.m_clientVtable.store(vt, std::memory_order_release);
            inter.m_clientCb.store(cb, std::memory_order_release);
        }
    }

    if (cb && pktId == SDK::PacketID::TEXT)
    {
        void* vt = nullptr;
        if (SDK::Memory::read(cb, &vt, sizeof(vt)) && vt)
        {
            void* clientVt = inter.m_clientVtable.load(std::memory_order_acquire);
            if (!clientVt || vt == clientVt)
            {
                inter.m_clientCb.store(cb, std::memory_order_release);
            }
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
        if (SDK::Memory::read(netId, inter.m_savedNetIdBuffer, sizeof(inter.m_savedNetIdBuffer)))
        {
            inter.m_lastNetId.store(inter.m_savedNetIdBuffer, std::memory_order_release);
        }
        else
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
    PacketContext ctx{PacketDirection::Inbound, pkt.get()};
    SDK::Crash::g_lastCheckpoint.store("trapInbound: Pipeline::process");
    Pipeline::get().process(ctx);
    bool shouldDrop = ctx.dropped;

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
        callTrampolineSafe(trampoline, self, netId, cb, pkt);
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

    SDK::PacketID pktId = safeGetPacketId(pkt);
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

    t_isProcessing = false;
    return shouldDrop;
}

void PacketInterceptor::trapSendToServer(SDK::PacketSender* sender, SDK::Packet* pkt)
{
    PacketInterceptor& inter = PacketInterceptor::get();
    TrapGuard guard(inter.m_activeTraps);

    SendFn orig = s_origSendToServer.load(std::memory_order_acquire);
    if (inter.m_uninstalled.load(std::memory_order_acquire))
    {
        if (sender && pkt && orig)
        {
            orig(sender, pkt);
        }
        return;
    }

    if (!sender || !pkt || processOutbound(pkt))
    {
        return;
    }

    SDK::Crash::g_lastCheckpoint.store("trapSendToServer: calling s_origSendToServer");
    orig = s_origSendToServer.load(std::memory_order_acquire);
    if (orig)
    {
        orig(sender, pkt);
    }
    SDK::Crash::g_lastCheckpoint.store("trapSendToServer: complete");
}

// ---------------------------------------------------------------------------
// install() — hook all packet dispatcher handle() functions
// ---------------------------------------------------------------------------

bool PacketInterceptor::install()
{
    m_uninstalled.store(false, std::memory_order_seq_cst);
    m_outboundHooked.store(false, std::memory_order_release);
    s_origSendToServer.store(nullptr, std::memory_order_release);

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
    if (!sender || m_outboundHooked.load(std::memory_order_acquire))
    {
        return m_outboundHooked.load(std::memory_order_relaxed);
    }

    uintptr_t* vtable = nullptr;
    if (!SDK::Memory::read(sender, &vtable, sizeof(vtable)) || !vtable)
    {
        SDK::Log::log("[PacketInterceptor] hookSender: bad vtable pointer");
        return false;
    }

    // In LoopbackPacketSender vtable:
    // [0] ~PacketSender()
    // [1] send(Packet*) - short thunk (calls sendToServer)
    // [2] sendToServer(Packet*) - target function for all outbound packets
    void* sendFn = nullptr;
    void* sendToSrvFn = nullptr;
    SDK::Memory::read(&vtable[1], &sendFn, sizeof(sendFn));
    SDK::Memory::read(&vtable[2], &sendToSrvFn, sizeof(sendToSrvFn));

    SDK::Log::log("[PacketInterceptor] PacketSender vtable: send @ {:#x}, sendToServer @ {:#x}",
        reinterpret_cast<uintptr_t>(sendFn), reinterpret_cast<uintptr_t>(sendToSrvFn));

    if (!sendToSrvFn)
    {
        SDK::Log::log("[PacketInterceptor] hookSender: null vtable slot [2]");
        return false;
    }

    void* origTrampoline = nullptr;
    MH_STATUS s = MH_CreateHook(
        sendToSrvFn,
        reinterpret_cast<void*>(&trapSendToServer),
        &origTrampoline
    );

    if (s == MH_OK)
    {
        s_origSendToServer.store(reinterpret_cast<SendFn>(origTrampoline), std::memory_order_release);
        MH_EnableHook(sendToSrvFn);
        SDK::Log::log("[PacketInterceptor] Hooked sendToServer @ {:#x}", reinterpret_cast<uintptr_t>(sendToSrvFn));
        m_outboundHooked.store(true, std::memory_order_release);
        return true;
    }
    else if (s == MH_ERROR_ALREADY_CREATED)
    {
        m_outboundHooked.store(true, std::memory_order_release);
        return true;
    }
    else
    {
        SDK::Log::log("[PacketInterceptor] hookSender: MH_CreateHook failed with status={}", static_cast<int>(s));
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

    for (int i = 0; i < 50 && m_activeTraps.load(std::memory_order_acquire) > 0; ++i)
    {
        ::Sleep(10);
    }
    ::Sleep(50); // Margin for CPU instruction pipelines and returning frames

    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    m_lastNetId.store(nullptr, std::memory_order_relaxed);
    m_lastCb.store(nullptr, std::memory_order_relaxed);
    m_clientCb.store(nullptr, std::memory_order_relaxed);
    m_clientVtable.store(nullptr, std::memory_order_relaxed);
    m_outboundHooked.store(false, std::memory_order_release);
    s_origSendToServer.store(nullptr, std::memory_order_release);

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

    PacketInterceptor& inter = PacketInterceptor::get();
    SDK::PacketID pktId = safeGetPacketId(pkt.get());
    const std::size_t idx = static_cast<std::size_t>(
        static_cast<std::underlying_type_t<SDK::PacketID>>(pktId));

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
    void* liveVt = nullptr;
    if (liveCb && SDK::Memory::read(liveCb, &liveVt, sizeof(liveVt)) && liveVt == clientVt)
    {
        cb = liveCb;
    }
    else
    {
        void* storedVt = nullptr;
        if (SDK::Memory::read(clientCb, &storedVt, sizeof(storedVt)) && storedVt == clientVt)
        {
            cb = clientCb;
        }
        else
        {
            return;
        }
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
        callTrampolineSafe(trampoline, dispatcher, netId, cb, pkt);
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

    uintptr_t vec[2]{};
    if (!SDK::Memory::read(synchedActorDataVec, vec, sizeof(vec)))
    {
        return;
    }

    if (!vec[0] || !vec[1] || vec[1] <= vec[0])
    {
        return;
    }

    size_t bytes = vec[1] - vec[0];
    if (bytes > 8192)
    {
        return;
    }

    size_t count = bytes / 8;
    for (size_t i = 0; i < count; ++i)
    {
        uintptr_t elemPtr = 0;
        if (!SDK::Memory::read(reinterpret_cast<const void*>(vec[0] + i * sizeof(uintptr_t)), &elemPtr, sizeof(elemPtr)))
        {
            break;
        }

        if (elemPtr > 0x10000)
        {
            SDK::DataItem itemHeader{};
            if (SDK::Memory::read(reinterpret_cast<const void*>(elemPtr), &itemHeader, sizeof(itemHeader)))
            {
                if (itemHeader.type < 16 && !m_dataItemVtables[itemHeader.type])
                {
                    m_dataItemVtables[itemHeader.type] = itemHeader.vtable;
                    SDK::Log::log("[PacketInterceptor] Harvested DataItem type {} vtable = {:#x}",
                        itemHeader.type, reinterpret_cast<uintptr_t>(itemHeader.vtable));
                }
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
