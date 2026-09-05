# Module Development Guide

This document covers everything you need to know to write a module for BedrockUtils.

---

## Basics

A module is just a C++ class that extends `Module`. Drop its `.cpp` and `.h` files anywhere under `src/modules/` — CMake picks them up automatically via `CONFIGURE_DEPENDS`. No central registry file needs editing.

**MyFeature.h**
```cpp
#pragma once
#include "modules/Module.h"

class MyFeature : public Module
{
public:
    MyFeature();

    void onLoad()    override;
    void onEnable()  override;
    void onDisable() override;
    void onUnload()  override;
};
```

**MyFeature.cpp**
```cpp
#include "MyFeature.h"
#include "pch.h"
#include "modules/ModuleRegistry.h"
#include "sdk/Chat.h"
#include "sdk/Logger.h"

REGISTER_MODULE(MyFeature);

MyFeature::MyFeature()
    : Module("my_feature", Category::MISC, true, "Does something cool") {}

void MyFeature::onLoad()
{
    // register packet listeners and commands here
}

void MyFeature::onEnable()
{
    SDK::Log::log("[MyFeature] Enabled");
}

void MyFeature::onDisable()
{
    SDK::Log::log("[MyFeature] Disabled");
}

void MyFeature::onUnload()
{
    SDK::Log::log("[MyFeature] Unloaded");
    // listeners and commands are cleaned up automatically
}
```

---

## Lifecycle Hooks

| Hook | When it fires |
|---|---|
| `onLoad()` | Once when the module is first registered at startup. Register listeners and commands here. |
| `onEnable()` | Every time the module is enabled (including at startup if enabled by default). |
| `onDisable()` | Every time the module is toggled off. |
| `onUnload()` | Once on DLL eject. All listeners and commands are auto-cleaned after this. |
| `onTick()` | Called on every game tick, only while the module is enabled. |

---

## Packet Listeners

Use `listen<T>()` inside `onLoad()` to subscribe to a specific mapped packet type. The listener is automatically skipped (no overhead) when the module is disabled.

```cpp
void MyFeature::onLoad()
{
    listen<SDK::TextPacket>([this](TypedPacketContext<SDK::TextPacket>& ctx)
    {
        // fires for every inbound AND outbound TextPacket
        if (ctx.dir == PacketDirection::Inbound && ctx.packet)
        {
            std::string msg = ctx.packet->getMessage();
            SDK::Log::log("[MyFeature] Chat message: {}", msg);
        }
    });
}
```

To filter direction, check `ctx.dir`:
```cpp
ctx.dir == PacketDirection::Inbound   // server → client
ctx.dir == PacketDirection::Outbound  // client → server
```

To drop a packet so it never reaches its destination:
```cpp
ctx.drop("reason shown in log");
```

### Mapped Packet Types

These are already defined in `src/sdk/packets/`:

| Class | ID | Direction |
|---|---|---|
| `SDK::TextPacket` | `0x09` | Both |
| `SDK::AddPlayerPacket` | `0x0C` | S→C |
| `SDK::AddActorPacket` | `0x0D` | S→C |
| `SDK::RemoveActorPacket` | `0x0E` | S→C |
| `SDK::MoveActorAbsolutePacket` | `0x12` | S→C |
| `SDK::MovePlayerPacket` | `0x13` | Both |
| `SDK::ChangeDimensionPacket` | `0x3D` | S→C |
| `SDK::PlayerListPacket` | `0x3F` | S→C |
| `SDK::AvailableCommandsPacket` | `0x4C` | S→C |
| `SDK::CommandRequestPacket` | `0x4D` | C→S |
| `SDK::TransferPacket` | `0x55` | S→C |
| `SDK::ModalFormRequestPacket` | `0x64` | S→C |
| `SDK::ModalFormResponsePacket` | `0x65` | C→S |
| `SDK::MoveActorDeltaPacket` | `0x6F` | S→C |

---

## Listening to Unmapped Packets

If a packet you need isn't in the SDK yet, you can listen by raw `PacketID` and read its fields manually from memory offsets.

```cpp
void MyFeature::onLoad()
{
    // 0x27 is just an example — replace with the actual packet ID
    listen(static_cast<SDK::PacketID>(0x27), [](PacketContext& ctx)
    {
        if (ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }

        const uint8_t* base = reinterpret_cast<const uint8_t*>(ctx.packet);

        // Try reading a SafeString at a known offset
        std::string value;
        if (SDK::safeReadString(base + 0x30, value) && !value.empty())
        {
            SDK::Log::log("[MyFeature] Unknown packet 0x27 string: {}", value);
        }

        // Read a raw integer at an offset
        __try
        {
            int32_t someInt = *reinterpret_cast<const int32_t*>(base + 0x40);
            SDK::Log::log("[MyFeature] Unknown packet 0x27 int: {}", someInt);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    });
}
```

