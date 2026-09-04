# BedrockUtils

A modular DLL utility framework for **Minecraft Bedrock Edition (Windows x64)**. It hooks into the game's packet layer at runtime, giving you a clean foundation to intercept, inspect, and modify packets — both inbound and outbound — without patching the binary.

Modules are self-registering and fully isolated. Adding a new one is just dropping a `.cpp` file.

---

## Building

### Windows (MSVC)

```bash
cmake -B build
cmake --build build --config Release
```

### Linux (Cross-compile with clang-cl + xwin)

You'll need an MSVC sysroot. The easiest way to get one is [xwin](https://github.com/Jake-Shadle/xwin):

```bash
cargo install xwin
xwin --accept-license splat --output ~/msvc_sysroot
```

Then:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
cmake --build build
```

The toolchain resolves the sysroot automatically in this order:
1. `-DXWIN_DIR=/path` on the command line
2. `XWIN_DIR` environment variable
3. `~/Projects/msvc_sysroot` or `~/msvc_sysroot`

### CI

Every push automatically builds on `windows-latest` via GitHub Actions. The resulting `BedrockUtils.dll` is uploaded as an artifact on every successful run.

---

## Usage

1. Build or grab `BedrockUtils.dll` from the latest Actions run.
2. Inject into `Minecraft.Windows.exe` with any DLL injector.
3. The mod initializes and hooks automatically on attach.

---

## Disclaimer

For educational and research purposes only. Use responsibly.
