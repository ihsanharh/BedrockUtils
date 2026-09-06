#include "Pipeline.h"
#include "pch.h"
#include "sdk/Logger.h"
#include "services/PacketDumper.h"
#include <algorithm>

static bool invokeHandlerSafe(const PacketHandler& fn, PacketContext& ctx)
{
    __try
    {
        fn(ctx);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        SDK::Log::log("[Pipeline] SEH Exception 0x{:08X} in handler for packet ID 0x{:02x} ({})",
            GetExceptionCode(), static_cast<uint8_t>(ctx.id()), SDK::getPacketName(ctx.id()));
        return false;
    }
}

Pipeline::Handle Pipeline::on(SDK::PacketID id, PacketHandler fn, const std::atomic<bool>* enabledGate)
{
    std::unique_lock<std::shared_mutex> lock{m_mutex};
    const Handle h = m_next++;

    Entry e{h, id, std::move(fn), enabledGate};
    if (id == kAnyPacket)
    {
        m_anyEntries.push_back(std::move(e));
        m_anyCount.fetch_add(1, std::memory_order_release);
    }
    else
    {
        uint8_t rawId = static_cast<uint8_t>(id);
        m_handlersById[rawId].push_back(std::move(e));
        m_handlerCount[rawId].fetch_add(1, std::memory_order_release);
    }

    return h;
}

void Pipeline::off(Handle h)
{
    std::unique_lock<std::shared_mutex> lock{m_mutex};
    std::vector<Entry>::iterator anyIt = std::ranges::find_if(m_anyEntries, [h](const Entry& e)
    {
        return e.handle == h;
    });
    if (anyIt != m_anyEntries.end())
    {
        m_anyEntries.erase(anyIt);
        m_anyCount.fetch_sub(1, std::memory_order_release);
        return;
    }

    for (size_t i = 0; i < m_handlersById.size(); ++i)
    {
        std::vector<Entry>::iterator it = std::ranges::find_if(m_handlersById[i], [h](const Entry& e)
        {
            return e.handle == h;
        });
        if (it != m_handlersById[i].end())
        {
            m_handlersById[i].erase(it);
            m_handlerCount[i].fetch_sub(1, std::memory_order_release);
            return;
        }
    }
}

void Pipeline::clear()
{
    std::unique_lock<std::shared_mutex> lock{m_mutex};
    m_anyEntries.clear();
    m_anyCount.store(0, std::memory_order_release);
    for (size_t i = 0; i < m_handlersById.size(); ++i)
    {
        m_handlersById[i].clear();
        m_handlerCount[i].store(0, std::memory_order_release);
    }
    m_next = 1;
}

bool Pipeline::process(PacketContext& ctx)
{
    SDK::PacketDumper::get().checkPacket(ctx);

    SDK::PacketID pktId = ctx.id();
    uint8_t rawId = static_cast<uint8_t>(pktId);

    // Fast-path: if no handlers exist for this packet ID and no any-handlers, return immediately!
    if (m_anyCount.load(std::memory_order_relaxed) == 0 &&
        m_handlerCount[rawId].load(std::memory_order_relaxed) == 0)
    {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock{m_mutex};

    // 1. Check any-packet handlers (rare)
    for (const Entry& e : m_anyEntries)
    {
        if (!e.enabledGate || e.enabledGate->load(std::memory_order_relaxed))
        {
            if (e.fn)
            {
                invokeHandlerSafe(e.fn, ctx);
                if (ctx.dropped)
                {
                    return true;
                }
            }
        }
    }

    // 2. Fast O(1) indexed packet handlers
    const std::vector<Entry>& entries = m_handlersById[rawId];
    for (const Entry& e : entries)
    {
        if (!e.enabledGate || e.enabledGate->load(std::memory_order_relaxed))
        {
            if (e.fn)
            {
                invokeHandlerSafe(e.fn, ctx);
                if (ctx.dropped)
                {
                    return true;
                }
            }
        }
    }

    return ctx.dropped;
}
