#include "pch.h"
#include "PacketDumper.h"
#include "sdk/Chat.h"
#include "sdk/Logger.h"
#include "sdk/SafeString.h"
#include "sdk/SafeMem.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace SDK
{

PacketDumper& PacketDumper::get()
{
    static PacketDumper instance;
    return instance;
}

bool PacketDumper::arm(PacketDirection direction, uint8_t packetId, size_t byteCount, bool anyDirection)
{
    if (byteCount == 0)
    {
        byteCount = 384;
    }
    if (byteCount > 2048)
    {
        byteCount = 2048;
    }

    m_targetId.store(packetId, std::memory_order_relaxed);
    m_targetDirection.store(direction, std::memory_order_relaxed);
    m_anyDirection.store(anyDirection, std::memory_order_relaxed);
    m_dumpBytes.store(byteCount, std::memory_order_relaxed);
    m_armed.store(true, std::memory_order_release);
    return true;
}

void PacketDumper::disarm()
{
    m_armed.store(false, std::memory_order_release);
}

bool PacketDumper::isArmed() const noexcept
{
    return m_armed.load(std::memory_order_acquire);
}

uint8_t PacketDumper::targetPacketId() const noexcept
{
    return m_targetId.load(std::memory_order_relaxed);
}

PacketDirection PacketDumper::targetDirection() const noexcept
{
    return m_targetDirection.load(std::memory_order_relaxed);
}

void PacketDumper::checkPacket(const PacketContext& ctx)
{
    if (!m_armed.load(std::memory_order_relaxed))
    {
        return;
    }

    if (!ctx.packet)
    {
        return;
    }

    const uint8_t targetId = m_targetId.load(std::memory_order_relaxed);
    const bool matchAny = m_anyDirection.load(std::memory_order_relaxed);
    const PacketDirection targetDir = m_targetDirection.load(std::memory_order_relaxed);

    uint8_t currentId = static_cast<uint8_t>(ctx.id());
    if (currentId == 0)
    {
        return;
    }

    if (currentId != targetId)
    {
        return;
    }

    if (!matchAny && ctx.dir != targetDir)
    {
        return;
    }

    // Disarm latch immediately (one-shot capture)
    m_armed.store(false, std::memory_order_release);

    std::string report = dumpPacketMemory(ctx, m_dumpBytes.load(std::memory_order_relaxed));
    writeToDumperLog(report);

    std::string dirStr = (ctx.dir == PacketDirection::Inbound) ? "INBOUND" : "OUTBOUND";
    std::string pktName = getPacketName(ctx.packet->getID());

    SDK::Log::log("[PacketDumper] Captured and dumped {} packet 0x{:02x} ({}) to butils_dumper.log", dirStr, currentId, pktName);
    SDK::Chat::success(std::format("Captured & dumped §f{}§a packet §e0x{:02x} ({})§a to §fbutils_dumper.log§a!",
        dirStr, currentId, pktName));
}

static bool isPrintableAscii(char c) noexcept
{
    return static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) <= 126;
}

static bool isReadableMemoryRange(const void* ptr, size_t size)
{
    return SDK::Memory::isValidReadPtr(ptr, size);
}

