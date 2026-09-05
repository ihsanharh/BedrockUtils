# BedrockUtils

BedrockUtils is an internal mod DLL for **Minecraft Bedrock Edition (Windows x64)** (supported version is **26.4x**). It operates as an in-process Man-in-the-Middle (MITM) that hooks directly into the game's network layer to intercept, inspect, modify, and dispatch packets in real time without external proxies or binary patching.

Every module inside the client is self-contained and self-registering. Adding or extending functionality is as simple as creating a single C++ source file.

For the complete guide on writing modules, packet structures, memory layouts, coding standards, and the full SDK reference, see the [Developer Documentation](DEVELOPER.md).

---

## Building the Project

### On Windows using Visual Studio

```bash
cmake -B build
cmake --build build --config Release
```

### Cross-compiling on Linux with Clang and xwin

Cross-compilation requires an MSVC sysroot. You can obtain one easily using [xwin](https://github.com/Jake-Shadle/xwin):

```bash
cargo install xwin
xwin --accept-license splat --output ~/msvc_sysroot
```

Then configure and compile using the included toolchain file:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
cmake --build build
```

The toolchain automatically locates the sysroot via command-line arguments (`-DXWIN_DIR=/path`), the `XWIN_DIR` environment variable, or default directory locations like `~/Projects/msvc_sysroot`.

### Continuous Integration

Every commit triggers an automated build on `windows-latest` via GitHub Actions. The compiled `BedrockUtils.dll` binary is published as a downloadable workflow artifact on every run.

---

## How to Run

1. Build or download the latest `BedrockUtils.dll` binary.
2. Launch Minecraft Bedrock Edition.
3. Inject the DLL into `Minecraft.Windows.exe` using any standard DLL injector.
4. Open the in-game chat and type `;;help` to see the roster of available commands.

---

## Documentation

Full documentation, code snippets, architecture details, and SDK references are available in [DEVELOPER.md](DEVELOPER.md).

---

## Disclaimer

This project is created for educational and reverse-engineering research purposes. Please use it responsibly.
