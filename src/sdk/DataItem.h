#pragma once
#include "intercept/PacketInterceptor.h"
#include "sdk/SafeString.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

namespace SDK
{

#pragma pack(push, 4)

struct DataItem
{
    void* vtable; // +0x00
    uint8_t type; // +0x08
    bool dirty;   // +0x09
    uint16_t id;  // +0x0A
};

// Subclasses

struct ByteDataItem : DataItem
{
    uint8_t value; // +0x0C
};

struct ShortDataItem : DataItem
{
    int16_t value; // +0x0C
};

struct IntDataItem : DataItem
{
    int32_t value; // +0x0C
};

struct FloatDataItem : DataItem
{
    float value; // +0x0C
};

struct StringDataItem : DataItem
{
    // 4 bytes padding at +0x0C to align SafeString to +0x10
    uint8_t pad[4];
    SafeString value; // +0x10
};

struct Int64DataItem : DataItem
{
    // 4 bytes padding at +0x0C to align int64_t to +0x10
    uint8_t pad[4];
    int64_t value; // +0x10
};

#pragma pack(pop)

using LongDataItem = Int64DataItem;

namespace EntityDataIDs
{
constexpr uint16_t FLAGS = 0;
constexpr uint16_t STRUCTURAL_INTEGRITY = 1;
constexpr uint16_t VARIANT = 2;
constexpr uint16_t COLOR = 3;
constexpr uint16_t NAME = 4;
constexpr uint16_t NAMETAG = 4;
constexpr uint16_t OWNER_EID = 5;
constexpr uint16_t TARGET_EID = 6;
constexpr uint16_t AIR_SUPPLY = 7;
constexpr uint16_t EFFECT_COLOR = 8;
constexpr uint16_t EFFECT_AMBIENCE = 9;
constexpr uint16_t JUMP_DURATION = 10;
constexpr uint16_t HURT_TICKS = 11;
constexpr uint16_t HURT_DIRECTION = 12;
constexpr uint16_t ROW_TIME_LEFT = 13;
constexpr uint16_t ROW_TIME_RIGHT = 14;
constexpr uint16_t VALUE = 15;
constexpr uint16_t DISPLAY_FIREWORK = 16;
constexpr uint16_t DISPLAY_OFFSET = 17;
constexpr uint16_t CUSTOM_DISPLAY = 18;
constexpr uint16_t HORSE_TYPE = 19;
constexpr uint16_t OLD_SWELL = 20;
constexpr uint16_t SWELL_DIRECTION = 21;
constexpr uint16_t CHARGE_AMOUNT = 22;
constexpr uint16_t CLIENT_EVENT = 24;
constexpr uint16_t USING_ITEM = 25;
constexpr uint16_t PLAYER_FLAGS = 26;
constexpr uint16_t PLAYER_INDEX = 27;
constexpr uint16_t BED_POSITION = 28;
constexpr uint16_t FIREBALL_POWER_X = 29;
constexpr uint16_t FIREBALL_POWER_Y = 30;
constexpr uint16_t FIREBALL_POWER_Z = 31;
constexpr uint16_t AUX_POWER = 32;
constexpr uint16_t FISH_X = 33;
constexpr uint16_t FISH_Z = 34;
constexpr uint16_t FISH_ANGLE = 35;
constexpr uint16_t AUX_VALUE_DATA = 36;
constexpr uint16_t LEASH_HOLDER = 37;
constexpr uint16_t SCALE = 38;
constexpr uint16_t INTERACT_TEXT = 39;
constexpr uint16_t SKIN_ID = 40;
constexpr uint16_t ACTIONS = 41;
constexpr uint16_t AIR_SUPPLY_MAX = 42;
constexpr uint16_t MARK_VARIANT = 43;
constexpr uint16_t CONTAINER_TYPE = 44;
constexpr uint16_t CONTAINER_SIZE = 45;
constexpr uint16_t CONTAINER_STRENGTH_MODIFIER = 46;
constexpr uint16_t BLOCK_TARGET_POS = 47;
constexpr uint16_t WITHER_INVULNERABLE_TICKS = 48;
constexpr uint16_t WITHER_TARGET_A = 49;
constexpr uint16_t WITHER_TARGET_B = 50;
constexpr uint16_t WITHER_TARGET_C = 51;
constexpr uint16_t WITHER_AERIAL_ATTACK = 52;
constexpr uint16_t WIDTH = 53;
constexpr uint16_t HEIGHT = 54;
constexpr uint16_t FUSE_TIME = 55;
constexpr uint16_t SEAT_OFFSET = 56;
constexpr uint16_t SEAT_LOCK_RIDER_ROTATION = 57;
constexpr uint16_t SEAT_LOCK_RIDER_ROTATION_DEGREES = 58;
constexpr uint16_t SEAT_HAS_ROTATION = 59;
constexpr uint16_t AREA_EFFECT_CLOUD_RADIUS = 60;
constexpr uint16_t AREA_EFFECT_CLOUD_WAITING = 61;
constexpr uint16_t AREA_EFFECT_CLOUD_PARTICLE = 62;
constexpr uint16_t SHULKER_PEEK_AMOUNT = 63;
constexpr uint16_t SHULKER_ATTACH_FACE = 64;
constexpr uint16_t SHULKER_ATTACHED = 65;
constexpr uint16_t SHULKER_ATTACH_POS = 66;
constexpr uint16_t TRADE_TARGET_EID = 67;
constexpr uint16_t CAREER = 68;
constexpr uint16_t COMMAND_BLOCK_ENABLED = 69;
constexpr uint16_t COMMAND_BLOCK_NAME = 70;
constexpr uint16_t COMMAND_BLOCK_LAST_OUTPUT = 71;
constexpr uint16_t COMMAND_BLOCK_TRACK_OUTPUT = 72;
constexpr uint16_t CONTROLLING_RIDER_SEAT_INDEX = 73;
constexpr uint16_t STRENGTH = 74;
constexpr uint16_t STRENGTH_MAX = 75;
constexpr uint16_t EVOKER_SPELL_CASTING_COLOR = 76;
constexpr uint16_t DATA_LIFETIME_TICKS = 77;
constexpr uint16_t ARMOR_STAND_POSE_INDEX = 78;
constexpr uint16_t END_CRYSTAL_TICK_OFFSET = 79;
constexpr uint16_t NAMETAG_ALWAYS_SHOW = 80;
constexpr uint16_t COLOR_2 = 81;
constexpr uint16_t NAME_AUTHOR = 82;
constexpr uint16_t SCORE = 83;
constexpr uint16_t BALLOON_ANCHOR_EID = 84;
constexpr uint16_t PUFFED_STATE = 85;
constexpr uint16_t BOAT_BUBBLE_TIME = 86;
constexpr uint16_t AGENT_EID = 87;
constexpr uint16_t NAME_RAW_TEXT = 111;
constexpr uint16_t NAMEPLATE_RENDER_DISTANCE_MAX = 140;
} // namespace EntityDataIDs

enum class EntityFlag : uint32_t
{
    ON_FIRE = 0,
    SNEAKING = 1,
    RIDING = 2,
    SPRINTING = 3,
    USING_ITEM = 4,
    INVISIBLE = 5,
    TEMPTED = 6,
    IN_LOVE = 7,
    SADDLED = 8,
    POWERED = 9,
    IGNITED = 10,
    BABY = 11,
    CONVERTING = 12,
    CRITICAL = 13,
    CAN_SHOW_NAME = 14,
    ALWAYS_SHOW_NAME = 15,
    NO_AI = 16,
    SILENT = 17,
    WALL_CLIMBING = 18,
    CAN_CLIMB = 19,
    CAN_SWIM = 20,
    CAN_FLY = 21,
    CAN_WALK = 22,
    RESTING = 23,
    SITTING = 24,
    ANGRY = 25,
    INTERESTED = 26,
    CHARGED = 27,
    TAMED = 28,
    ORPHANED = 29,
    LEASHED = 30,
    SHEARED = 31,
    GLIDING = 32,
    ELDER = 33,
    MOVING = 34,
    BREATHING = 35,
    CHESTED = 36,
    STACKABLE = 37,
    SHOW_BOTTOM = 38,
    STANDING = 39,
    SHAKING = 40,
    IDLING = 41,
    CASTING = 42,
    CHARGING = 43,
    WASD_CONTROLLED = 44,
    CAN_POWER_JUMP = 45,
    LINGERING = 46,
    HAS_COLLISION = 47,
    HAS_GRAVITY = 48,
    FIRE_IMMUNE = 49,
    DANCING = 50,
    ENCHANTED = 51,
    RETURN_TRIDENT = 52,
    CONTAINER_IS_PRIVATE = 53,
    IS_TRANSFORMING = 54,
    DAMAGE_NEARBY_MOBS = 55,
    SWIMMING = 56,
    BRIBED = 57,
    IS_PREGNANT = 58,
    LAYING_EGG = 59,
    RIDER_CAN_PICK = 60,
    TRANSITION_SITTING = 61,
    EATING = 62,
    LAYING_DOWN = 63
};

template<typename V>
struct DataItemMap;

template<>
struct DataItemMap<bool>
{
    using item_type = ByteDataItem;
    static constexpr uint8_t type_id = 0;
};

template<>
struct DataItemMap<uint8_t>
{
    using item_type = ByteDataItem;
    static constexpr uint8_t type_id = 0;
};

template<>
struct DataItemMap<int8_t>
{
    using item_type = ByteDataItem;
    static constexpr uint8_t type_id = 0;
};

template<>
struct DataItemMap<int16_t>
{
    using item_type = ShortDataItem;
    static constexpr uint8_t type_id = 1;
};

template<>
struct DataItemMap<uint16_t>
{
    using item_type = ShortDataItem;
    static constexpr uint8_t type_id = 1;
};

template<>
struct DataItemMap<int32_t>
{
    using item_type = IntDataItem;
    static constexpr uint8_t type_id = 2;
};

template<>
struct DataItemMap<uint32_t>
{
    using item_type = IntDataItem;
    static constexpr uint8_t type_id = 2;
};

template<>
struct DataItemMap<float>
{
    using item_type = FloatDataItem;
    static constexpr uint8_t type_id = 3;
};

template<>
struct DataItemMap<double>
{
    using item_type = FloatDataItem;
    static constexpr uint8_t type_id = 3;
};

template<>
struct DataItemMap<std::string>
{
    using item_type = StringDataItem;
    static constexpr uint8_t type_id = 4;
};

template<>
struct DataItemMap<std::string_view>
{
    using item_type = StringDataItem;
    static constexpr uint8_t type_id = 4;
};

template<>
struct DataItemMap<const char*>
{
    using item_type = StringDataItem;
    static constexpr uint8_t type_id = 4;
};

template<>
struct DataItemMap<SafeString>
{
    using item_type = StringDataItem;
    static constexpr uint8_t type_id = 4;
};

template<>
struct DataItemMap<int64_t>
{
    using item_type = LongDataItem;
    static constexpr uint8_t type_id = 7;
};

template<>
struct DataItemMap<uint64_t>
{
    using item_type = LongDataItem;
    static constexpr uint8_t type_id = 7;
};

class SynchedActorData
{
public:
    PODVector<DataItem*> items; // +0x00

