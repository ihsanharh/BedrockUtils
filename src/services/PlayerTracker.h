#pragma once
#include "sdk/SafeString.h"
#include "sdk/packets/AddPlayerPacket.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SDK
{
    struct LocalPlayer;
}

struct TrackedPlayer
{
    // Identity & Roster
    std::string uuid;
    std::string name;              // Clean username (color codes stripped)
    std::string xuid;              // Xbox User ID
    std::string platformOnlineId;  // Platform Online Tag
    std::string deviceId;          // Device ID
    SDK::BuildPlatform buildPlatform = SDK::BuildPlatform::UNKNOWN;

    // Nametags
    std::string nametag;           // Real in-world nametag with formatting and colors (e.g. "§6[Lvl 20] §bSteve§r")
    bool nametagAlwaysShow = false;

    // Entity IDs
    int64_t uniqueEntityId  = 0;
    int64_t runtimeEntityId = 0;   // 0 when out of render distance

    // Spatial & State (pos holds current coords or last known coords when out of sight)
    bool  isSpawned = false;       // true when actively rendered in 3D world
    float pos[3]     = {0.0f, 0.0f, 0.0f};
    float prevPos[3] = {0.0f, 0.0f, 0.0f};
    float motion[3]  = {0.0f, 0.0f, 0.0f};
    float pitch      = 0.0f;
    float yaw        = 0.0f;
    float headYaw    = 0.0f;
    bool  onGround   = true;

    // Timestamps
    std::chrono::steady_clock::time_point firstSeen{};
    std::chrono::steady_clock::time_point lastSeen{};
    std::chrono::steady_clock::time_point lastPosUpdate{};

    // Helper methods
    void updatePosition(const float p[3])
    {
        if (p)
        {
            prevPos[0] = pos[0]; prevPos[1] = pos[1]; prevPos[2] = pos[2];
            pos[0] = p[0]; pos[1] = p[1]; pos[2] = p[2];
            motion[0] = pos[0] - prevPos[0];
            motion[1] = pos[1] - prevPos[1];
            motion[2] = pos[2] - prevPos[2];
            lastPosUpdate = std::chrono::steady_clock::now();
            lastSeen = lastPosUpdate;
        }
    }

    void updateTransform(const float p[3], float pPitch, float pYaw, float pHeadYaw, bool pOnGround = true)
    {
        updatePosition(p);
        pitch = pPitch;
        yaw = pYaw;
        headYaw = pHeadYaw;
        onGround = pOnGround;
    }

    [[nodiscard]] const std::string& getNametag() const noexcept
    {
        return !nametag.empty() ? nametag : name;
    }

    [[nodiscard]] bool isSpawnedInWorld() const noexcept
    {
        return isSpawned && runtimeEntityId != 0;
    }

    [[nodiscard]] float distanceTo(float x, float y, float z) const noexcept
    {
        float dx = pos[0] - x;
        float dy = pos[1] - y;
        float dz = pos[2] - z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    [[nodiscard]] float distanceTo(const float p[3]) const noexcept
    {
        return p ? distanceTo(p[0], p[1], p[2]) : 0.0f;
    }

    [[nodiscard]] float distanceTo(const TrackedPlayer& other) const noexcept
    {
        return distanceTo(other.pos[0], other.pos[1], other.pos[2]);
    }

    [[nodiscard]] float distanceToLocalPlayer() const;

    [[nodiscard]] std::string getPlatformName() const;
};

class PlayerTracker
{
public:
    using PlayerCallback = std::function<void(const std::shared_ptr<TrackedPlayer>& player)>;
    using PlayerDespawnCallback = std::function<void(int64_t runtimeId, const TrackedPlayer& player)>;

    static PlayerTracker& get()
    {
        static PlayerTracker inst;
        return inst;
    }

    void init();
    void reset();
    void shutdown();
    void clearWorldActors();

    // Configuration
    void setAutoClearOnDimensionChange(bool enable) noexcept
    {
        m_autoClearOnDimension = enable;
    }

    [[nodiscard]] bool shouldAutoClearOnDimensionChange() const noexcept
    {
        return m_autoClearOnDimension;
    }

    void setTrackMovement(bool enable) noexcept
    {
        m_trackMovement.store(enable, std::memory_order_relaxed);
    }

    [[nodiscard]] bool isTrackingMovement() const noexcept
    {
        return m_trackMovement.load(std::memory_order_relaxed);
    }

    // Roster / PlayerList mutation
    void addPlayer(const std::string& uuid, const std::string& playerName, int64_t entityId);
    void addPlayer(const std::string& uuid, const std::string& playerName, int64_t entityId, int64_t runtimeId, const float pos[3]);
    void removePlayer(int64_t runtimeId);
    void removePlayerByUuid(std::string_view uuid);

    // World state mutation
    void updatePlayerPosition(int64_t runtimeId, const float pos[3]);
    void updatePlayerTransform(int64_t runtimeId, const float pos[3], float pitch, float yaw, float headYaw, bool onGround = true);

    // Queries
    std::shared_ptr<TrackedPlayer> getPlayer(int64_t runtimeId) const;
    std::shared_ptr<TrackedPlayer> getPlayerByUuid(std::string_view uuid) const;
    std::shared_ptr<TrackedPlayer> getPlayerByEntityId(int64_t entityId) const;
    std::shared_ptr<TrackedPlayer> findPlayerByName(std::string_view name) const;
    std::shared_ptr<TrackedPlayer> findPlayerByNametag(std::string_view nametag) const;

    // Collection queries
    std::vector<std::shared_ptr<TrackedPlayer>> getAllPlayerPtrs() const;
    std::vector<std::shared_ptr<TrackedPlayer>> getWorldPlayers() const;
    size_t getPlayerCount() const;
    size_t getWorldPlayerCount() const;

    // Events
    void onPlayerJoin(PlayerCallback cb);
    void onPlayerLeave(PlayerCallback cb);
    void onPlayerSpawn(PlayerCallback cb);
    void onPlayerDespawn(PlayerDespawnCallback cb);
    void onPlayerMove(PlayerCallback cb);

    static std::string uuidToString(const uint8_t uuid[16]);

private:
    PlayerTracker() = default;
    ~PlayerTracker() = default;

    std::shared_ptr<TrackedPlayer> getOrCreatePlayerLocked(const std::string& uuid, bool& isNew);
    void unbindPlayerIndices(const std::shared_ptr<TrackedPlayer>& player);
    void bindPlayerIndices(const std::shared_ptr<TrackedPlayer>& player);

    mutable std::recursive_mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<TrackedPlayer>> m_players; // UUID -> Player
    std::unordered_map<int64_t, std::shared_ptr<TrackedPlayer>> m_runtimeIdToPlayer;
    std::unordered_map<int64_t, std::string>     m_entityIdToUuid;
    std::unordered_map<std::string, std::string> m_nameToUuid;
    std::unordered_map<std::string, std::string> m_nametagToUuid;

    std::vector<PlayerCallback>        m_joinCallbacks;
    std::vector<PlayerCallback>        m_leaveCallbacks;
    std::vector<PlayerCallback>        m_spawnCallbacks;
    std::vector<PlayerDespawnCallback> m_despawnCallbacks;
    std::vector<PlayerCallback>        m_moveCallbacks;

    bool m_initialized = false;
    bool m_autoClearOnDimension = true;
    std::atomic<bool> m_trackMovement{false};
};
