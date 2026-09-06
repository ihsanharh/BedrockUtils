# Coding Style & Architecture Guidelines

This repository (`BedrockUtils`) follows strict C++20 conventions and design principles. All contributors (human or AI) must adhere to the following rules.

---

## 1. Formatting & Brace Style

- **Allman Style for Non-Empty Blocks**: The opening curly brace `{` must always be placed on a new line, matching the indentation of the preceding statement.
  ```cpp
  // Correct (Allman style for non-empty blocks)
  void MyModule::onLoad()
  {
      if (isEnabled())
      {
          doSomething();
      }
  }

  // INCORRECT (K&R style - DO NOT USE)
  void MyModule::onLoad() {
      if (isEnabled()) {
          doSomething();
      }
  }
  ```

- **Empty Braces on the Same Line (`{}`)**: Whenever a block, function body, loop, `catch`, `__except`, constructor, or destructor is empty, the braces **must be placed together on the same line**: `{}`.
  ```cpp
  // Correct (inline empty braces)
  virtual void onLoad() {}
  virtual void onEnable() {}
  virtual void onDisable() {}
  virtual ~MyClass() = default;
  catch (const std::exception&) {}
  __except (EXCEPTION_EXECUTE_HANDLER) {}

  // INCORRECT (Never split empty braces across multiple lines):
  virtual void onLoad()
  {
  }

  catch (const std::exception&)
  {
  }
  ```

- **Indentation**: 4 spaces per indentation level. Do not use tabs.
- **Pointer & Reference Alignment**: Left-aligned to the type (`Type* name`, `const Type& ref`).
- **Control Statements**: Never put single-line `if`, `for`, or `while` statements on the same line. Always use full braces:
  ```cpp
  // Correct
  if (!pkt)
  {
      return;
  }

  // INCORRECT
  if (!pkt) return;
  ```

---

## 2. Server-Agnostic Core Architecture

- **Zero Server-Specific Leaks**: Core abstractions (`AppCore`, `Module.h`, `Pipeline`, `CommandDispatcher`, `PlayerTracker`, and SDK classes) must **NEVER** contain server-specific enums, names, heuristics, or hardcoded logic.
  - Examples of violations: `Category::HIVE`, `Category::CUBECRAFT`, or hardcoded server IPs in core headers.
  - All categories must be universal (e.g. `COMBAT`, `MOVEMENT`, `PLAYER`, `VISUAL`, `WORLD`, `EXPLOIT`, `NETWORK`, `MISC`).
- **Domain Isolation**: Any server-specific feature (e.g. The Hive, CubeCraft) must be isolated completely within its own optional module directory (e.g. `src/modules/hive/`). If that folder is deleted, the entire codebase must continue to compile and run universally without a single broken dependency.

---

## 3. DRY Principle (Don't Repeat Yourself)

- Eliminate duplicate logic, boilerplate, and redundant packet parsing.
- Centralize shared utilities in `sdk/util/` or helper methods on the base `Module` class (such as `listen<T>()` and `registerCommand()`).
- Re-use established patterns and abstractions rather than re-implementing them.

---

## 4. Naming Conventions

- **Types (Classes, Structs, Enums, Type Aliases)**: `PascalCase`
  - Examples: `Module`, `PacketContext`, `ModuleRegistry`, `CommandArgs`
- **Functions & Methods**: `camelCase`
  - Examples: `onLoad()`, `onEnable()`, `registerCommand()`, `findModule()`
- **Member Variables**: `m_` prefix + `camelCase`
  - Examples: `m_name`, `m_enabled`, `m_pipelineHandles`, `m_description`
- **Static Class Variables**: `s_` prefix + `camelCase`
  - Examples: `s_instance`, `s_origSendToServer`
- **Global Variables**: `g_` prefix + `camelCase`
  - Examples: `g_isRunning`, `g_lastCheckpoint`
- **Constants & Enum Enumerators**: `UPPER_SNAKE_CASE` or `kCamelCase`
  - Examples: `PacketID::TEXT`, `Category::MISC`, `kAnyPacket`, `kMaxId`
