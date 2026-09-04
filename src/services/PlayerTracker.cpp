#include "PlayerTracker.h"
#include "pch.h"
#include "pipeline/PacketContext.h"
#include "pipeline/Pipeline.h"
#include "sdk/Game.h"
#include "sdk/SafeString.h"
#include "sdk/packets/AddPlayerPacket.h"
#include "sdk/packets/ChangeDimensionPacket.h"
#include "sdk/packets/MoveActorAbsolutePacket.h"
#include "sdk/packets/MoveActorDeltaPacket.h"
#include "sdk/packets/MovePlayerPacket.h"
#include "sdk/packets/PlayerListPacket.h"
#include "sdk/packets/PlayStatusPacket.h"
#include "sdk/packets/RemoveActorPacket.h"
#include <algorithm>
#include <format>
#include <iostream>

float TrackedPlayer::distanceToLocalPlayer() const
{
    SDK::LocalPlayer* lp = SDK::Game::player();
    if (!lp)
    {
        return 0.0f;
    }
    float* p = lp->getPos();
    return p ? distanceTo(p) : 0.0f;
}

std::string TrackedPlayer::getPlatformName() const
{
    switch (buildPlatform)
    {
    case SDK::BuildPlatform::GOOGLE:        return "Android";
    case SDK::BuildPlatform::IOS:           return "iOS";
    case SDK::BuildPlatform::OSX:           return "macOS";
    case SDK::BuildPlatform::AMAZON:        return "Amazon";
    case SDK::BuildPlatform::GEAR_VR:       return "GearVR";
    case SDK::BuildPlatform::HOLOLENS:      return "HoloLens";
    case SDK::BuildPlatform::UWP:           return "Windows UWP";
    case SDK::BuildPlatform::WINDOWS_32:    return "Windows";
    case SDK::BuildPlatform::DEDICATED:     return "Dedicated";
    case SDK::BuildPlatform::TV_OS:         return "AppleTV";
    case SDK::BuildPlatform::SONY:          return "PlayStation";
    case SDK::BuildPlatform::NX:            return "Switch";
    case SDK::BuildPlatform::XBOX:          return "Xbox";
    case SDK::BuildPlatform::WINDOWS_PHONE: return "Windows Phone";
    case SDK::BuildPlatform::LINUX:         return "Linux";
    default:                                return "Unknown";
    }
}

static inline float byteToDegrees(uint8_t byteVal)
{
    return byteVal * (360.0f / 256.0f);
}

std::string PlayerTracker::uuidToString(const uint8_t uuid[16])
{
    return SDK::uuidToString(uuid);
}

std::shared_ptr<TrackedPlayer> PlayerTracker::getOrCreatePlayerLocked(const std::string& uuid, bool& isNew)
{
    std::unordered_map<std::string, std::shared_ptr<TrackedPlayer>>::iterator it = m_players.find(uuid);
    if (it != m_players.end() && it->second)
    {
        unbindPlayerIndices(it->second);
        isNew = false;
        return it->second;
    }

    std::shared_ptr<TrackedPlayer> player = std::make_shared<TrackedPlayer>();
    player->uuid = uuid;
    player->firstSeen = std::chrono::steady_clock::now();
    m_players[uuid] = player;
    isNew = true;
    return player;
}

void PlayerTracker::unbindPlayerIndices(const std::shared_ptr<TrackedPlayer>& player)
{
    if (!player)
    {
        return;
    }
    if (player->runtimeEntityId != 0)
    {
        m_runtimeIdToPlayer.erase(player->runtimeEntityId);
    }
    if (player->uniqueEntityId != 0)
    {
        m_entityIdToUuid.erase(player->uniqueEntityId);
    }
    if (!player->name.empty())
    {
        m_nameToUuid.erase(SDK::toLower(player->name));
    }
    if (!player->nametag.empty())
    {
        m_nametagToUuid.erase(SDK::toLower(SDK::stripColorCodes(player->nametag)));
    }
}