    [[nodiscard]] size_t size() const noexcept
    {
        return items.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return items.empty();
    }

    const DataItem* getItem(uint16_t id) const
    {
        for (const DataItem* item : items)
        {
            if (item && item->id == id)
            {
                return item;
            }
        }
        return nullptr;
    }

    DataItem* getItem(uint16_t id)
    {
        return const_cast<DataItem*>(static_cast<const SynchedActorData*>(this)->getItem(id));
    }

    bool has(uint16_t id) const
    {
        return getItem(id) != nullptr;
    }

    template<typename T>
    T get(uint16_t id, T defaultVal = {}) const
    {
        using Mapping = DataItemMap<T>;
        using ItemT = typename Mapping::item_type;
        const DataItem* item = getItem(id);
        if (item && item->type == Mapping::type_id)
        {
            return static_cast<const ItemT*>(item)->value;
        }
        return defaultVal;
    }

    std::string_view getString(uint16_t id, std::string_view defaultVal = "") const
    {
        const DataItem* item = getItem(id);
        if (item && item->type == 4)
        {
            const StringDataItem* strItem = static_cast<const StringDataItem*>(item);
            return strItem->value.view();
        }
        return defaultVal;
    }

    [[nodiscard]] uint8_t getByte(uint16_t id, uint8_t defaultVal = 0) const
    {
        return get<uint8_t>(id, defaultVal);
    }

