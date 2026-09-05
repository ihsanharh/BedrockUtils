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
   - [HTTP and Async Services (`SDK::Http`, `SDK::Async`)](#sdkhttp-and-sdkasync)
   - [Logging (`SDK::Log`)](#sdklog)
   - [SafeString and Utility Helpers (`SDK::SafeString`)](#sdksafestring-and-utilities)
8. [Supported Packets and Memory Layouts](#8-supported-packets-and-memory-layouts)
   - [TextPacket (`0x09`)](#textpacket-0x09)
   - [CommandRequestPacket (`0x4D`)](#commandrequestpacket-0x4d)
   - [AddPlayerPacket (`0x0C`)](#addplayerpacket-0x0c)
   - [PlayerListPacket (`0x3F`)](#playerlistpacket-0x3f)
9. [Coding Style and Standards](#9-coding-style-and-standards)

---

<a id="1-architecture-overview" name="1-architecture-overview"></a><a id="architecture-overview" name="architecture-overview"></a>
## 1. Architecture Overview

BedrockUtils operates as an in-process Man-in-the-Middle (MITM) DLL dynamically loaded into `Minecraft.Windows.exe` on Windows x64. The supported Minecraft version is **26.4x**. By hooking the network layer directly inside the game's memory space, it intercepts, inspects, modifies, and dispatches packets with zero latency without requiring external proxies or binary patching.

### Core Architectural Pillars

1. **Non-Intrusive Runtime Hooking**: Network interception is achieved through lightweight virtual table and function hooks using MinHook. Specifically, the client hooks `LoopbackPacketSender::sendToServer` for outbound traffic and the engine dispatcher for inbound traffic.
2. **Self-Registering Modules**: Modules are completely decoupled. By using the `REGISTER_MODULE(ClassName)` macro, a module registers itself into the global `ModuleRegistry` during static initialization before the client starts up.
3. **Server-Agnostic Foundation**: Core systems such as the pipeline, command dispatcher, and base SDK contain zero hardcoded server logic or server-specific heuristics. Universal categories and generic hooks ensure the codebase runs identically across singleplayer worlds, Realms, and custom servers.
4. **Resilient Memory Protection**: Because Minecraft Bedrock uses structured exception handling and custom memory allocators, all packet reads and critical hooks are guarded by structured exception handling (`__try` and `__except`) to guarantee client stability.

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

[Back to Top](#quick-navigation)

---

<a id="sdkhttp-and-sdkasync" name="sdkhttp-and-sdkasync"></a><a id="http-and-async-services-sdkhttp-sdkasync" name="http-and-async-services-sdkhttp-sdkasync"></a>
### HTTP and Async Services (`SDK::Http`, `SDK::Async`)

Defined in `src/sdk/Http.h` and `src/sdk/Async.h`.

```cpp
#include "sdk/Http.h"
#include "sdk/Async.h"
```

#### Asynchronous HTTP Requests

Execute web requests in background worker threads without freezing the game render loop:

```cpp
SDK::Http::getAsync("https://api.example.com/status", [](const SDK::HttpResponse& res)
{
    if (res.isSuccess())
    {
        SDK::Log::log("[HTTP] Received response: {}", res.body);
    }
});
```

#### Scheduled Task Execution

Run background work or schedule a function to run after a delay:

```cpp
SDK::Async::runDelayed(1000, []()
{
    SDK::Chat::notify("One second has elapsed.");
});
```

[Back to Top](#quick-navigation)

---

<a id="sdklog" name="sdklog"></a><a id="logging-sdklog" name="logging-sdklog"></a>
### Logging (`SDK::Log`)

Defined in `src/sdk/Logger.h`. Provides timestamped logging written directly to `butils.log`.

```cpp
#include "sdk/Logger.h"
```

#### Methods

* `SDK::Log::log("[ModuleName] Message here: {}", arg)`: Standard lifecycle and operational logging.
* `SDK::Log::logDebug("[ModuleName] Verbose event: {}", arg)`: Diagnostic logging for high-frequency packet ticks.

[Back to Top](#quick-navigation)

---

<a id="sdksafestring-and-utilities" name="sdksafestring-and-utilities"></a><a id="safestring-and-utilities-sdksafestring" name="safestring-and-utilities-sdksafestring"></a>
### SafeString and Utilities (`SDK::SafeString`)

Defined in `src/sdk/SafeString.h`. Implements memory-safe string abstractions matching MSVC standard library binary layout.

```cpp
#include "sdk/SafeString.h"
```

#### Useful Helper Functions

* `SDK::stripColorCodes(str)`: Removes Minecraft formatting codes (`§a`, `§l`, `§r`).
* `SDK::cleanPlayerName(str)`: Strips colors, newlines, and bracketed guild/rank tags from nametags.
* `SDK::uuidToString(uuidBytes)`: Converts a 16-byte buffer into a formatted UUID string (`xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`).
* `SDK::toLower(str)`: Returns a lowercase copy of the string.
* `SDK::toUpper(str)`: Returns an uppercase copy of the string.
* `SDK::trim(str)`: Removes leading and trailing whitespace.

[Back to Top](#quick-navigation)

---

<a id="8-supported-packets-and-memory-layouts" name="8-supported-packets-and-memory-layouts"></a><a id="supported-packets-and-memory-layouts" name="supported-packets-and-memory-layouts"></a>
## 8. Supported Packets and Memory Layouts

All packet headers are located under `src/sdk/packets/`.

<a id="textpacket-0x09" name="textpacket-0x09"></a><a id="textpacket" name="textpacket"></a>
### TextPacket (`0x09`)

Used for all player chat, broadcasts, whispers, and system notifications across both directions.

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x38` | `xuid` | `SafeString` | 16-digit Xbox User ID |
| `+0x58` | `platformChatId` | `SafeString` | Platform-specific chat identifier |
| `+0x78` | `parameters` | `PODVector<SafeString>` | Translation parameter arguments |
| `+0xA0` | `type` | `TextPacketType` | Packet type enum (`RAW=0`, `CHAT=1`, `SYSTEM=6`, etc.) |
| `+0xA1` | `needsTranslation` | `bool` | Translation requirement flag |
| `+0xA8` | `sourceName` | `SafeString` | Author gamertag |
| `+0xC8` | `message` | `SafeString` | Primary message text payload |
| `+0xE8` | `variantIndex` | `uint8_t` | Variant active tag (`0` = raw, `1` = chat, `2` = params) |

<a id="commandrequestpacket-0x4d" name="commandrequestpacket-0x4d"></a><a id="commandrequestpacket" name="commandrequestpacket"></a>
### CommandRequestPacket (`0x4D`)

Outbound packet dispatched when running server commands.

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `command` | `SafeString` | Command string including leading slash (`/help`) |
| `+0x50` | `origin` | `CommandOriginData` | Command execution context |
| `+0x68` | `origin.requestId`| `SafeString` | Internal tracking ID |
| `+0x90` | `version` | `uint32_t` | Command protocol version (default 50) |

<a id="addplayerpacket-0x0c" name="addplayerpacket-0x0c"></a><a id="addplayerpacket" name="addplayerpacket"></a>
### AddPlayerPacket (`0x0C`)

Inbound packet received when another player enters render distance.

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x38` | `uuid` | `SafeString` | Player UUID string |
| `+0x58` | `username` | `SafeString` | Player username |
| `+0x78` | `entityId` | `int64_t` | Unique entity identifier |
| `+0x80` | `runtimeId` | `int64_t` | Runtime entity identifier |
| `+0x98` | `pos` | `float[3]` | Spatial coordinates (X, Y, Z) |

<a id="playerlistpacket-0x3f" name="playerlistpacket-0x3f"></a><a id="playerlistpacket" name="playerlistpacket"></a>
### PlayerListPacket (`0x3F`)

Server roster update packet containing arrays of connected players.

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0x30` | `entries` | `PODVector<Entry>` | Array of connected player entries |

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