std::string PacketDumper::dumpPacketMemory(const PacketContext& ctx, size_t maxBytes)
{
    if (!ctx.packet)
    {
        return "Packet is null.";
    }

    std::string dirStr = (ctx.dir == PacketDirection::Inbound) ? "INBOUND" : "OUTBOUND";
    uint8_t pktId = static_cast<uint8_t>(ctx.packet->getID());
    std::string pktName = getPacketName(ctx.packet->getID());

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    std::tm tmNow{};
    localtime_s(&tmNow, &timeNow);

    char timeBuffer[64] = {};
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmNow);

    std::ostringstream ss;
    ss << "\n====================================================================================================\n";
    ss << std::format(" PACKET MEMORY DUMP: ID 0x{:02x} ({}) — {} (Requested: {} bytes) — {}\n",
        pktId, pktName, dirStr, maxBytes, timeBuffer);
    ss << "====================================================================================================\n";
    ss << "RAW MEMORY (HEX + ASCII):\n";

    const uint8_t* base = reinterpret_cast<const uint8_t*>(ctx.packet);
    size_t validBytes = 0;

    // Determine readable byte count
    for (size_t i = 0; i < maxBytes; ++i)
    {
        if (isReadableMemoryRange(base + i, 1))
        {
            validBytes = i + 1;
        }
        else
        {
            break;
        }
    }

    for (size_t i = 0; i < validBytes; i += 16)
    {
        ss << std::format("+0x{:04x} | ", i);

        // Hex bytes
        for (size_t j = 0; j < 16; ++j)
        {
            if (i + j < validBytes)
            {
                ss << std::format("{:02x} ", base[i + j]);
            }
            else
            {
                ss << "   ";
            }
            if (j == 7)
            {
                ss << " ";
            }
        }

        ss << "| ";

        // ASCII representation
        for (size_t j = 0; j < 16 && (i + j) < validBytes; ++j)
        {
            char c = static_cast<char>(base[i + j]);
            ss << (isPrintableAscii(c) ? c : '.');
        }

        ss << "\n";
    }

    ss << "----------------------------------------------------------------------------------------------------\n";
    ss << "HEURISTIC FIELD & STRUCTURE DETECTIONS:\n";

    // 1. Scan for std::string / SafeString structures (32 bytes aligned to 8)
    for (size_t off = 0x30; off + 32 <= validBytes; off += 8)
    {
        SafeString s{};
        if (!SDK::Memory::read(base + off, &s, sizeof(s)))
        {
            continue;
        }

        size_t size = s.size;
        size_t cap = s.res;

        // SSO check (size <= 15, cap == 15, valid printable or Minecraft string)
        if (cap == 15 && size <= 15)
        {
            if (size > 0 && s.buf[size] == '\0')
            {
                bool printable = true;
                for (size_t k = 0; k < size; ++k)
                {
                    if (static_cast<unsigned char>(s.buf[k]) < 32 && s.buf[k] != '\n' && s.buf[k] != '\r')
                    {
                        printable = false;
                        break;
                    }
                }
                if (printable)
                {
                    std::string_view sv(s.buf, size);
                    ss << std::format("  [+0x{:04x}] std::string (SSO, len={}, cap={}): \"{}\"\n", off, size, cap, sv);
                }
            }
        }
        // Heap-allocated string check
        else if (cap >= size && size > 0 && size <= 32768 && cap <= 65536)
        {
            if (isReadableMemoryRange(s.ptr, size + 1))
            {
                char nullTerm = '\0';
                if (SDK::Memory::read(s.ptr + size, &nullTerm, 1) && nullTerm == '\0')
                {
                    bool printable = true;
                    for (size_t k = 0; k < size; ++k)
                    {
                        char ch = 0;
                        if (!SDK::Memory::read(s.ptr + k, &ch, 1))
                        {
                            printable = false;
                            break;
                        }
                        if (static_cast<unsigned char>(ch) < 32 && ch != '\n' && ch != '\r' && ch != '\t')
                        {
                            printable = false;
                            break;
                        }
                    }
                    if (printable)
                    {
                        std::string_view sv(s.ptr, size);
                        ss << std::format("  [+0x{:04x}] std::string (HEAP, len={}, cap={}): \"{}\"\n", off, size, cap, sv);
                    }
                }
            }
        }
    }

    // 2. Scan for 3D coordinates (3 sequential floats)
    for (size_t off = 0x30; off + 12 <= validBytes; off += 4)
    {
        float f[3]{};
        if (SDK::Memory::read(base + off, f, sizeof(f)))
        {
            float x = f[0];
            float y = f[1];
            float z = f[2];

            if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z))
            {
                if (x >= -100000.0f && x <= 100000.0f &&
                    y >= -200.0f    && y <= 500.0f &&
                    z >= -100000.0f && z <= 100000.0f &&
                    !(x == 0.0f && y == 0.0f && z == 0.0f))
                {
                    ss << std::format("  [+0x{:04x}] Vec3 coords: ({:.2f}, {:.2f}, {:.2f})\n", off, x, y, z);
                    off += 8; // Advance past y and z
                }
            }
        }
    }

    // 3. Scan for variant discriminant tags (single byte at +0x20 relative to string or variant boundary)
    for (size_t off = 0x30; off < validBytes; off += 8)
    {
        uint8_t byteVal = base[off];
        if (byteVal > 0 && byteVal <= 4)
        {
            // Check if surrounded by 7 zeros (common for 8-byte aligned variant tag)
            bool zerosAround = true;
            for (size_t k = 1; k < 8 && (off + k) < validBytes; ++k)
            {
                if (base[off + k] != 0)
                {
                    zerosAround = false;
                    break;
                }
            }
            if (zerosAround)
            {
                ss << std::format("  [+0x{:04x}] Variant discriminant index / enum: {}\n", off, static_cast<int>(byteVal));
            }
        }
    }

    ss << "====================================================================================================\n\n";
    return ss.str();
}

