#pragma once
#include "pipeline/PacketContext.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace SDK
{

class PacketDumper
{
public:
    static PacketDumper& get();

    // Arm the packet dumper latch for the next matching packet
    bool arm(PacketDirection direction, uint8_t packetId, size_t byteCount = 384, bool anyDirection = false);

    // Cancel any active pending dump latch
    void disarm();

    [[nodiscard]] bool isArmed() const noexcept;
    [[nodiscard]] uint8_t targetPacketId() const noexcept;
    [[nodiscard]] PacketDirection targetDirection() const noexcept;

    // Called on every packet in Pipeline::process
    void checkPacket(const PacketContext& ctx);

    // Formats packet raw memory and performs heuristic field extraction
    std::string dumpPacketMemory(const PacketContext& ctx, size_t maxBytes);

    // Appends report to butils_dumper.log
    bool writeToDumperLog(std::string_view content);

    // Resolves a user-provided string token (decimal, hex 0x.., or name) to a numeric PacketID
    static bool parsePacketId(std::string_view token, uint8_t& outId, std::string& outName);

private:
    PacketDumper() = default;
    ~PacketDumper() = default;

    std::atomic<bool>            m_armed{false};
    std::atomic<uint8_t>         m_targetId{0};
    std::atomic<PacketDirection> m_targetDirection{PacketDirection::Inbound};
    std::atomic<bool>            m_anyDirection{false};
    std::atomic<size_t>          m_dumpBytes{384};
    std::mutex                   m_dumpMutex;
};

} // namespace SDK
