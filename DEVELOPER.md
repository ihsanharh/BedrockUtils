<a id="top" name="top"></a>
# BedrockUtils Maintainer Reference

Internal technical documentation for project maintainers. Covers module creation, packet pipeline routing, client commands, memory offsets, and the complete SDK API surface.

> **Target Environment**: Minecraft Bedrock Edition (Windows x64), supported version is **26.4x**.

---

<a id="quick-navigation" name="quick-navigation"></a>
## Quick Navigation

[Architecture Overview](#1-architecture-overview) · [Writing Your First Module](#2-writing-your-first-module) · [Working Example](#3-complete-working-example) · [Lifecycle Hooks](#4-module-lifecycle-hooks) · [Packet Pipeline](#5-packet-interception-and-pipeline) · [Command System](#6-client-command-system) · [SDK Reference](#7-exhaustive-sdk-reference) · [Packet Layouts](#8-supported-packets-and-memory-layouts) · [Coding Style](#9-coding-style-and-standards) · [STYLE.md](STYLE.md)

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Writing Your First Module](#2-writing-your-first-module)
3. [Complete Working Example](#3-complete-working-example)
4. [Module Lifecycle Hooks](#4-module-lifecycle-hooks)
5. [Packet Interception and Pipeline](#5-packet-interception-and-pipeline)
6. [Client Command System](#6-client-command-system)
7. [Exhaustive SDK Reference](#7-exhaustive-sdk-reference)
   - [Chat Service (`SDK::Chat`)](#sdkchat)
   - [Game and Player State (`SDK::Game`)](#sdkgame)
   - [Packet Factory (`SDK::Factory`)](#sdkfactory)
   - [Command Manager (`SDK::CommandManager`)](#sdkcommandmanager)
   - [Player Tracker (`SDK::PlayerTracker`)](#sdkplayertracker)
   - [Packet Dumper (`SDK::PacketDumper`)](#sdkpacketdumper)
   - [HTTP Client (`SDK::Http`)](#sdkhttp)
   - [Async Worker Pool (`SDK::Async`)](#sdkasync)
   - [Logging (`SDK::Log`)](#sdklog)
   - [SafeString, PODVector, and Memory Helpers (`SDK::SafeString`)](#sdksafestring)
   - [Chat Parser (`SDK::ChatUtils`)](#sdkchatutils)
   - [JSON Utilities (`SDK::Json`)](#sdkjson)
8. [Supported Packets and Memory Layouts](#8-supported-packets-and-memory-layouts)
   - [Version Offset Notice](#version-offset-notice)
   - [Base Packet Header](#base-packet-header)
   - [TextPacket (`0x09`)](#textpacket-0x09)
   - [CommandRequestPacket (`0x4D`)](#commandrequestpacket-0x4d)
   - [AddPlayerPacket (`0x0C`)](#addplayerpacket-0x0c)
   - [AddActorPacket (`0x0D`)](#addactorpacket-0x0d)
   - [RemoveActorPacket (`0x0E`)](#removeactorpacket-0x0e)
   - [MovePlayerPacket (`0x13`)](#moveplayerpacket-0x13)
   - [MoveActorAbsolutePacket (`0x12`)](#moveactorabsolutepacket-0x12)
   - [MoveActorDeltaPacket (`0x6F`)](#moveactordeltapacket-0x6f)
   - [PlayerListPacket (`0x3F`)](#playerlistpacket-0x3f)
   - [ModalFormRequestPacket (`0x64`)](#modalformrequestpacket-0x64)
   - [ModalFormResponsePacket (`0x65`)](#modalformresponsepacket-0x65)
   - [AvailableCommandsPacket (`0x4C`)](#availablecommandspacket-0x4c)
   - [ChangeDimensionPacket (`0x3D`)](#changedimensionpacket-0x3d)
   - [PlayStatusPacket (`0x02`)](#playstatuspacket-0x02)
9. [Coding Style and Standards](#9-coding-style-and-standards)

---

<a id="1-architecture-overview" name="1-architecture-overview"></a><a id="architecture-overview" name="architecture-overview"></a>
## 1. Architecture Overview

BedrockUtils operates as an in-process Man-in-the-Middle (MITM) DLL dynamically loaded into `Minecraft.Windows.exe` on Windows x64. The supported Minecraft version is **26.4x**. By hooking the network layer directly inside the game's memory space, it intercepts, inspects, modifies, and dispatches packets with zero latency without requiring external proxies or binary patching.

### Core Architectural Pillars

1. **Non-Intrusive Runtime Hooking**: Network interception is achieved through lightweight virtual table and function hooks using MinHook. Specifically, the client hooks `LoopbackPacketSender::sendToServer` for outbound traffic and the engine dispatcher for inbound traffic.
2. **Self-Registering Modules**: Modules are completely decoupled. By using the `REGISTER_MODULE(ClassName)` macro, a module registers itself into the global `ModuleRegistry` during static initialization before the client starts up.
3. **Server-Agnostic Foundation**: Core systems such as the pipeline, command dispatcher, and base SDK contain zero hardcoded server logic or server-specific heuristics. Universal categories and generic hooks ensure the codebase runs identically across singleplayer worlds, Realms, and custom servers.
4. **Resilient Memory Protection**: Because Minecraft Bedrock uses structured exception handling and custom memory allocators, all raw memory probes are isolated into pure POD leaf functions (`SafeMem.h`), allowing the entire codebase to compile cleanly under standard `/EHsc` with guaranteed C++ object stack unwinding.

[Back to Top](#quick-navigation)

---

<a id="2-writing-your-first-module" name="2-writing-your-first-module"></a><a id="2-writing-a-module" name="2-writing-a-module"></a><a id="writing-your-first-module" name="writing-your-first-module"></a>
## 2. Writing Your First Module

Creating a new module requires only two files placed inside the `src/modules/` directory: a header file (`.h`) and an implementation file (`.cpp`). CMake automatically collects all source files via `CONFIGURE_DEPENDS`, so you never need to edit build scripts or central registry rosters.

### Step 1: Declare Your Class

Create a header file inheriting from the `Module` base class.

```cpp
#pragma once
#include "modules/Module.h"

class AutoGreet : public Module
{
public:
    AutoGreet();

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;
    void onTick() override;
};
```

### Step 2: Implement and Self-Register

In your implementation file, call `REGISTER_MODULE(AutoGreet)` at file scope and define your constructor and lifecycle hooks.

```cpp
#include "AutoGreet.h"
#include "pch.h"
#include "modules/ModuleRegistry.h"
#include "sdk/Chat.h"
#include "sdk/Logger.h"

REGISTER_MODULE(AutoGreet);

AutoGreet::AutoGreet()
    : Module("auto_greet", Category::PLAYER, false, "Greets players who join the world")
{
}

void AutoGreet::onLoad()
{
    // Register commands and packet listeners here
}

void AutoGreet::onEnable()
{
    SDK::Log::log("[AutoGreet] Enabled");
    SDK::Chat::moduleNotify(name(), "AutoGreet is now active.");
}

void AutoGreet::onDisable()
{
    SDK::Log::log("[AutoGreet] Disabled");
}

void AutoGreet::onTick()
{
    // Optional logic executed on every game tick while enabled
}
```

### Module Categories

When constructing a module, categorize it under one of the universal categories defined in `src/modules/Module.h`:

* `Category::COMBAT`: Combat assists, hitboxes, target selection.
* `Category::MOVEMENT`: Flight, sprint, speed, step modifications.
* `Category::PLAYER`: Inventory tools, nametags, identity, auto-armor.
* `Category::VISUAL`: ESP, HUD overlays, tracers, freecam.
* `Category::WORLD`: Block placement, world interaction, nuker.
* `Category::EXPLOIT`: Protocol edge cases, packet timing tools.
* `Category::NETWORK`: Connection diagnostics, latency meters.
* `Category::MISC`: Commands, chat macros, translators, utility helpers.

[Back to Top](#quick-navigation)

---

<a id="3-complete-working-example" name="3-complete-working-example"></a><a id="complete-working-example" name="complete-working-example"></a>
## 3. Complete Working Example

Below is a complete, production-ready module demonstrating how to intercept inbound chat, send outbound chat to the server, execute client commands, and read player information.

### ChatMacroModule.h

```cpp
#pragma once
#include "modules/Module.h"
#include "sdk/packets/TextPacket.h"

class ChatMacroModule : public Module
{
public:
    ChatMacroModule();

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;

private:
    std::string m_customGreeting = "Hello everyone!";
};
```

### ChatMacroModule.cpp

```cpp
#include "ChatMacroModule.h"
#include "pch.h"
#include "modules/ModuleRegistry.h"
#include "sdk/Chat.h"
#include "sdk/Game.h"
#include "sdk/Logger.h"
#include <format>

REGISTER_MODULE(ChatMacroModule);

ChatMacroModule::ChatMacroModule()
    : Module("chat_macro", Category::MISC, false, "Custom chat automation and broadcast commands")
{
}

void ChatMacroModule::onLoad()
{
    // 1. Listen for inbound chat packets from other players
    listen<SDK::TextPacket>([this](TypedPacketContext<SDK::TextPacket>& ctx)
    {
        if (ctx.dir != PacketDirection::Inbound || !ctx.packet)
        {
            return;
        }

        std::string author = ctx.packet->sourceName.str();
        std::string text = ctx.packet->getMessage();

        if (text.find("!ping") != std::string::npos)
        {
            SDK::Log::log("[ChatMacro] Detected ping request from {}", author);
            SDK::Chat::notify(std::format("Player §e{}§7 requested a ping!", author));
        }
    });

    // 2. Register a client-side command to broadcast a message to the server
    CommandCallback greetCb = [this](const CommandArgs& args)
    {
        std::string messageToSend = m_customGreeting;
        if (!args.empty())
        {
            messageToSend = std::string(args.get(0));
        }

        if (SDK::Chat::sendToServer(messageToSend))
        {
            SDK::Chat::success(std::format("Broadcasted to server: §f\"{}\"", messageToSend));
        }
        else
        {
            SDK::Chat::error("Could not send chat packet (not connected to a server).");
        }
    };
    registerCommand("greet", "Broadcast your custom greeting to the server", greetCb);

    // 3. Register a command to reconfigure the module greeting in-game
    CommandCallback setGreetingCb = [this](const CommandArgs& args)
    {
        if (args.empty())
        {
            SDK::Chat::warn("Usage: ;;setgreeting <message>");
            return;
        }

        m_customGreeting = std::string(args.raw());
        SDK::Chat::notify(std::format("Greeting updated to: §e\"{}\"", m_customGreeting));
    };
    registerCommand("setgreeting", "Set custom greeting message", setGreetingCb);
}

void ChatMacroModule::onEnable()
{
    SDK::Log::log("[ChatMacro] Enabled");
    SDK::Chat::moduleNotify(name(), "Chat macro listener armed.");
}

void ChatMacroModule::onDisable()
{
    SDK::Log::log("[ChatMacro] Disabled");
}
```

[Back to Top](#quick-navigation)

---

<a id="4-module-lifecycle-hooks" name="4-module-lifecycle-hooks"></a><a id="module-lifecycle-hooks" name="module-lifecycle-hooks"></a>
## 4. Module Lifecycle Hooks

The `Module` base class provides five virtual lifecycle functions. You only need to override the hooks required for your specific functionality.

| Hook Method | When It Fires | Best Usage |
|---|---|---|
| `virtual void onLoad()` | Called exactly once when the DLL is first initialized. | Register packet listeners (`listen<T>`) and client commands (`registerCommand`). |
| `virtual void onEnable()` | Called whenever the module transitions from disabled to enabled. | Allocate runtime caches, arm timers, and notify the user via chat. |
| `virtual void onDisable()` | Called whenever the module is toggled off. | Clear runtime buffers, release state, and reset UI elements. |
| `virtual void onTick()` | Called on every game loop frame, only while the module is enabled. | Process per-tick calculations, spatial checks, or movement updates. |
| `virtual void onUnload()` | Called once when BedrockUtils is safely ejected from the game. | Release native handles or external resources. |

All pipeline handles and registered commands created by a module are tracked through RAII. When a module is unloaded or disabled, its hooks and commands are automatically detached without requiring manual cleanup loops.

[Back to Top](#quick-navigation)

---

<a id="5-packet-interception-and-pipeline" name="5-packet-interception-and-pipeline"></a><a id="packet-interception-and-pipeline" name="packet-interception-and-pipeline"></a>
## 5. Packet Interception and Pipeline

The client pipeline routes every packet passing through the game engine. Modules register type-safe packet listeners using the `listen<T>()` helper function inside `onLoad()`.

### Subscribing to Packets

```cpp
listen<SDK::TextPacket>([this](TypedPacketContext<SDK::TextPacket>& ctx)
{
    // Type-safe packet context
});
```

### Packet Context Properties

The `TypedPacketContext<T>` structure provides full control over the intercepted packet:

* `ctx.dir`: The packet direction. Can be `PacketDirection::Inbound` (server to client) or `PacketDirection::Outbound` (client to server).
* `ctx.packet`: A strongly typed pointer to the packet instance (such as `SDK::TextPacket*`).
* `ctx.dropped`: A boolean indicating whether the packet has been cancelled.
* `ctx.drop(reason)`: Cancels the packet immediately. Inbound packets will not reach the game receiver; outbound packets will not be transmitted over the wire.
* `ctx.isInbound()`: Convenience method returning true if the packet was sent by the server.
* `ctx.isOutbound()`: Convenience method returning true if the packet originated locally.

### Dropping Packets Example

```cpp
listen<SDK::TextPacket>([](TypedPacketContext<SDK::TextPacket>& ctx)
{
    if (ctx.isInbound() && ctx.packet)
    {
        std::string msg = ctx.packet->getMessage();
        if (msg.find("[Spam]") != std::string::npos)
        {
            // Drop packet so it never renders in chat
            ctx.drop("Filtered spam text");
        }
    }
});
```

[Back to Top](#quick-navigation)

---

<a id="6-client-command-system" name="6-client-command-system"></a><a id="client-command-system" name="client-command-system"></a>
## 6. Client Command System

BedrockUtils features a double-semicolon command system (`;;`). Commands typed into the game chat starting with `;;` are intercepted on the client side, executed locally, and dropped from transmission so the server never sees them.

### Registering a Command

Inside `onLoad()`, call `registerCommand` with a name, description, and callback:

```cpp
CommandCallback myCb = [](const CommandArgs& args)
{
    if (args.empty())
    {
        SDK::Chat::warn("No arguments provided.");
        return;
    }

    std::string_view first = args.get(0);
    SDK::Chat::notify(std::format("Argument 0: {}", first));
};
registerCommand("mycmd", "Explanation of what mycmd does", myCb);
```

### The `CommandArgs` Helper

The `CommandArgs` container parses user input whitespace-separated tokens:

* `args.size()`: Total number of arguments provided after the command name.
* `args.empty()`: Returns true if no arguments were given.
* `args.get(index)`: Returns the token at the given index as a `std::string_view`.
* `args.raw()`: Returns the entire unparsed argument string following the command name.

### Standard Built-In Commands

The client includes an extensive suite of core commands:

* `;;help`: Lists all registered commands with descriptions.
* `;;modules`: Displays all loaded modules, their categories, and active toggle states.
* `;;toggle <name>`: Enables or disables a module by name.
* `;;ping`: Checks responsiveness between client and mod.
* `;;players`: Prints all currently tracked players in the world.
* `;;list`: Displays a formatted roster of players with platforms and entity IDs.
* `;;player <name>`: Shows detailed telemetry for a specific player.
* `;;floatingtext <text>`: Spawns a client-side floating text entity.
* `;;trycmd <command>`: Executes a silent server command and captures the response.
* `;;chat [message]`: Broadcasts outbound chat to the server, or injects a local test message if empty.
* `;;dump <in|out|any> <id|name> [bytes]`: Arms the memory dumper for reverse engineering.
* `;;eject`: Unhooks all native functions and cleanly unloads the DLL from memory.

[Back to Top](#quick-navigation)

---

<a id="7-exhaustive-sdk-reference" name="7-exhaustive-sdk-reference"></a><a id="exhaustive-sdk-reference" name="exhaustive-sdk-reference"></a>
## 7. Exhaustive SDK Reference

All client utilities, services, and memory helpers are organized under the `SDK` namespace.

---

<a id="sdkchat" name="sdkchat"></a><a id="chat-service-sdkchat" name="chat-service-sdkchat"></a>
### Chat Service (`SDK::Chat`)

Defined in `src/sdk/Chat.h`. Provides high-level methods for displaying formatted local chat messages and broadcasting outbound packets to the server.

```cpp
#include "sdk/Chat.h"
```

#### Methods

1. `void send(std::string_view message)`
   Injects an unformatted raw text packet directly into the local client chat UI.

2. `void sendChat(std::string_view author, std::string_view message)`
   Injects a formatted chat message appearing as `<author> message` into the local client chat UI.

3. `bool sendToServer(std::string_view message)`
   Constructs a genuine outbound `TextPacket` populated with the local player's authenticated username, Xbox XUID, and variant tags, then transmits it to the server. Returns true if dispatched.

4. `void notify(std::string_view message)`
   Displays a soft-gray branded notification: `§a§l[BedrockUtils]§r §7<message>`.

5. `void success(std::string_view message)`
   Displays a green success notification: `§a§l[BedrockUtils]§r §a<message>`.

6. `void warn(std::string_view message)`
   Displays a gold warning notification: `§6§l[BedrockUtils]§r §e<message>`.

7. `void error(std::string_view message)`
   Displays a red error notification: `§c§l[BedrockUtils]§r §c<message>`.

8. `void moduleNotify(std::string_view moduleName, std::string_view message)`
   Displays a module-scoped notification: `§a[<moduleName>]§r §7<message>`.

[Back to Top](#quick-navigation)

---

<a id="sdkgame" name="sdkgame"></a><a id="game-and-player-state-sdkgame" name="game-and-player-state-sdkgame"></a>
### Game and Player State (`SDK::Game`)

Defined in `src/sdk/Game.h`. Exposes direct access to player pointers, packet senders, credentials, and connection state.

```cpp
#include "sdk/Game.h"
```

#### Methods

1. `SDK::LocalPlayer* player()`
   Returns a pointer to the local player object, or `nullptr` if not currently in a world.

2. `SDK::PacketSender* sender()`
   Returns the active game `PacketSender` instance used to transmit packets directly to the server.

3. `bool isConnected()`
   Returns true if the client is actively connected to a server, Realm, or local game session.

4. `bool isServer(std::string_view pattern)`
   Checks if the current connected server host IP, unresolved URL, or server creator matches a pattern (case-insensitive).

5. `SDK::ServerConnectionDetails connection()`
   Returns detailed information about the active connection including `hostIp`, `port`, `unresolvedUrl`, and `creatorName`.

6. `std::string getLocalPlayerName()`
   Returns the authenticated username or gamertag of the local player.

7. `std::string getLocalPlayerXuid()`
   Returns the 16-character Xbox User ID (XUID) of the local player.

[Back to Top](#quick-navigation)

---

<a id="sdkfactory" name="sdkfactory"></a><a id="packet-factory-sdkfactory" name="packet-factory-sdkfactory"></a>
### Packet Factory (`SDK::Factory`)

Defined in `src/sdk/Factory.h`. Interfaces with Minecraft Bedrock's internal packet allocator to instantiate packets containing genuine game virtual tables and serialization functions.

```cpp
#include "sdk/Factory.h"
```

#### Methods

1. `std::shared_ptr<SDK::Packet> createPacket(SDK::PacketID id)`
   Allocates a raw game packet matching the given numeric ID. Returns `nullptr` if the ID is unknown or unresolved.

2. `template<typename T> std::shared_ptr<T> create()`
   Convenience template that creates the packet matching `T::ID` and casts it to the target subclass:
   ```cpp
   std::shared_ptr<SDK::TextPacket> tp = SDK::Factory::create<SDK::TextPacket>();
   ```

[Back to Top](#quick-navigation)

---

<a id="sdkcommandmanager" name="sdkcommandmanager"></a><a id="command-manager-sdkcommandmanager" name="command-manager-sdkcommandmanager"></a>
### Command Manager (`SDK::CommandManager`)

Defined in `src/sdk/CommandManager.h`. Enables modules to dispatch server commands silently in the background and capture responses without showing command output in the user's chat.

```cpp
#include "sdk/CommandManager.h"
```

#### Dispatching a Command

```cpp
SDK::CommandRequest req;
req.command = "/list";
req.silent = true;
req.timeoutMs = 2500;

req.onComplete = [](const SDK::CommandResult& res)
{
    if (res.isText())
    {
        for (const std::string& line : res.lines)
        {
            SDK::Log::log("[ServerList] Line: {}", line);
        }
    }
    else if (res.isForm())
    {
        SDK::Log::log("[ServerList] Captured JSON Form ID {}", res.form->id);
    }
    else if (res.isTimeout())
    {
        SDK::Log::log("[ServerList] Command timed out.");
    }
};

SDK::CommandManager::get().execute(std::move(req));
```

#### Request Settings

* `req.silent`: When true, server text responses matching the command are squelched from the chat UI.
* `req.timeoutMs`: Milliseconds to wait before declaring a timeout.
* `req.batchDebounceMs`: Debounce delay for multiline server outputs.
* `req.expectedLines`: When greater than zero, finishes capturing as soon as N lines are collected.

[Back to Top](#quick-navigation)

---

<a id="sdkplayertracker" name="sdkplayertracker"></a><a id="player-tracker-sdkplayertracker" name="player-tracker-sdkplayertracker"></a>
### Player Tracker (`SDK::PlayerTracker`)

Defined in `src/services/PlayerTracker.h`. Automatically monitors player join packets, entity movement, and disconnects to maintain an accurate spatial roster of every player in the world.

```cpp
#include "services/PlayerTracker.h"
```

#### Query Methods

* `PlayerTracker::get().findPlayerByName(name)`: Returns `std::shared_ptr<TrackedPlayer>` by matching username.
* `PlayerTracker::get().findPlayerByNametag(tag)`: Returns a player by their in-world nametag.
* `PlayerTracker::get().getPlayer(runtimeId)`: Finds a player by their runtime entity ID.
* `PlayerTracker::get().getPlayerByUuid(uuid)`: Finds a player by their 128-bit UUID string.
* `PlayerTracker::get().getAllPlayerPtrs()`: Returns all players currently registered on the server roster.
* `PlayerTracker::get().getWorldPlayers()`: Returns only players currently rendered within 3D world distance.

#### Event Hooks

Register callback functions for player lifecycle events:

```cpp
PlayerTracker::get().onPlayerJoin([](std::shared_ptr<TrackedPlayer> p)
{
    SDK::Chat::notify(std::format("Player §e{}§7 connected from §f{}", p->name, p->getPlatformName()));
});

PlayerTracker::get().onPlayerSpawn([](std::shared_ptr<TrackedPlayer> p)
{
    SDK::Log::log("[Spawn] {} appeared at ({:.1f}, {:.1f}, {:.1f})", p->name, p->pos[0], p->pos[1], p->pos[2]);
});
```

#### The `TrackedPlayer` Structure

* `p->name`: Clean player username with color codes stripped.
* `p->nametag`: Full formatted nametag including prefixes, ranks, and colors.
* `p->pos`: Current 3D coordinates `float[3]`.
* `p->distanceTo(x, y, z)`: Calculates Cartesian distance to given coordinates.
* `p->distanceToLocalPlayer()`: Calculates distance to the local user.
* `p->buildPlatform`: Device platform enum (`WINDOWS_32`, `UWP`, `IOS`, `GOOGLE`, `SWITCH`, etc.).

[Back to Top](#quick-navigation)

---

<a id="sdkpacketdumper" name="sdkpacketdumper"></a><a id="packet-dumper-sdkpacketdumper" name="packet-dumper-sdkpacketdumper"></a>
### Packet Dumper (`SDK::PacketDumper`)

Defined in `src/services/PacketDumper.h`. Provides real-time binary memory dumping for reverse engineering unknown packets and verifying field offsets against live server traffic.

```cpp
#include "services/PacketDumper.h"
```

#### In-Game Command Usage

* `;;dump in 9`: Arms the latch to dump the next inbound `TextPacket` (ID `0x09`).
* `;;dump out 1`: Arms the latch to dump the next outbound `LoginPacket` (ID `0x01`).
* `;;dump any text 512`: Dumps up to 512 bytes of the next matching text packet in either direction.
* `;;dump status`: Displays active latch status and target packet details.
* `;;dump cancel`: Disarms the dumper latch.

#### Output File

Dumps are formatted with 16-byte hex tables, ASCII text columns, and heuristic structure detectors (automatically highlighting `std::string`, `SafeString`, coordinates, and variant tags). Reports are written to `butils_dumper.log` located inside `%LOCALAPPDATA%\BedrockUtils\logs`.

#### Guide: Mapping an Unknown Packet

This is the workflow for taking a packet you have never seen before and producing a working, fully typed C++ header for it.

**Step 1: Identify the packet ID.**

If you know the name (e.g. `TRANSFER`), look it up in `src/sdk/Packet.h` where all known IDs are enumerated in the `PacketID` enum. If you do not know the ID yet, enable verbose logging (`;;toggle verbose` or `SDK::Log::setVerbose(true)`) and watch `butils.log` for unhandled packet IDs flying past as you trigger the network event in-game.

**Step 2: Arm the dumper.**

Run the dump command from the in-game chat with enough bytes to capture the full struct. Start with 256 bytes; increase if the output is truncated:

```
;;dump in 55 256
```

This arms the latch for the next inbound packet with ID `0x55` (the `TRANSFER` packet in this example) and captures 256 bytes of its raw memory.

**Step 3: Trigger the packet.**

Perform the in-game action that causes the server to send this packet (accepting a server transfer prompt, entering a portal, etc.). The dumper fires once and writes the result to `butils_dumper.log`.

**Step 4: Read the hex dump.**

Open `butils_dumper.log`. You will see output like this:

```
[PacketDumper] === DUMP: INBOUND packet ID=0x55 | 256 bytes ===
Offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F   ASCII
0x0000   00 00 00 00 00 00 00 00 02 00 00 00 01 00 00 00   ................
0x0010   00 00 00 00 00 00 00 00 78 xx xx xx 00 00 00 00   ........x.......
0x0020   0F 00 00 00 0F 00 00 00 73 65 72 76 65 72 2E 68   ........server.h
0x0030   69 76 65 6D 63 2E 63 6F 6D 00 00 00 00 00 00 00   ivemk.com.......
0x0040   xx xx xx xx 13 27 00 00 00 00 00 00 00 00 00 00   .....'.........
```

The heuristic detector highlights the `SafeString` header pattern at `+0x20` (size `0x0F` = 15, res `0x0F` = 15, SSO buffer starts at `+0x28` containing `server.hivemk.com`). The `uint16_t` at `+0x44` reads as `0x2713` = `10003` which looks like a port number.

**Step 5: Map the fields.**

Work through the dump systematically. The base `Packet` header occupies `+0x00` to `+0x2F`. Your packet fields start at `+0x30`:

| Dump Offset | Raw Bytes | Interpretation |
|---|---|---|
| `+0x30` | `0F 00 00 00 0F 00 00 00 73 65 72 76 65 72...` | `SafeString` - hostname (`server.hivemk.com`) |
| `+0x50` | `13 27 00 00` | `uint16_t` - port (`10003`) |

**Step 6: Write the header.**

Create `src/sdk/packets/TransferPacket.h` following the same pattern as existing packets. Add `static_assert` checks for every field offset you measured:

```cpp
#pragma once
#include "sdk/Packet.h"
#include "sdk/SafeString.h"
#include <cstdint>

namespace SDK
{

class TransferPacket : public Packet
{
public:
    static constexpr PacketID ID = PacketID::TRANSFER;

    SafeString address; // +0x30 (server hostname or IP)
    uint16_t port;      // +0x50 (server port)

    virtual ~TransferPacket() = default;

    PacketID getID() const override
    {
        return PacketID::TRANSFER;
    }
};

static_assert(offsetof(TransferPacket, address) == 0x30, "address offset mismatch");
static_assert(offsetof(TransferPacket, port)    == 0x50, "port offset mismatch");

} // namespace SDK
```

**Step 7: Verify with a listener.**

Add a temporary listener in any module and log the parsed fields. If the values match what the server sent, the layout is correct:

```cpp
listen<SDK::TransferPacket>([](TypedPacketContext<SDK::TransferPacket>& ctx)
{
    if (ctx.isInbound() && ctx.packet)
    {
        SDK::Log::log("[Transfer] Server transferring to {}:{}", ctx.packet->address.str(), ctx.packet->port);
    }
});
```

If values look garbled, re-examine the dump at adjacent offsets or increase the dump size and repeat from Step 3.

[Back to Top](#quick-navigation)

---

<a id="sdkhttp" name="sdkhttp"></a>
### HTTP Client (`SDK::Http`)

Defined in `src/sdk/Http.h`. Performs synchronous HTTP and HTTPS requests using Windows WinINet with automatic fallback to `curl.exe` if WinINet fails or returns a non-success status.

```cpp
#include "sdk/Http.h"
```

#### `SDK::Http::HttpResponse` Structure

Every request function returns an `HttpResponse`:

* `bool success`: True if the HTTP status code is in the 200-299 range.
* `uint32_t statusCode`: The raw HTTP status code (200, 404, 500, etc.).
* `std::string body`: The raw response body text.

#### Request Functions

1. `SDK::Http::HttpResponse SDK::Http::get(std::string_view url, uint32_t timeoutMs = 5000, const std::vector<std::string>& headers = {})`
   Performs an HTTP GET request and returns the response.

2. `SDK::Http::HttpResponse SDK::Http::post(std::string_view url, std::string_view body, std::string_view contentType = "application/x-www-form-urlencoded", uint32_t timeoutMs = 5000, const std::vector<std::string>& headers = {})`
   Performs an HTTP POST request with the given body and content type.

3. `SDK::Http::HttpResponse SDK::Http::postJson(std::string_view url, std::string_view jsonPayload, uint32_t timeoutMs = 5000, const std::vector<std::string>& headers = {})`
   Shorthand for a POST request with `Content-Type: application/json`.

4. `SDK::Http::HttpResponse SDK::Http::request(std::string_view method, std::string_view url, std::string_view body = "", std::string_view contentType = "", uint32_t timeoutMs = 5000, const std::vector<std::string>& headers = {})`
   Base function for any arbitrary HTTP method.

5. `SDK::Http::UrlComponents SDK::Http::parseUrl(std::string_view rawUrl)`
   Parses a URL string into host, port, path, and SSL flag components.

All HTTP calls are **synchronous**. Always wrap them inside `SDK::Async::run` (see below) to avoid blocking the game render thread.

[Back to Top](#quick-navigation)

---

<a id="sdkasync" name="sdkasync"></a>
### Async Worker Pool (`SDK::Async`)

Defined in `src/sdk/Async.h`. Provides a C++20 `std::jthread`-backed worker pool to run arbitrary callables off the game render thread.

```cpp
#include "sdk/Async.h"
```

#### Functions

1. `template<typename F, typename... Args> std::future<std::invoke_result_t<F, Args...>> SDK::Async::run(F&& f, Args&&... args)`
   Submits a callable and its arguments to the background thread pool. Returns a `std::future` containing the eventual result.

2. `void SDK::Async::init()`
   Starts the worker threads (2 to 4 threads based on hardware concurrency). Called automatically on first use.

3. `void SDK::Async::shutdown()`
   Signals all worker threads to stop and waits for them to finish. Called during DLL ejection.

#### Usage Examples

Running an HTTP request off the render thread:

```cpp
SDK::Async::run([]()
{
    SDK::Http::HttpResponse res = SDK::Http::get("https://api.example.com/status");
    if (res.success)
    {
        SDK::Log::log("[MyModule] Response {}: {}", res.statusCode, res.body);
    }
});
```

Running a delayed notification using `std::this_thread::sleep_for`:

```cpp
SDK::Async::run([]()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    SDK::Chat::notify("Two seconds have passed.");
});
```

Capturing a future result:

```cpp
std::future<int> fut = SDK::Async::run([](int a, int b) { return a + b; }, 10, 20);
// fut.get() returns 30, but only block on it from another background thread
```

[Back to Top](#quick-navigation)

---

<a id="sdklog" name="sdklog"></a>
### Logging (`SDK::Log`)

Defined in `src/sdk/Logger.h`. Writes timestamped log entries simultaneously to `stdout`, `butils.log` on disk, and `OutputDebugStringA` for attached debuggers.

```cpp
#include "sdk/Logger.h"
```

#### Functions

1. `template<typename... Args> void SDK::Log::log(std::format_string<Args...> fmt, Args&&... args)`
   Standard operational logging. Use for lifecycle transitions, errors, and important state changes. Prefix each message with the module name in brackets.

   ```cpp
   SDK::Log::log("[MyModule] Enabled, greeting = {}", m_greeting);
   ```

2. `template<typename... Args> void SDK::Log::logDebug(std::format_string<Args...> fmt, Args&&... args)`
   High-frequency diagnostic logging. Output is suppressed unless verbose mode is active. Use for per-packet or per-tick events.

   ```cpp
   SDK::Log::logDebug("[MyModule] Tick pos=({:.1f}, {:.1f})", x, y);
   ```

3. `void SDK::Log::setVerbose(bool enable)`
   Enables or disables verbose mode globally.

4. `bool SDK::Log::isVerbose()`
   Returns true if verbose logging is currently enabled.

5. `void SDK::Log::flush()`
   Flushes `stdout` and the log file to disk. Do not call inside hot packet loops.

6. `std::string SDK::Log::getLogDirectory()`
   Returns the resolved writable log directory path (e.g. `C:\Users\User\AppData\Local\BedrockUtils\logs`).

[Back to Top](#quick-navigation)

---

<a id="sdksafestring" name="sdksafestring"></a>
### SafeString, PODVector, and Memory Helpers (`SDK::SafeString`)

Defined in `src/sdk/SafeString.h`. Provides structs that precisely mirror MSVC's `std::string` and `std::vector` binary layout so that packet fields can be read and written without CRT heap mismatches.

```cpp
#include "sdk/SafeString.h"
```

#### `SDK::SafeString`

Matches the 32-byte MSVC `std::string` layout. Strings up to 15 characters are stored in an inline SSO buffer. Longer strings use a heap pointer. All packet string fields use this type.

* `std::string SafeString::str() const`: Returns a safe `std::string` copy of the content.
* `std::string_view SafeString::view() const`: Returns a non-owning `std::string_view`.
* `const char* SafeString::c_str() const`: Returns the null-terminated raw pointer.
* `size_t SafeString::length() const`, `bool SafeString::empty() const`
* `void SafeString::assign(std::string_view sv)`: Assigns new content, allocating heap memory with capacity aligned to `(size | 0x0F)` so Minecraft's internal `free()` can safely reclaim the buffer.

#### `SDK::PODVector<T>`

Matches the 24-byte MSVC `std::vector` layout (`_Myfirst`, `_Mylast`, `_Myend` pointers). Packet fields like `parameters` and `entries` use this type.

* `size_t size() const`, `bool empty() const`
* `const T& operator[](size_t idx) const`
* `T* begin()`, `T* end()` - supports range-for loops.
* `void push_back(const T& val)`

#### Structured Exception (SEH) Safe Read Helpers

For reading packet fields that may point into protected or page-guarded memory:

* `bool SDK::safeReadString(const void* strAddr, std::string& out)`: SEH-guarded copy of a `SafeString` at a raw address.
* `bool SDK::safeReadVectorString(const void* vecAddr, size_t index, std::string& out)`: SEH-guarded read of a `PODVector<SafeString>` element by index.

#### String Utility Functions

* `std::string SDK::stripColorCodes(std::string_view input)`: Removes `§` Minecraft color and formatting codes from any string.
* `std::string SDK::cleanPlayerName(std::string_view input)`: Strips color codes, newlines, and common bracketed guild or rank tags.
* `std::string SDK::uuidToString(const uint8_t uuid[16])`: Converts a 16-byte raw UUID array to the standard `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` string form.
* `std::string_view SDK::trim(std::string_view sv)`: Trims leading and trailing ASCII whitespace.
* `std::string SDK::toLower(std::string_view input)`: Returns a lowercase copy.
* `std::string SDK::toUpper(std::string_view input)`: Returns an uppercase copy.

[Back to Top](#quick-navigation)

---

<a id="sdkchatutils" name="sdkchatutils"></a>
### Chat Parser (`SDK::ChatUtils`)

Defined in `src/sdk/ChatUtils.h`. Extracts clean author names and message text from `TextPacket` across vanilla, Realms, and custom server chat formats.

```cpp
#include "sdk/ChatUtils.h"
```

#### `SDK::ParsedChat` Structure

* `std::string author`: Clean player gamertag with color codes and rank tags stripped.
* `std::string cleanMessage`: The message body with formatting removed.
* `std::string rawMessage`: The original unmodified message string.

#### Functions

1. `SDK::ParsedChat SDK::ChatUtils::parse(const SDK::TextPacket* packet)`
   Automatically detects the packet type (`TRANSLATION`, `CHAT`, `RAW`, `SYSTEM`) and extracts clean author and message content. Handles `<Author> Message`, `Author: Message`, `Author » Message`, and translation parameter formats.

   ```cpp
   listen<SDK::TextPacket>([](TypedPacketContext<SDK::TextPacket>& ctx)
   {
       if (ctx.isInbound() && ctx.packet)
       {
           SDK::ParsedChat parsed = SDK::ChatUtils::parse(ctx.packet);
           SDK::Log::log("[Chat] {}: {}", parsed.author, parsed.cleanMessage);
       }
   });
   ```

2. `std::string SDK::ChatUtils::stripPrefixDelimiters(std::string_view text)`
   Strips leading delimiter characters (`:`, `>`, `»`, `→`, `▶`, and similar UTF-8 arrows) from a string and trims whitespace.

3. `bool SDK::ChatUtils::hasTranslatableText(std::string_view text)`
   Returns true if the string contains ASCII letters or multi-byte UTF-8 characters (i.e. is non-empty and not purely punctuation or symbols).

[Back to Top](#quick-navigation)

---

<a id="sdkjson" name="sdkjson"></a>
### JSON Utilities (`SDK::Json`)

Defined in `src/sdk/JsonUtils.h`. Lightweight embedded JSON utilities for parsing modal form payloads and constructing JSON strings without a full DOM library.

```cpp
#include "sdk/JsonUtils.h"
```

#### Functions

1. `std::string SDK::Json::unescapeJsonString(std::string_view str)`
   Decodes standard JSON escape sequences (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, and `\uXXXX` Unicode points). The Minecraft section sign `\u00A7` is converted to `§`.

2. `std::string SDK::Json::escapeJson(std::string_view str)`
   Encodes a raw string into a valid JSON string value (escaping quotes, backslashes, and control characters).

3. `std::string SDK::Json::extractJsonString(std::string_view json, std::string_view key)`
   Extracts the value of a string field by key name without allocating a full JSON DOM. Useful for pulling specific fields from `ModalFormRequestPacket::formData`.

4. `std::vector<std::string> SDK::Json::extractFormButtons(std::string_view json)`
   Extracts all button label strings from a Bedrock `simple_form` or `form` JSON body.

   ```cpp
   listen<SDK::ModalFormRequestPacket>([](TypedPacketContext<SDK::ModalFormRequestPacket>& ctx)
   {
       if (ctx.isInbound() && ctx.packet)
       {
           std::string json = ctx.packet->formData.str();
           std::string title = SDK::Json::extractJsonString(json, "title");
           std::vector<std::string> buttons = SDK::Json::extractFormButtons(json);
           SDK::Log::log("[Form] Title: {}, Buttons: {}", title, buttons.size());
       }
   });
   ```

[Back to Top](#quick-navigation)

---

<a id="8-supported-packets-and-memory-layouts" name="8-supported-packets-and-memory-layouts"></a><a id="supported-packets-and-memory-layouts" name="supported-packets-and-memory-layouts"></a>
## 8. Supported Packets and Memory Layouts

All packet headers are located under `src/sdk/packets/`.

<a id="version-offset-notice" name="version-offset-notice"></a>

> [!WARNING]
> **Version Offset Notice**: All struct field offsets documented here were reverse engineered for **Minecraft Bedrock Edition (Windows x64), supported version 26.4x**. Mojang frequently shifts data structures across updates by adding fields, changing padding, or reordering members. When upgrading to a new Minecraft version, always verify offsets using the compile-time `static_assert` checks in each packet header and the in-game packet dumper (`;;dump`) before relying on any field.

---

<a id="base-packet-header" name="base-packet-header"></a>
### Base Packet Header

All packet types inherit from `SDK::Packet`. The first 0x30 bytes are shared by every packet:

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x00` | `vtable` | `void**` | Virtual method table pointer |
| `+0x08` | `priority` | `int32_t` | Transmission priority (default 2) |
| `+0x0C` | `reliability` | `int32_t` | Network reliability enum (default 1) |
| `+0x10` | `subClientId` | `uint8_t` | Split-screen client index (default 0) |
| `+0x11` | `isHandled` | `bool` | Processing completion flag |
| `+0x20` | `handler` | `void***` | Engine dispatcher vtable pointer |
| `+0x28` | `compressibility` | `int32_t` | Compression eligibility flag |

Packet-specific fields begin at `+0x30`.

---

<a id="textpacket-0x09" name="textpacket-0x09"></a>
### TextPacket (`0x09`)

Direction: Bidirectional. Carries all chat, broadcasts, whispers, and system notifications.

Include: `sdk/packets/TextPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x38` | `xuid` | `SafeString` | 16-digit Xbox User ID of sender |
| `+0x58` | `platformChatId` | `SafeString` | Platform-specific chat identifier |
| `+0x78` | `parameters` | `PODVector<SafeString>` | Translation parameter arguments |
| `+0xA0` | `type` | `TextPacketType` | Packet type: `RAW=0`, `CHAT=1`, `TRANSLATION=2`, `SYSTEM=6` |
| `+0xA1` | `needsTranslation` | `bool` | Whether client should translate the message |
| `+0xA8` | `sourceName` | `SafeString` | Author gamertag |
| `+0xC8` | `message` | `SafeString` | Primary message text payload |
| `+0xE8` | `variantIndex` | `uint8_t` | Active union variant: `0`=raw, `1`=chat, `2`=params |

The `getMessage()` helper resolves the correct field based on `variantIndex` and `type`. Prefer `SDK::ChatUtils::parse(packet)` for extracting clean author and message fields.

---

<a id="commandrequestpacket-0x4d" name="commandrequestpacket-0x4d"></a>
### CommandRequestPacket (`0x4D`)

Direction: Outbound. Dispatched when the client executes a server command.

Include: `sdk/packets/CommandRequestPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `command` | `SafeString` | Command string with leading slash (e.g. `/help`) |
| `+0x50` | `origin` | `CommandOriginData` | Command execution origin context |
| `+0x68` | `origin.requestId` | `SafeString` | Internal request tracking UUID |
| `+0x90` | `version` | `uint32_t` | Command protocol version (default `50`) |

---

<a id="addplayerpacket-0x0c" name="addplayerpacket-0x0c"></a>
### AddPlayerPacket (`0x0C`)

Direction: Inbound. Received when another player enters render distance in the 3D world.

Include: `sdk/packets/AddPlayerPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `uuid` | `uint8_t[16]` | Player 128-bit UUID (use `getUuidString()` helper) |
| `+0x40` | `username` | `SafeString` | Raw username or nametag header |
| `+0x60` | `runtimeEntityId` | `int64_t` | Runtime entity identifier |
| `+0x68` | `platformChatId` | `SafeString` | Platform chat identifier |
| `+0x88` | `pos` | `float[3]` | Spawn coordinates (X, Y, Z) |
| `+0x94` | `motion` | `float[3]` | Initial velocity vector |
| `+0xA0` | `rotation` | `float[3]` | Pitch, Yaw, HeadYaw |
| `+0x118` | `entityData` | `SynchedActorData` | Synced actor metadata (name, scale, flags) |
| `+0x1A0` | `deviceId` | `SafeString` | Hardware device identifier |
| `+0x1C0` | `buildPlatform` | `BuildPlatform` | Client platform enum (8 = `WINDOWS_32`) |

The `BuildPlatform` enum values: `GOOGLE=1`, `IOS=2`, `OSX=3`, `AMAZON=4`, `UWP=7`, `WINDOWS_32=8`, `DEDICATED=9`, `SONY=11`, `NX=12`, `XBOX=13`, `LINUX=15`.

The `getNametag()` helper reads the `NAME` entity data item. The `getUuidString()` helper formats the raw UUID bytes.

---

<a id="addactorpacket-0x0d" name="addactorpacket-0x0d"></a>
### AddActorPacket (`0x0D`)

Direction: Inbound. Received when a non-player entity (mob, projectile, vehicle) enters render distance.

Include: `sdk/packets/AddActorPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `uniqueEntityId` | `int64_t` | Server-assigned unique entity ID |
| `+0x38` | `runtimeEntityId` | `int64_t` | Runtime entity identifier |
| `+0x40` | `identifier` | `SafeString` | Entity type string (e.g. `minecraft:zombie`) |
| `+0x60` | `pos` | `float[3]` | Spawn coordinates (X, Y, Z) |
| `+0x6C` | `motion` | `float[3]` | Initial velocity vector |
| `+0x78` | `rotation` | `float[2]` | Pitch, Yaw |
| `+0x80` | `headRotation` | `float` | Head yaw angle |
| `+0x84` | `bodyRotation` | `float` | Body yaw angle |
| `+0xA0` | `entityData` | `SynchedActorData` | Synchronized entity metadata |

---

<a id="removeactorpacket-0x0e" name="removeactorpacket-0x0e"></a>
### RemoveActorPacket (`0x0E`)

Direction: Inbound. Received when an entity leaves render distance or despawns.

Include: `sdk/packets/RemoveActorPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `uniqueEntityId` | `int64_t` | Unique entity ID of the removed actor |

---

<a id="moveplayerpacket-0x13" name="moveplayerpacket-0x13"></a>
### MovePlayerPacket (`0x13`)

Direction: Bidirectional. Updates a player's position, rotation, and ground collision state.

Include: `sdk/packets/MovePlayerPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `runtimeEntityId` | `int64_t` | Runtime entity identifier |
| `+0x38` | `pos` | `float[3]` | Position coordinates (X, Y, Z) |
| `+0x44` | `pitch` | `float` | Camera pitch angle |
| `+0x48` | `yaw` | `float` | Body yaw angle |
| `+0x4C` | `headYaw` | `float` | Head yaw angle |
| `+0x50` | `mode` | `uint8_t` | Movement mode (0=Normal, 2=Teleport) |
| `+0x51` | `onGround` | `bool` | Ground collision flag |
| `+0x58` | `ridingRuntimeId` | `int64_t` | Mount entity runtime ID |
| `+0x60` | `cause` | `int32_t` | Movement cause code |
| `+0x68` | `tick` | `int64_t` | Server simulation tick |

---

<a id="moveactorabsolutepacket-0x12" name="moveactorabsolutepacket-0x12"></a>
### MoveActorAbsolutePacket (`0x12`)

Direction: Inbound. Sends absolute position for non-player entities.

Include: `sdk/packets/MoveActorAbsolutePacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `runtimeEntityId` | `int64_t` | Runtime entity identifier |
| `+0x38` | `flags` | `uint8_t` | Teleport and orientation bitflags |
| `+0x3C` | `pos` | `float[3]` | Absolute position (X, Y, Z) |
| `+0x48` | `pitch` | `uint8_t` | Byte-quantized pitch angle |
| `+0x49` | `yaw` | `uint8_t` | Byte-quantized yaw angle |
| `+0x4A` | `headYaw` | `uint8_t` | Byte-quantized head yaw angle |

---

<a id="moveactordeltapacket-0x6f" name="moveactordeltapacket-0x6f"></a>
### MoveActorDeltaPacket (`0x6F`)

Direction: Inbound. High-frequency delta or absolute position update for moving entities.

Include: `sdk/packets/MoveActorDeltaPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `runtimeEntityId` | `int64_t` | Runtime entity identifier |
| `+0x38` | `flags` | `uint16_t` | Bitflags indicating which axes are present |
| `+0x3C` | `pos` | `float[3]` | Delta or absolute position (X, Y, Z) |
| `+0x48` | `pitch` | `uint8_t` | Quantized pitch angle |
| `+0x49` | `yaw` | `uint8_t` | Quantized yaw angle |
| `+0x4A` | `headYaw` | `uint8_t` | Quantized head yaw angle |

---

<a id="playerlistpacket-0x3f" name="playerlistpacket-0x3f"></a>
### PlayerListPacket (`0x3F`)

Direction: Inbound. Server roster update containing arrays of joining or leaving players.

Include: `sdk/packets/PlayerListPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `entries` | `PODVector<Entry>` | Array of player list entries |

Each `PlayerListPacket::Entry` in the `entries` vector has the following layout:

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x00` | `action` | `uint8_t` | Action flag: `0` = ADD, `1` = REMOVE |
| `+0x08` | `uuid` | `uint8_t[16]` | 128-bit player UUID (use `getUuidString()`) |
| `+0x18` | `entityId` | `int64_t` | Server-assigned unique entity ID |
| `+0x20` | `name` | `SafeString` | Player username |
| `+0x40` | `xuid` | `SafeString` | 16-digit Xbox User ID |
| `+0x60` | `platformOnlineId` | `SafeString` | Platform online gamertag |
| `+0x80` | `buildPlatform` | `uint32_t` | Platform identifier (see `BuildPlatform` enum) |

---

<a id="modalformrequestpacket-0x64" name="modalformrequestpacket-0x64"></a>
### ModalFormRequestPacket (`0x64`)

Direction: Inbound. Sent by the server to display an interactive JSON form in the game UI.

Include: `sdk/packets/ModalFormRequestPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `formId` | `uint32_t` | Server-assigned form tracking ID |
| `+0x38` | `formData` | `SafeString` | Raw JSON form definition payload |

Use `SDK::Json::extractJsonString(formData.str(), "title")` and `SDK::Json::extractFormButtons(formData.str())` to read form contents.

---

<a id="modalformresponsepacket-0x65" name="modalformresponsepacket-0x65"></a>
### ModalFormResponsePacket (`0x65`)

Direction: Outbound. Sent by the client when a player submits or closes a modal form.

Include: `sdk/packets/ModalFormResponsePacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `formId` | `uint32_t` | Target form identifier |
| `+0x38` | `responseData` | `SafeString` | Selected button index or JSON response string |

---

<a id="availablecommandspacket-0x4c" name="availablecommandspacket-0x4c"></a>
### AvailableCommandsPacket (`0x4C`)

Direction: Inbound. Synchronizes the server's registered command definitions with the client parser.

Include: `sdk/packets/AvailableCommandsPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `enumValues` | `PODVector<SafeString>` | Enum value strings for command arguments |
| `+0x48` | `postFixes` | `PODVector<SafeString>` | Postfix strings for command completions |
| `+0xA8` | `commands` | `PODVector<CommandData>` | Registered server command definitions |

Each `CommandData` entry exposes `getName()` and `getDescription()` helpers, along with `flags` and `permission` bytes.

The `hasCommand(std::string_view name)` method on the packet checks if a specific command exists in the list.

---

<a id="changedimensionpacket-0x3d" name="changedimensionpacket-0x3d"></a>
### ChangeDimensionPacket (`0x3D`)

Direction: Inbound. Sent when the player transitions between dimensions.

Include: `sdk/packets/ChangeDimensionPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `dimension` | `int32_t` | Target dimension: `0`=Overworld, `1`=Nether, `2`=The End |
| `+0x34` | `pos` | `float[3]` | Target spawn coordinates (X, Y, Z) |
| `+0x40` | `respawn` | `bool` | Respawn state flag |

---

<a id="playstatuspacket-0x02" name="playstatuspacket-0x02"></a>
### PlayStatusPacket (`0x02`)

Direction: Inbound. Sent during the connection handshake and world spawning sequence.

Include: `sdk/packets/PlayStatusPacket.h`

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `status` | `PlayStatusType` | Status code (see enum below) |

The `PlayStatusType` enum:

| Value | Name | Meaning |
|---|---|---|
| `0` | `LOGIN_SUCCESS` | Login accepted, world loading starts |
| `1` | `LOGIN_FAILED_CLIENT_OLD` | Client version too old |
| `2` | `LOGIN_FAILED_SERVER_OLD` | Server version too old |
| `3` | `PLAYER_SPAWN` | Player is fully spawned and ready |
| `4` | `LOGIN_FAILED_INVALID_TENANT` | Invalid tenant (education mismatch) |
| `7` | `FAILED_SERVER_FULL_SUB_CLIENT` | Server is full |

[Back to Top](#quick-navigation)

---

<a id="9-coding-style-and-standards" name="9-coding-style-and-standards"></a><a id="coding-style-and-standards" name="coding-style-and-standards"></a><a id="style-guide" name="style-guide"></a>
## 9. Coding Style and Standards

All code in this repository must strictly adhere to the project conventions documented in [STYLE.md](STYLE.md).

Key requirements to keep in mind when writing or reviewing code:

1. **Formatting**: Allman brace style for non-empty blocks, and inline `{}` on the same line for empty blocks. 4 spaces per indentation level.
2. **Explicit Typing**: Strict prohibition of the `auto` keyword. Every variable, container iterator, and loop target must state its type explicitly.
3. **Pointers & References**: Left-aligned to the type (`Packet* pkt`, `const std::string& ref`).
4. **Server-Agnostic Core Architecture**: Core classes, base SDK headers, and engine abstractions must remain 100% server-agnostic.
5. **Git Commits**: Conventional Commits specification (`feat`, `fix`, `refactor`, `chore`, `docs`, `style`).

Read the complete [STYLE.md](STYLE.md) document for exhaustive details and examples.

[Back to Top](#quick-navigation)