void PlayerTracker::bindPlayerIndices(const std::shared_ptr<TrackedPlayer>& player)
{
    if (!player || player->uuid.empty())
    {
        return;
    }
    if (player->runtimeEntityId != 0)
    {
        m_runtimeIdToPlayer[player->runtimeEntityId] = player;
    }
    if (player->uniqueEntityId != 0)
    {
        m_entityIdToUuid[player->uniqueEntityId] = player->uuid;
    }
    if (!player->name.empty())
    {
        m_nameToUuid[SDK::toLower(player->name)] = player->uuid;
    }
    if (!player->nametag.empty())
    {
        m_nametagToUuid[SDK::toLower(SDK::stripColorCodes(player->nametag))] = player->uuid;
    }
}

void PlayerTracker::addPlayer(const std::string& uuid, const std::string& playerName, int64_t entityId)
{
    addPlayer(uuid, playerName, entityId, 0, nullptr);
}

void PlayerTracker::addPlayer(const std::string& uuid, const std::string& playerName, int64_t entityId, int64_t runtimeId, const float pos[3])
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    bool isNew = false;
    std::shared_ptr<TrackedPlayer> player = getOrCreatePlayerLocked(uuid, isNew);

    if (!playerName.empty())
    {
        player->name = playerName;
    }
    if (entityId != 0)
    {
        player->uniqueEntityId = entityId;
    }
    if (runtimeId != 0)
    {
        player->runtimeEntityId = runtimeId;
        player->isSpawned = true;
    }
    if (pos)
    {
        player->updatePosition(pos);
    }

    player->lastSeen = std::chrono::steady_clock::now();
    bindPlayerIndices(player);

    if (isNew)
    {
        for (const PlayerCallback& cb : m_joinCallbacks)
        {
            if (cb)
            {
                cb(player);
            }
        }
    }
}

void PlayerTracker::updatePlayerPosition(int64_t runtimeId, const float pos[3])
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::unordered_map<int64_t, std::shared_ptr<TrackedPlayer>>::const_iterator it = m_runtimeIdToPlayer.find(runtimeId);
    if (it == m_runtimeIdToPlayer.end() || !it->second)
    {
        return;
    }

    it->second->updatePosition(pos);
    if (!m_moveCallbacks.empty())
    {
        for (const PlayerCallback& cb : m_moveCallbacks)
        {
            if (cb)
            {
                cb(it->second);
            }
        }
    }
}

void PlayerTracker::updatePlayerTransform(int64_t runtimeId, const float pos[3], float pitch, float yaw, float headYaw, bool onGround)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    if (m_runtimeIdToPlayer.empty())
    {
        return;
    }
    std::unordered_map<int64_t, std::shared_ptr<TrackedPlayer>>::const_iterator it = m_runtimeIdToPlayer.find(runtimeId);
    if (it == m_runtimeIdToPlayer.end() || !it->second)
    {
        return;
    }

    it->second->updateTransform(pos, pitch, yaw, headYaw, onGround);
    if (!m_moveCallbacks.empty())
    {
        for (const PlayerCallback& cb : m_moveCallbacks)
        {
            if (cb)
            {
                cb(it->second);
            }
        }
    }
}

void PlayerTracker::removePlayer(int64_t runtimeId)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::unordered_map<int64_t, std::shared_ptr<TrackedPlayer>>::iterator it = m_runtimeIdToPlayer.find(runtimeId);
    if (it != m_runtimeIdToPlayer.end() && it->second)
    {
        removePlayerByUuid(it->second->uuid);
    }
}

void PlayerTracker::removePlayerByUuid(std::string_view uuid)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::unordered_map<std::string, std::shared_ptr<TrackedPlayer>>::iterator it = m_players.find(std::string(uuid));
    if (it != m_players.end())
    {
        std::shared_ptr<TrackedPlayer> player = it->second;
        unbindPlayerIndices(player);
        m_players.erase(it);

        if (player)
        {
            for (const PlayerCallback& cb : m_leaveCallbacks)
            {
                if (cb)
                {
                    cb(player);
                }
            }
        }
    }
}

std::shared_ptr<TrackedPlayer> PlayerTracker::getPlayer(int64_t runtimeId) const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::unordered_map<int64_t, std::shared_ptr<TrackedPlayer>>::const_iterator it = m_runtimeIdToPlayer.find(runtimeId);
    if (it != m_runtimeIdToPlayer.end())
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<TrackedPlayer> PlayerTracker::getPlayerByUuid(std::string_view uuid) const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::unordered_map<std::string, std::shared_ptr<TrackedPlayer>>::const_iterator it = m_players.find(std::string(uuid));
    return (it != m_players.end()) ? it->second : nullptr;
}

