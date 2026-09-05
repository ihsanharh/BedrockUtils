#pragma once
#include "sdk/DataItem.h"
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace SDK
{

enum class BuildPlatform : int32_t
{
    UNKNOWN = 0,
    GOOGLE = 1, // Android
    IOS = 2,    // iOS
    OSX = 3,    // macOS
    AMAZON = 4, // Kindle / FireTV
    GEAR_VR = 5,
    HOLOLENS = 6,
    UWP = 7,        // Windows 10/11 Store
    WINDOWS_32 = 8, // Windows Win32 x86/x64
    DEDICATED = 9,  // Dedicated server
    TV_OS = 10,     // Apple TV
    SONY = 11,      // PlayStation
    NX = 12,        // Nintendo Switch
    XBOX = 13,      // Xbox One / Series
    WINDOWS_PHONE = 14,
    LINUX = 15,
};

class AddPlayerPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::ADD_PLAYER;

    struct AbilityLayer
    {
        uint16_t layerType;     // +0x00 Layer type (0=CACHE, 1=BASE, 2=SPECTATOR, 3=COMMANDS, 4=EDITOR)
        uint16_t pad;           // +0x02
        uint32_t abilitiesSet;  // +0x04 Enabled ability flags bitset
        uint32_t abilityValues; // +0x08 Active ability value flags
        float flySpeed;         // +0x0C Fly speed
        float walkSpeed;        // +0x10 Walk speed
        float verticalFlySpeed; // +0x14 Vertical fly speed
    };

    uint8_t uuid[16];          // +0x30 Player UUID (16 bytes)
    SafeString username;       // +0x40 Player raw username / nametag header string
    int64_t runtimeEntityId;   // +0x60 Runtime entity ID
    SafeString platformChatId; // +0x68 Platform chat ID
    float pos[3];              // +0x88 Position (X, Y, Z)
    float motion[3];           // +0x94 Motion vector (X, Y, Z)
    float rotation[3];         // +0xA0 Pitch, Yaw, HeadYaw
    uint8_t pad0[4];           // +0xAC Alignment padding

    // Embedded ItemStack in hand (+0xB0 to +0x118, sizeof = 0x68 = 104 bytes)
    struct CItemStack
    {
        void** vtable;   // +0xB0
        void* itemPtr;   // +0xB8
        void* tagPtr;    // +0xC0
        uint16_t count;  // +0xC8
        uint16_t aux;    // +0xCA
        uint8_t pad[76]; // +0xCC to +0x118
    } item;

    SynchedActorData entityData;    // +0x118 Synched actor data items (FLAGS, NAME, SCALE, etc.)
    uint8_t playerAbilitiesPad[64]; // +0x130 Player abilities pad (uniqueEntityId at +0x160, permissions at +0x168)
    uint8_t abilityLayers[24];      // +0x170 Ability layers (speed, fly, permissions) POD buffer
    uint8_t entityLinks[24];        // +0x188 Entity rider links POD buffer
    SafeString deviceId;            // +0x1A0 Device identifier
    BuildPlatform buildPlatform;    // +0x1C0 Client build platform (8 = WINDOWS_32)
    uint8_t unmapped[256];          // +0x1C4 Allocator safety buffer

    // Helpers
    [[nodiscard]] std::string_view getNametag() const
    {
        return entityData.getString(EntityDataIDs::NAME);
    }

    [[nodiscard]] std::string getUuidString() const
    {
        return SDK::uuidToString(uuid);
    }

    [[nodiscard]] int64_t getUniqueEntityId() const noexcept
    {
        // uniqueEntityId is located at offset +0x160 (+0x30 bytes into playerAbilitiesPad)
        const int64_t* pUid = reinterpret_cast<const int64_t*>(playerAbilitiesPad + 0x30);
        return *pUid;
    }

    virtual ~AddPlayerPacket() = default;

    [[nodiscard]] PacketID getID() const override
    {
        return PacketID::ADD_PLAYER;
    }
};

static_assert(offsetof(AddPlayerPacket, uuid) == 0x30, "uuid offset mismatch");
static_assert(offsetof(AddPlayerPacket, username) == 0x40, "username offset mismatch");
static_assert(offsetof(AddPlayerPacket, runtimeEntityId) == 0x60, "runtimeEntityId offset mismatch");
static_assert(offsetof(AddPlayerPacket, platformChatId) == 0x68, "platformChatId offset mismatch");
static_assert(offsetof(AddPlayerPacket, pos) == 0x88, "pos offset mismatch");
static_assert(offsetof(AddPlayerPacket, motion) == 0x94, "motion offset mismatch");
static_assert(offsetof(AddPlayerPacket, rotation) == 0xA0, "rotation offset mismatch");
static_assert(offsetof(AddPlayerPacket, item) == 0xB0, "item offset mismatch");
static_assert(offsetof(AddPlayerPacket, entityData) == 0x118, "entityData offset mismatch");
static_assert(offsetof(AddPlayerPacket, playerAbilitiesPad) == 0x130, "playerAbilitiesPad offset mismatch");
static_assert(offsetof(AddPlayerPacket, abilityLayers) == 0x170, "abilityLayers offset mismatch");
static_assert(offsetof(AddPlayerPacket, entityLinks) == 0x188, "entityLinks offset mismatch");
static_assert(offsetof(AddPlayerPacket, deviceId) == 0x1A0, "deviceId offset mismatch");
static_assert(offsetof(AddPlayerPacket, buildPlatform) == 0x1C0, "buildPlatform offset mismatch");

} // namespace SDK