- **Local Variables & Parameters**: `camelCase`
  - Examples: `commandName`, `packetContext`, `targetLang`

---

## 5. Header & Implementation Separation

- **Header Files (`.h`)**:
  - Class declarations, interface definitions, member variables, and trivial inline accessors (`[[nodiscard]] const std::string& name() const noexcept`).
  - Use `#pragma once` as the header guard.
  - Forward declare types wherever possible to minimize compilation times and header pollution.
  - Include only headers strictly necessary for type definitions.
- **Source Files (`.cpp`)**:
  - Implementation of non-trivial methods (`void MyModule::onLoad() { ... }`).
  - Module self-registration macro (`REGISTER_MODULE(MyModule);`).
  - Implementation-specific helper functions wrapped in anonymous namespaces or marked `static`.

---

## 6. Comments & Documentation

- **Quality Over Quantity**: Write comments that explain **why** something is done, not **what** the code does when the code is already self-explanatory.
  ```cpp
  // INCORRECT (Redundant / Noise):
  // Check if packet is null
  if (!packet)
  {
      return; // Return if null
  }

  // CORRECT (Explains rationale / non-obvious details):
  // MinHook requires the sendToServer hook to read vtable[2] from the live PacketSender instance.
  uintptr_t sendToServerAddr = vtable[2];
  ```
- Document memory layouts, binary structure offsets, SEH requirements, or packet protocol nuances.
- Keep comments concise, technical, and accurate.

---

## 7. Modern C++20 Standards & Explicit Typing

- **Strict Prohibition of `auto` Keyword**: **NEVER use the `auto` keyword.** Always state the explicit type for variables, loop targets, and container iterators.
  ```cpp
  // Correct (Explicit types)
  const std::vector<std::unique_ptr<Module>>& modules = ModuleRegistry::get().all();
  for (const std::unique_ptr<Module>& m : modules)
  for (const std::pair<const std::string, RegisteredCommand>& entry : m_commands)
  std::unordered_map<std::string, RegisteredCommand>::iterator it = m_commands.find(cmdName);
  std::string clean = SDK::stripColorCodes(raw);

  // INCORRECT (Never use auto)
  const auto& modules = ModuleRegistry::get().all();
  for (const auto& m : modules)
  for (const auto& [k, v] : m_commands)
  auto it = m_commands.find(cmdName);
  auto clean = SDK::stripColorCodes(raw);
  ```
- Pass read-only string parameters by `std::string_view` where feasible.
- Mark non-mutating query methods with `[[nodiscard]]` and `const`.
- Use RAII for all resource management (smart pointers, mutex locks, pipeline handles).
- Avoid raw heap allocations (`new`/`delete`); use `std::make_unique` or `std::make_shared`.

---

## 8. Consistent & Verbose Logging Standards

- **Standard Prefixing**: Always tag log lines with the component or module name in square brackets:
  ```cpp
  SDK::Log::log("[ModuleName] Message here...");
  SDK::Log::log("[Network] Connected to server: {}", address);
  ```
- **Verbose vs. Standard Logging**:
  - Use `SDK::Log::log(...)` for lifecycle transitions, critical state changes, and error reporting.
  - Use `SDK::Log::logDebug(...)` for high-frequency packet events, repetitive tick outputs, or verbose diagnostics.
  - High-frequency events (e.g. logging every move packet or per-tick calculations) must **NEVER** flood the standard log file un-gated.
- **Flushing**: Do not call `SDK::Log::flush()` inside hot packet loops. Flushing is handled automatically during idle periods or by crash handlers.

---

## 9. In-Game Text & Color Formatting Standards

- **Color Palette & Semantics**:
  - `§a§l[BedrockUtils]§r`: Primary brand header (Green Bold).
  - `§a[<ModuleName>]§r`: Module-scoped header.
  - `§7`: Default body text (Soft Gray - prevents visual clutter in chat).
  - `§f` / `§e`: Highlights, command names, arguments, values (White / Yellow).
  - `§a`: Success confirmation (Green).
  - `§6` / `§e`: Warning messages (Gold / Yellow).
  - `§c`: Error messages (Red).
  - `§8`: Separators and brackets (Dark Gray, e.g. ` §8- §7`).