bool PacketDumper::writeToDumperLog(std::string_view content)
{
    std::lock_guard<std::mutex> lk(m_dumpMutex);

    std::string logDir = SDK::Log::getLogDirectory();
    std::string filePath = logDir + "\\butils_dumper.log";

    std::ofstream file(filePath, std::ios::out | std::ios::app);
    if (!file.is_open())
    {
        return false;
    }

    file << content;
    file.flush();
    file.close();
    return true;
}

bool PacketDumper::parsePacketId(std::string_view token, uint8_t& outId, std::string& outName)
{
    if (token.empty())
    {
        return false;
    }

    std::string lower = SDK::toLower(token);

    // 1. Check hexadecimal (0x...)
    if (lower.starts_with("0x") && lower.size() > 2)
    {
        try
        {
            unsigned long val = std::stoul(lower.substr(2), nullptr, 16);
            if (val <= 255)
            {
                outId = static_cast<uint8_t>(val);
                outName = getPacketName(static_cast<PacketID>(outId));
                return true;
            }
        }
        catch (...) {}
    }

    // 2. Check decimal
    bool isDec = true;
    for (char c : lower)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
        {
            isDec = false;
            break;
        }
    }
    if (isDec)
    {
        try
        {
            unsigned long val = std::stoul(lower, nullptr, 10);
            if (val <= 255)
            {
                outId = static_cast<uint8_t>(val);
                outName = getPacketName(static_cast<PacketID>(outId));
                return true;
            }
        }
        catch (...) {}
    }

    // 3. Check by standard name mapping
    static const std::unordered_map<std::string, uint8_t> s_nameMap = {
        {"login", 0x01},
        {"play_status", 0x02},
        {"disconnect", 0x05},
        {"text", 0x09},
        {"chat", 0x09},
        {"add_player", 0x0C},
        {"add_actor", 0x0D},
        {"remove_actor", 0x0E},
        {"move_actor_absolute", 0x12},
        {"move_player", 0x13},
        {"change_dimension", 0x3D},
        {"player_list", 0x3F},
        {"available_commands", 0x4C},
        {"command_request", 0x4D},
        {"command_output", 0x4F},
        {"transfer", 0x55},
        {"set_title", 0x58},
        {"modal_form_request", 0x64},
        {"modal_form_response", 0x65},
        {"move_actor_delta", 0x6F},
    };

    std::unordered_map<std::string, uint8_t>::const_iterator it = s_nameMap.find(lower);
    if (it != s_nameMap.end())
    {
        outId = it->second;
        outName = getPacketName(static_cast<PacketID>(outId));
        return true;
    }

    return false;
}

} // namespace SDK