> **Tip:** Always wrap raw memory reads in `__try / __except` or use `SDK::safeReadString`. Packet layouts can vary and a bad read will crash.

Once you've mapped the fields, add a proper struct in `src/sdk/packets/` following the same layout as the existing ones, then switch to `listen<YourNewPacket>()`.

---

## Registering Commands

Use `registerCommand()` inside `onLoad()`. The command is automatically unregistered when the module unloads, and requires the module to be enabled to execute (configurable via flags).

```cpp
void MyFeature::onLoad()
{
    registerCommand("hello", "Say hello in chat", [](const CommandArgs& args)
    {
        if (args.empty())
        {
            SDK::Chat::notify("Hello, world!");
            return;
        }

        std::string name = std::string(args.get(0));
        SDK::Chat::notify(std::format("Hello, {}!", name));
    });
}
```

Invoked by the user as `;;hello` or `;;hello Ihsan`.

`CommandArgs` helpers:
```cpp
args.empty()        // true if no arguments
args.size()         // number of arguments
args.get(0)         // get argument at index (returns string_view, empty if out of range)
args.rawCommand     // the full raw input string
args.commandName    // the command name without prefix
```

---

## Chat & Logging

### In-game chat (client-side only, never sent to server)

```cpp
SDK::Chat::notify("Something happened");              // §a§l[BedrockUtils]§r §7...
SDK::Chat::success("Operation complete");             // §a§l[BedrockUtils]§r §a...
SDK::Chat::warn("Something looks wrong");             // §6§l[BedrockUtils]§r §e...
SDK::Chat::error("Something failed");                 // §c§l[BedrockUtils]§r §c...
SDK::Chat::moduleNotify(name(), "Module-scoped msg"); // §a[my_feature]§r §7...
SDK::Chat::send("§eCustom §fformatted §7message");    // raw, no prefix
```

### File logging (`butils.log`)

```cpp
SDK::Log::log("[MyFeature] Player joined: {}", playerName);      // always written
SDK::Log::logDebug("[MyFeature] Raw offset value: {:#x}", val);  // only when verbose mode is on
```

Always prefix log lines with `[ModuleName]` so they're easy to grep.

---

## Categories

Pick the one that best fits your module:

| Category | Use for |
|---|---|
| `Category::COMBAT` | Hit detection, reach, combat stats |
| `Category::MOVEMENT` | Speed, flight, position spoofing |
| `Category::PLAYER` | Player info, friends, rosters |
| `Category::VISUAL` | ESP, rendering, HUD overlays |
| `Category::WORLD` | Chunk data, entity world events |
| `Category::EXPLOIT` | Packet manipulation, protocol tricks |
| `Category::NETWORK` | Raw network inspection, latency |
| `Category::MISC` | Anything that doesn't fit elsewhere |

---

## Full Example

A module that logs every inbound chat message and adds a `;;lastseen` command:

```cpp
// src/modules/player/ChatLogger.cpp
#include "ChatLogger.h"
#include "pch.h"
#include "modules/ModuleRegistry.h"
#include "sdk/Chat.h"
#include "sdk/Logger.h"
#include "sdk/SafeString.h"
#include "sdk/packets/TextPacket.h"
#include <format>

REGISTER_MODULE(ChatLogger);

ChatLogger::ChatLogger()
    : Module("chat_logger", Category::NETWORK, false, "Logs all inbound chat to butils.log") {}

void ChatLogger::onLoad()
{
    listen<SDK::TextPacket>([this](TypedPacketContext<SDK::TextPacket>& ctx)
    {
        if (ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }

        std::string msg = ctx.packet->getMessage();
        std::string src = ctx.packet->sourceName.str();

        if (!msg.empty())
        {
            SDK::Log::logDebug("[ChatLogger] <{}> {}", src, msg);
            m_lastSender = src;
        }
    });

    registerCommand("lastseen", "Show the last chat sender", [this](const CommandArgs&)
    {
        if (m_lastSender.empty())
        {
            SDK::Chat::warn("No chat seen yet.");
            return;
        }
        SDK::Chat::notify(std::format("Last sender: §f{}", m_lastSender));
    });
}

void ChatLogger::onEnable()
{
    SDK::Log::log("[ChatLogger] Enabled");
}

void ChatLogger::onDisable()
{
    SDK::Log::log("[ChatLogger] Disabled");
}

void ChatLogger::onUnload()
{
    SDK::Log::log("[ChatLogger] Unloaded");
}
```