    [[nodiscard]] int16_t getShort(uint16_t id, int16_t defaultVal = 0) const
    {
        return get<int16_t>(id, defaultVal);
    }

    [[nodiscard]] int32_t getInt(uint16_t id, int32_t defaultVal = 0) const
    {
        return get<int32_t>(id, defaultVal);
    }

    [[nodiscard]] float getFloat(uint16_t id, float defaultVal = 0.0f) const
    {
        return get<float>(id, defaultVal);
    }

    [[nodiscard]] int64_t getInt64(uint16_t id, int64_t defaultVal = 0) const
    {
        return get<int64_t>(id, defaultVal);
    }

    bool getFlag(uint32_t flagIndex) const
    {
        return flagIndex < 64 && ((getInt64(EntityDataIDs::FLAGS) & (1ULL << flagIndex)) != 0);
    }

    bool getFlag(EntityFlag flag) const
    {
        return getFlag(static_cast<uint32_t>(flag));
    }

    void setFlag(uint32_t flagIndex, bool enable)
    {
        if (flagIndex >= 64)
        {
            return;
        }
        int64_t current = getInt64(EntityDataIDs::FLAGS, 0);
        if (enable)
        {
            current |= (1ULL << flagIndex);
        }
        else
        {
            current &= ~(1ULL << flagIndex);
        }
        set(EntityDataIDs::FLAGS, current);
    }

