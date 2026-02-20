# Porthole

Run Windows games on macOS Apple Silicon via Wine + DXVK + MoltenVK.

Porthole builds [CrossOver 26](https://www.codeweavers.com/crossover) Wine (based on Wine 11.0) as x86_64, runs it under Rosetta 2, and translates DirectX calls through DXVK (D3D11 -> Vulkan) and MoltenVK (Vulkan -> Metal).

Currently targets 32-bit Windows games like Civilization IV: Beyond the Sword.

## Prerequisites

- macOS on Apple Silicon (M1/M2/M3/M4)
- Rosetta 2 (`softwareupdate --install-rosetta`)
- CrossOver 26.0.0 source tarball extracted to `sources/wine/`

## Project Structure

```
porthole/
├── build_x86_64.sh         # Build Wine from source (x86_64 via Rosetta)
├── run_wine.sh             # Launch Wine with correct env vars
├── install_dxvk.sh         # Install DXVK DLLs into Wine prefix
├── sources/
│   └── wine/               # CrossOver/Wine source tree
├── build/
│   └── wine-x86_64-build/  # Out-of-source build directory
├── install-x86_64/         # Built Wine binaries (DESTDIR)
│   └── usr/local/bin/wine
├── dxvk/
│   ├── x32/                # 32-bit DXVK DLLs (d3d11.dll, d3d10core.dll)
│   └── x64/                # 64-bit DXVK DLLs
├── x86brew/                # Isolated x86_64 Homebrew (build dependencies)
├── steamcmd/               # SteamCMD + installed game files
└── tests/                  # DComp test programs
```

## Setting Up the x86_64 Homebrew Toolchain

All build dependencies live in an isolated x86_64 Homebrew prefix at `x86brew/`, completely separate from the system ARM64 Homebrew at `/opt/homebrew`. This prefix should never be added to your shell PATH.

```bash
# Bootstrap x86_64 Homebrew into the project directory
arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" -- --prefix="$(pwd)/x86brew"

# Install build dependencies
arch -x86_64 x86brew/bin/brew install mingw-w64 bison flex freetype gnutls sdl2

# Install MoltenVK (Vulkan -> Metal translation)
arch -x86_64 x86brew/bin/brew install molten-vk
```

## Building Wine

```bash
./build_x86_64.sh
```

This script:
1. Re-executes itself under `arch -x86_64` if needed (Rosetta)
2. Sets PATH/CFLAGS/LDFLAGS to use only the x86brew toolchain
3. Configures Wine with `--enable-archs=i386,x86_64` (WoW64 for 32-bit games)
4. Builds with `make -j$(nproc)` and installs to `install-x86_64/`

Key configure flags: `--with-vulkan --with-coreaudio --with-freetype --with-gnutls --with-sdl --without-x`

The build uses `/bin/bash` explicitly (not `bash`) because Homebrew's bash is ARM64-only.

### Rebuilding a Single DLL

After modifying Wine source (e.g. `sources/wine/dlls/dcomp/`):

```bash
cd build/wine-x86_64-build
arch -x86_64 make dlls/dcomp/all
arch -x86_64 make install-lib DESTDIR=/path/to/porthole/install-x86_64
```

If you changed a `Makefile.in`, regenerate the build system first:

```bash
cd build/wine-x86_64-build
arch -x86_64 ./config.status
```

## Installing DXVK

DXVK translates Direct3D 11/10 calls to Vulkan. Pre-built DLLs are in `dxvk/`.

```bash
./install_dxvk.sh
```

This copies the DLLs into the Wine prefix (`~/.porthole-wine`) and sets registry overrides so Wine loads them instead of its built-in d3d11/d3d10core.

## Running Programs

```bash
# Run any Windows executable
./run_wine.sh path/to/program.exe

# Run with Wine debug output
WINEDEBUG=+dcomp ./run_wine.sh path/to/program.exe

# Quick smoke test
./run_wine.sh notepad
```

`run_wine.sh` sets up the runtime environment:
- `WINEPREFIX=~/.porthole-wine`
- `DYLD_FALLBACK_LIBRARY_PATH` pointing to MoltenVK, freetype, gnutls, sdl2
- `VK_ICD_FILENAMES` pointing to MoltenVK's Vulkan ICD
- `WINEDLLOVERRIDES=d3d11,d3d10core=n` to use DXVK

## Running Steam / Games

```bash
# First run: creates the Wine prefix and installs Steam
./run_wine.sh "$HOME/.porthole-wine/drive_c/Program Files (x86)/Steam/steam.exe"
```

Steam's UI renders as a black window (CEF/Chromium compositing issue - see HANDOVER.md). Workaround for login: type username, Tab, password, Enter blindly.

To install games without the GUI, use SteamCMD:

```bash
./run_wine.sh steamcmd/steamcmd.exe +login <username> +app_update <appid> +quit
```

To launch a game through Steam:

```bash
./run_wine.sh "$HOME/.porthole-wine/drive_c/Program Files (x86)/Steam/steam.exe" -applaunch <appid>
```

## Running Tests

```bash
# DComp COM tests (works over SSH, no display needed)
x86_64-w64-mingw32-gcc -o tests/test_dcomp_minimal.exe tests/test_dcomp_minimal.c -lole32 -luuid
./run_wine.sh tests/test_dcomp_minimal.exe

# Full DComp test with rendering (needs local display session)
x86_64-w64-mingw32-gcc -o tests/test_dcomp.exe tests/test_dcomp.c -ld3d11 -ldxgi -lole32 -luuid -lgdi32 -luser32
./run_wine.sh tests/test_dcomp.exe
```

## Architecture

```
Windows Game (.exe)
    │
    ▼
Wine (x86_64, Rosetta 2)
    │
    ├── D3D11 calls ──► DXVK (d3d11.dll) ──► Vulkan ──► MoltenVK ──► Metal
    ├── Audio ──► CoreAudio
    ├── Input ──► SDL2
    └── Window management ──► Wine Mac driver
```

## Current Status

- Wine GUI works (notepad, wine-mono, Mac driver)
- DXVK + MoltenVK rendering pipeline works
- Steam installs and authenticates (black window, blind login)
- SteamCMD works for game downloads
- Civ IV: Beyond the Sword runs (32-bit game, some audio stutter)
- DComp implementation in progress to fix Steam's black window (see HANDOVER.md)