std::shared_ptr<TrackedPlayer> PlayerTracker::getPlayerByEntityId(int64_t entityId) const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::unordered_map<int64_t, std::string>::const_iterator it = m_entityIdToUuid.find(entityId);
    if (it == m_entityIdToUuid.end())
    {
        return nullptr;
    }
    std::unordered_map<std::string, std::shared_ptr<TrackedPlayer>>::const_iterator pIt = m_players.find(it->second);
    return (pIt != m_players.end()) ? pIt->second : nullptr;
}

std::shared_ptr<TrackedPlayer> PlayerTracker::findPlayerByName(std::string_view name) const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::string lower = SDK::toLower(SDK::stripColorCodes(SDK::trim(name)));
    if (lower.empty())
    {
        return nullptr;
    }

    std::unordered_map<std::string, std::string>::const_iterator itName = m_nameToUuid.find(lower);
    if (itName != m_nameToUuid.end())
    {
        return getPlayerByUuid(itName->second);
    }

    std::unordered_map<std::string, std::string>::const_iterator itTag = m_nametagToUuid.find(lower);
    if (itTag != m_nametagToUuid.end())
    {
        return getPlayerByUuid(itTag->second);
    }

    for (const std::pair<const std::string, std::shared_ptr<TrackedPlayer>>& pair : m_players)
    {
        if (!pair.second)
        {
            continue;
        }
        std::string pName = SDK::toLower(pair.second->name);
        std::string pTag = SDK::toLower(SDK::stripColorCodes(pair.second->nametag));

        if (pName == lower || pTag == lower ||
            pName.find(lower) != std::string::npos ||
            pTag.find(lower) != std::string::npos ||
            SDK::toLower(pair.second->uuid).starts_with(lower))
        {
            return pair.second;
        }
    }

    return nullptr;
}

std::shared_ptr<TrackedPlayer> PlayerTracker::findPlayerByNametag(std::string_view nametag) const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::string lower = SDK::toLower(SDK::stripColorCodes(nametag));
    std::unordered_map<std::string, std::string>::const_iterator it = m_nametagToUuid.find(lower);
    return (it != m_nametagToUuid.end()) ? getPlayerByUuid(it->second) : nullptr;
}

std::vector<std::shared_ptr<TrackedPlayer>> PlayerTracker::getAllPlayerPtrs() const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::vector<std::shared_ptr<TrackedPlayer>> result;
    result.reserve(m_players.size());
    for (const std::pair<const std::string, std::shared_ptr<TrackedPlayer>>& pair : m_players)
    {
        if (pair.second)
        {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<TrackedPlayer>> PlayerTracker::getWorldPlayers() const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    std::vector<std::shared_ptr<TrackedPlayer>> result;
    result.reserve(m_runtimeIdToPlayer.size());
    for (const std::pair<const int64_t, std::shared_ptr<TrackedPlayer>>& pair : m_runtimeIdToPlayer)
    {
        if (pair.second && pair.second->isSpawned)
        {
            result.push_back(pair.second);
        }
    }
    return result;
}

size_t PlayerTracker::getPlayerCount() const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_players.size();
}

size_t PlayerTracker::getWorldPlayerCount() const
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_runtimeIdToPlayer.size();
}

void PlayerTracker::clearWorldActors()
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_runtimeIdToPlayer.clear();
    for (const std::pair<const std::string, std::shared_ptr<TrackedPlayer>>& pair : m_players)
    {
        if (pair.second)
        {
            pair.second->isSpawned = false;
            pair.second->runtimeEntityId = 0;
        }
    }
}

void PlayerTracker::reset()
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_players.clear();
    m_runtimeIdToPlayer.clear();
    m_entityIdToUuid.clear();
    m_nameToUuid.clear();
    m_nametagToUuid.clear();
}

void PlayerTracker::shutdown()
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    reset();
    m_joinCallbacks.clear();
    m_leaveCallbacks.clear();
    m_spawnCallbacks.clear();
    m_despawnCallbacks.clear();
    m_moveCallbacks.clear();
}

void PlayerTracker::onPlayerJoin(PlayerCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_joinCallbacks.push_back(std::move(cb));
}

void PlayerTracker::onPlayerLeave(PlayerCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_leaveCallbacks.push_back(std::move(cb));
}