    void setFlag(EntityFlag flag, bool enable)
    {
        setFlag(static_cast<uint32_t>(flag), enable);
    }

    template<typename ValT>
    bool set(uint16_t id, ValT&& value)
    {
        using RawValT = std::decay_t<ValT>;
        using Mapping = std::conditional_t<
            std::is_constructible_v<std::string, RawValT> && !std::is_arithmetic_v<RawValT>,
            DataItemMap<std::string>,
            DataItemMap<RawValT>
        >;
        using ItemT = typename Mapping::item_type;

        // If item already exists, update in-place
        for (DataItem* it : items)
        {
            if (it && it->id == id && it->type == Mapping::type_id)
            {
                static_cast<ItemT*>(it)->value = std::forward<ValT>(value);
                it->dirty = true;
                return true;
            }
        }

        // If item does not exist, construct a new one using harvested vtable
        void* vt = PacketInterceptor::get().getDataItemVtable(Mapping::type_id);
        if (!vt)
        {
            return false;
        }

        ItemT* newItem = new (std::nothrow) ItemT();
        if (!newItem)
        {
            return false;
        }

        newItem->vtable = vt;
        newItem->type = Mapping::type_id;
        newItem->dirty = true;
        newItem->id = id;
        newItem->value = std::forward<ValT>(value);

        items.push_back(newItem);
        return true;
    }
};

} // namespace SDK