- **Formatting Reset Rule**: Always append `§r` immediately after bold or colored prefixes (e.g. `§a§l[BedrockUtils]§r`) to prevent color and font styles from bleeding into the rest of the message.
- **Use `SDK::Chat` Helper Methods (DRY)**:
  Avoid manually concatenating brand prefixes. Use the centralized helpers in `sdk/Chat.h`:
  ```cpp
  SDK::Chat::notify("Operation completed successfully.");
  SDK::Chat::success("Module enabled.");
  SDK::Chat::warn("Missing required argument: <name>");
  SDK::Chat::error("Command execution failed.");
  SDK::Chat::moduleNotify(name(), "Toggled to new mode.");
  ```
  Only use raw `SDK::Chat::send(...)` for custom multi-line formatted tables (such as the `;;modules` roster).

---

## 10. Commit Message Style

Use the **Conventional Commits** format. This keeps the history readable and makes it easy to understand what changed and why at a glance.

```
<type>(<scope>): <short summary>
```

- The summary should be lowercase, imperative, and under 72 characters.
- The scope is optional but encouraged — use the component or module name.
- Add a body (blank line after summary) if the change needs more context.

### Types

| Type | When to use |
|---|---|
| `feat` | A new feature or module |
| `fix` | A bug fix |
| `refactor` | Code restructured without changing behavior |
| `chore` | Build system, toolchain, CI, or dependency changes |
| `docs` | Documentation only |
| `style` | Formatting, brace style, naming — no logic changes |
| `perf` | Performance improvement |
| `revert` | Reverting a previous commit |

### Examples

```
feat(core): add CommandDispatcher double-slash interception
fix(interceptor): guard against null clientCb before inbound flush
refactor(pipeline): replace linear scan with O(1) indexed dispatch
chore(ci): add windows-latest build workflow
docs: add STYLE.md commit message guidelines
style(CoreCommands): fix Allman brace style on empty catch blocks
```

### Rules

- **Never** use vague messages like `fix stuff`, `update`, or `wip`.
- If a commit fixes a specific crash or regression, say what it was: `fix(chat): prevent ACCESS_VIOLATION when player not yet spawned`.
- Breaking changes should be noted in the body: `BREAKING CHANGE: Module::onLoad() no longer receives PacketSender.`

---

## 11. Exception Handling & Memory Safety (SEH)

- **Standard C++ Exception Model (`/EHsc`)**: The codebase strictly uses `/EHsc`. Do **not** reintroduce asynchronous exception models (`/EHa`).
- **SEH in Leaf Functions Only**: `__try` / `__except` blocks must **never** be placed in functions that declare or contain local C++ objects requiring stack unwinding / destruction (MSVC error `C2712`).
- **Centralized Safe Dereferencing**: Always use `SDK::Memory::read()` or `SDK::Memory::readValue()` from `sdk/SafeMem.h` for safe memory probing, pointer-chasing, and vtable inspection.
- **`SafeString` Standard Capacity Bounds**: Any heap allocation for game `std::string` fields must round capacity to `(size | 0x0F)` and allocate `capacity + 1` bytes via `HeapAlloc`, matching MSVC STL standard allocator invariants.

---

## 12. Inbound Packet Injection & Thread Safety

- **Live Network Thread Execution**: Injected inbound packets (such as test chat, command replies, or client-side entity spawns) must **only** be flushed on the live network thread inside `trapInbound()`.
- **Zero Background-Thread Dispatch**: Never invoke `flushInbound()` or call Minecraft's `ClientNetworkHandler` / `PacketDispatcher::handle` directly from background worker threads (such as `AppCore::tick()`). Minecraft's client packet handling and chat UI subsystem are strictly non-thread-safe.