void PlayerTracker::onPlayerSpawn(PlayerCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_spawnCallbacks.push_back(std::move(cb));
}

void PlayerTracker::onPlayerDespawn(PlayerDespawnCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_despawnCallbacks.push_back(std::move(cb));
}

void PlayerTracker::onPlayerMove(PlayerCallback cb)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_moveCallbacks.push_back(std::move(cb));
}

void PlayerTracker::init()
{
    if (m_initialized)
    {
        return;
    }
    m_initialized = true;

    // Standard PlayerListPacket (0x3F): Server roster lifecycle
    Pipeline::get().on<SDK::PlayerListPacket>([this](TypedPacketContext<SDK::PlayerListPacket>& ctx)
    {
        if (ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }

        for (const SDK::PlayerListPacket::Entry& entry : ctx.packet->entries)
        {
            std::string uuidStr = SDK::uuidToString(entry.uuid);
            if (entry.action == 0) // ADD
            {
                std::string cleanName = SDK::stripColorCodes(entry.name);
                if (cleanName.empty())
                {
                    cleanName = entry.name;
                }

                std::lock_guard<std::recursive_mutex> lk(m_mutex);
                bool isNew = false;
                std::shared_ptr<TrackedPlayer> player = getOrCreatePlayerLocked(uuidStr, isNew);

                if (!cleanName.empty())
                {
                    player->name = cleanName;
                }
                if (!entry.xuid.empty())
                {
                    player->xuid = entry.xuid;
                }
                if (!entry.platformOnlineId.empty())
                {
                    player->platformOnlineId = entry.platformOnlineId;
                }
                if (entry.entityId != 0)
                {
                    player->uniqueEntityId = entry.entityId;
                }
                player->buildPlatform = static_cast<SDK::BuildPlatform>(entry.buildPlatform);
                player->lastSeen = std::chrono::steady_clock::now();

                bindPlayerIndices(player);

                if (isNew)
                {
                    for (const PlayerCallback& cb : m_joinCallbacks)
                    {
                        if (cb)
                        {
                            cb(player);
                        }
                    }
                }
            }
            else if (entry.action == 1) // REMOVE
            {
                removePlayerByUuid(uuidStr);
            }
        }
    });

    // Standard AddPlayerPacket (0x0C): In-world 3D spawn, positions, and nametags
    Pipeline::get().on<SDK::AddPlayerPacket>([this](TypedPacketContext<SDK::AddPlayerPacket>& ctx)
    {
        if (ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }

        std::string uuidStr = SDK::uuidToString(ctx.packet->uuid);
        int64_t runtimeId = ctx.packet->runtimeEntityId;
        std::string rawName = SDK::cleanPlayerName(ctx.packet->username.view());
        if (rawName.empty())
        {
            rawName = SDK::stripColorCodes(ctx.packet->username);
        }

        std::string_view rawNametag = ctx.packet->getNametag();
        std::shared_ptr<TrackedPlayer> player;
        bool isNew = false;

        {
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
            player = getOrCreatePlayerLocked(uuidStr, isNew);

            if (!rawName.empty() && player->name.empty())
            {
                player->name = rawName;
            }
            if (ctx.packet->uniqueEntityId != 0)
            {
                player->uniqueEntityId = ctx.packet->uniqueEntityId;
            }

            player->runtimeEntityId = runtimeId;
            player->isSpawned = true;
            player->updatePosition(ctx.packet->pos);
            player->pitch = ctx.packet->rotation[0];
            player->yaw = ctx.packet->rotation[1];
            player->headYaw = ctx.packet->rotation[2];
            player->motion[0] = ctx.packet->motion[0];
            player->motion[1] = ctx.packet->motion[1];
            player->motion[2] = ctx.packet->motion[2];
            player->deviceId = ctx.packet->deviceId;
            player->buildPlatform = ctx.packet->buildPlatform;
            player->nametagAlwaysShow = ctx.packet->entityData.getByte(SDK::EntityDataIDs::NAMETAG_ALWAYS_SHOW) != 0 || ctx.packet->entityData.getFlag(SDK::EntityFlag::ALWAYS_SHOW_NAME);

            std::string cleanTag = SDK::stripColorCodes(rawNametag);
            if (!rawNametag.empty() && !SDK::trim(cleanTag).empty())
            {
                player->nametag = std::string(rawNametag);
            }

            player->lastSeen = std::chrono::steady_clock::now();
            bindPlayerIndices(player);
        }

        if (player)
        {
            if (isNew)
            {
                for (const PlayerCallback& cb : m_joinCallbacks)
                {
                    if (cb)
                    {
                        cb(player);
                    }
                }
            }

            for (const PlayerCallback& cb : m_spawnCallbacks)
            {
                if (cb)
                {
                    cb(player);
                }
            }
        }
    });

    // Real-time movement packets (gated by m_trackMovement to prevent network tick CPU overhead)
    Pipeline::get().on<SDK::MoveActorAbsolutePacket>([this](TypedPacketContext<SDK::MoveActorAbsolutePacket>& ctx)
    {
        if (!m_trackMovement.load(std::memory_order_relaxed) || ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }
        float pitch = byteToDegrees(ctx.packet->pitch);
        float yaw = byteToDegrees(ctx.packet->yaw);
        float headYaw = byteToDegrees(ctx.packet->headYaw);
        bool onGround = (ctx.packet->flags & 0x01) != 0;
        updatePlayerTransform(ctx.packet->runtimeEntityId, ctx.packet->pos, pitch, yaw, headYaw, onGround);
    }, &m_trackMovement);

    Pipeline::get().on<SDK::MovePlayerPacket>([this](TypedPacketContext<SDK::MovePlayerPacket>& ctx)
    {
        if (!m_trackMovement.load(std::memory_order_relaxed) || ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }
        updatePlayerTransform(ctx.packet->runtimeEntityId, ctx.packet->pos, ctx.packet->pitch, ctx.packet->yaw, ctx.packet->headYaw, ctx.packet->onGround);
    }, &m_trackMovement);

    Pipeline::get().on<SDK::MoveActorDeltaPacket>([this](TypedPacketContext<SDK::MoveActorDeltaPacket>& ctx)
    {
        if (!m_trackMovement.load(std::memory_order_relaxed) || ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }
        float pitch = byteToDegrees(ctx.packet->pitch);
        float yaw = byteToDegrees(ctx.packet->yaw);
        float headYaw = byteToDegrees(ctx.packet->headYaw);
        updatePlayerTransform(ctx.packet->runtimeEntityId, ctx.packet->pos, pitch, yaw, headYaw, true);
    }, &m_trackMovement);

    // Actor despawn (RemoveActorPacket 0x0E): Preserves last known coordinates, unbinds runtimeEntityId
    Pipeline::get().on<SDK::RemoveActorPacket>([this](TypedPacketContext<SDK::RemoveActorPacket>& ctx)
    {
        if (ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }

        int64_t removedId = ctx.packet->uniqueEntityId;
        TrackedPlayer playerDataCopy;
        bool hasData = false;

        {
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
            std::shared_ptr<TrackedPlayer> player = getPlayer(removedId);
            if (!player)
            {
                player = getPlayerByEntityId(removedId);
            }

            if (player)
            {
                playerDataCopy = *player;
                hasData = true;

                if (player->runtimeEntityId != 0)
                {
                    m_runtimeIdToPlayer.erase(player->runtimeEntityId);
                    player->runtimeEntityId = 0;
                }
                player->isSpawned = false;
                player->lastSeen = std::chrono::steady_clock::now();
            }
        }

        if (hasData)
        {
            for (const PlayerDespawnCallback& cb : m_despawnCallbacks)
            {
                if (cb)
                {
                    cb(removedId, playerDataCopy);
                }
            }
        }
    });

    // Dimension / Map transitions
    Pipeline::get().on<SDK::ChangeDimensionPacket>([this](TypedPacketContext<SDK::ChangeDimensionPacket>& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound && ctx.packet && m_autoClearOnDimension)
        {
            clearWorldActors();
        }
    });

    // Server Disconnect
    Pipeline::get().on(SDK::PacketID::DISCONNECT, [this](PacketContext& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            reset();
        }
    });

    // Server transfer (moving between proxies or sub-servers)
    Pipeline::get().on(SDK::PacketID::TRANSFER, [this](PacketContext& ctx)
    {
        if (ctx.dir == PacketDirection::Inbound)
        {
            reset();
        }
    });
}
