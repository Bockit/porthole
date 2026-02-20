# Porthole: Build Plan

## Goal

Run Windows games (including 32-bit games like Civilization IV) from Steam on macOS (Apple Silicon M1).

## Problem Statement

- **Whisky** (the app we were modeling after) is discontinued and Steam no longer works with it
- **GPTK** (Apple's Game Porting Toolkit) does not support 32-bit games
- **wine-crossover** from Gcenx (Homebrew) is stuck on CrossOver 23.7.1 (Feb 2024)
- **CrossOver 25/26** has Steam fixes but costs $64/year
- Steam's `chrome_elf.dll` / CEF component fails with error: `"chrome_elf.dll" failed to initialize, aborting`

## Current State

We have wine-crossover 23.7.1 installed via Homebrew. Steam installs and updates but crashes when `steamwebhelper.exe` (Chromium Embedded Framework) tries to start.

### Error Details
```
err:module:LdrInitializeThunk "chrome_elf.dll" failed to initialize, aborting
err:module:LdrInitializeThunk Initializing dlls for "steamwebhelper.exe" failed, status 80000003
```

### What We Tried
- `-no-cef-sandbox` flag — didn't fix it
- `-allosarches -cef-force-32bit` flags — didn't fix it
- `winetricks vcrun2019` — installed but didn't fix it
- Deleting steamwebhelper — Steam redownloads it, still broken

## Solution: Build CrossOver 26 Wine from Source

CrossOver 26 source is available under LGPL. We can build it ourselves using the [macos-crossover-wine-cloud-builder](https://github.com/GabLeRoux/macos-crossover-wine-cloud-builder) as a template.

### Why This Should Work

1. CodeWeavers fixes Steam compatibility issues in each CrossOver release
2. CrossOver 26 (Feb 2026) should have the `chrome_elf.dll` / steamwebhelper fixes
3. The build process doesn't require building LLVM from source — uses `brew install gcenx/wine/cx-llvm`

---

## Architecture Overview

### How Wine + wine32on64 Works

```
Windows Game (.exe)
       ↓
   Wine (compatibility layer, NOT emulation)
       ↓
   Translates Windows API calls → macOS API calls
       ↓
   For 32-bit games: wine32on64 thunks convert 32-bit → 64-bit calls
       ↓
   Rosetta 2 (on Apple Silicon): x86-64 → ARM64
       ↓
   macOS / Metal GPU
```

### Graphics Translation Paths

| Path | DirectX | Translation Chain |
|------|---------|-------------------|
| DXVK + MoltenVK | DX9-11 | D3D → Vulkan → Metal |
| D3DMetal (GPTK) | DX11-12 | D3D → Metal (direct) |
| DXMT | DX11 | D3D11 → Metal (direct) |

### Key Components

| Component | Purpose | Source |
|-----------|---------|--------|
| Wine | Windows API compatibility layer | CodeWeavers fork |
| wine32on64 | Run 32-bit Windows apps on 64-bit macOS | Requires custom LLVM (cx-llvm) |
| DXVK | DirectX 9-11 → Vulkan translation | Bundled in CrossOver source |
| MoltenVK | Vulkan → Metal translation | Bundled in CrossOver source |
| cx-llvm | Custom LLVM/Clang with `-mwine32` support | `brew install gcenx/wine/cx-llvm` |

---

## Build Process

### Prerequisites

```bash
# Homebrew packages
brew install bison mingw-w64 pkgconfig flex
brew install gcenx/wine/cx-llvm
brew install freetype gnutls molten-vk sdl2
brew install meson glslang  # For DXVK
```

**Important**: On Apple Silicon, the build runs under x86_64 via Rosetta. Requires Intel Homebrew at `/usr/local/bin/brew`.

### Step 1: Download CrossOver 26 Source

```bash
export CROSS_OVER_VERSION=26.0.0
curl -o crossover-${CROSS_OVER_VERSION}.tar.gz \
  https://media.codeweavers.com/pub/crossover/source/crossover-sources-${CROSS_OVER_VERSION}.tar.gz
tar xf crossover-${CROSS_OVER_VERSION}.tar.gz
```

### Step 2: Apply Patches

**Required patch** — creates missing `distversion.h`:
```bash
cd sources/wine
patch -p1 < /path/to/distversion.patch
```

**DXVK patches** (if building DXVK):
```bash
cd sources/dxvk
patch -p1 < 0001-build-macOS-Fix-up-for-macOS.patch
patch -p1 < 0002-fix-d3d11-header-for-MinGW-9-1883.patch
patch -p1 < 0003-fixes-for-mingw-gcc-12.patch
```

Note: CX26 may have different source structure — patches may need updating.

### Step 3: Set Environment Variables

```bash
export CC="$(brew --prefix cx-llvm)/bin/clang"
export CXX="${CC}++"
export MACOSX_DEPLOYMENT_TARGET=10.14
export CFLAGS="-g -O2 -Wno-implicit-function-declaration -Wno-deprecated-declarations -Wno-format"
export LDFLAGS="-Wl,-headerpad_max_install_names"
export CROSSCFLAGS="-g -O2"
```

### Step 4: Build wine64 (First Pass)

```bash
mkdir -p build/wine64
cd build/wine64
../../sources/wine/configure \
    --enable-win64 \
    --disable-winedbg \
    --disable-tests \
    --with-mingw \
    --without-x \
    --without-alsa \
    --with-coreaudio \
    --with-freetype \
    --with-gnutls \
    --with-opencl \
    --with-opengl \
    --with-sdl
make -j$(sysctl -n hw.ncpu)
```

### Step 5: Build wine32on64 (Second Pass)

```bash
mkdir -p build/wine32on64
cd build/wine32on64
../../sources/wine/configure \
    --enable-win32on64 \
    --with-wine64=../wine64 \
    --disable-winedbg \
    --disable-tests \
    --with-mingw \
    --without-x \
    --without-vulkan
make -j$(sysctl -n hw.ncpu)
```

### Step 6: Install

```bash
# Install wine32on64 first
cd build/wine32on64
make install-lib DESTDIR=/path/to/install

# Install wine64 second (overlays)
cd build/wine64
make install-lib DESTDIR=/path/to/install

# Create symlinks
cd /path/to/install/usr/local/bin
ln -s wine32on64 wine
ln -s wine32on64-preloader wine-preloader
```

---

## Patches Reference

### distversion.patch (REQUIRED)

Creates `include/distversion.h` with debug message macros. Without this, Wine won't compile.

```diff
--- /dev/null
+++ b/include/distversion.h
@@ -0,0 +1,12 @@
+/* distversion.h - CodeWeavers */
+#define WINDEBUG_WHAT_HAPPENED_MESSAGE "..."
+#define WINDEBUG_USER_SUGGESTION_MESSAGE "..."
```

### DXVK Patches (for CX ≥ 21)

1. **0001-build-macOS-Fix-up-for-macOS.patch** — Disables D3D9, removes 32-bit build, fixes Linux-specific commands
2. **0002-fix-d3d11-header-for-MinGW-9-1883.patch** — MinGW 9+ compatibility
3. **0003-fixes-for-mingw-gcc-12.patch** — GCC 12 strictness fixes

---

## Cloud Build Option

Instead of building locally, fork [GabLeRoux/macos-crossover-wine-cloud-builder](https://github.com/GabLeRoux/macos-crossover-wine-cloud-builder) and:

1. Update `CROSS_OVER_VERSION` to `26.0.0`
2. Add version conditionals for CX26 if needed
3. Push to GitHub and let Actions build it
4. Download artifact

---

## Alternative Approaches

| Approach | Pros | Cons |
|----------|------|------|
| **Build CX26 ourselves** | Free, learn how it works | May need debugging, patches may need updating |
| **Ask Gcenx to update** | He maintains wine-crossover, works at CodeWeavers | Depends on his time/priorities |
| **Buy CrossOver** | Just works, $14/month trial | Costs money |
| **GOG Civ IV** | DRM-free, no Steam needed | Still need working Wine for 32-bit |
| **Wait** | Eventually fixes trickle down | Could be months |

---

## Resources

- **CrossOver Source**: https://media.codeweavers.com/pub/crossover/source/
- **Gcenx wine-crossover**: https://github.com/Gcenx/winecx
- **Gcenx Homebrew tap**: https://github.com/Gcenx/homebrew-wine
- **Cloud builder**: https://github.com/GabLeRoux/macos-crossover-wine-cloud-builder
- **cx-llvm**: `brew install gcenx/wine/cx-llvm`

---

## Whisky Architecture Reference

Whisky (cloned to `./whisky/`) shows how to build a macOS app around Wine:

```
WhiskyKit/           # Core library
├── Whisky/          # Bottle, Program, BottleSettings models
├── Wine/            # Wine process execution
└── WhiskyWine/      # Wine binary management

Key patterns:
- Bottles = Wine prefixes stored as directories
- Settings = plist files (BottleSettings, ProgramSettings)
- Launching = `wine64 start /unix /path/to/game.exe`
- Environment = WINEPREFIX, WINEDLLOVERRIDES, DXVK_*, etc.
```

---

## Build Progress (2026-02-16)

### Native ARM64 Build - COMPLETED WITH ISSUES

Successfully built CrossOver 26 (Wine 11.0) natively on ARM64 macOS:

**What Worked:**
- Downloaded CrossOver 26.0.0 source (142MB)
- Applied distversion.patch (required for compilation)
- Fixed two D3DMetal ARM64 compatibility issues:
  - `cocoa_window.m`: Wrapped `WineMetalLayer` usage in `#if defined(__x86_64__)`
  - `event.c`: Wrapped `CLIENT_SURFACE_PRESENTED` case in `#if defined(__x86_64__)`
- Used native ARM Homebrew (no cx-llvm needed for upstream WoW64)
- Built with `--enable-archs=i386,x86_64` for multi-arch PE support
- Installed to `/Users/james/personal/porthole/install/`

**What Didn't Work:**
- Wine executable gets SIGKILL when running `wine wineboot`
- `wine --version` works, but actual Wine prefix initialization fails
- Likely cause: macOS security restrictions on ARM64 WoW64 thunking layer

**Key Findings:**
1. CrossOver 26 uses upstream Wine 11.0's WoW64 (not wine32on64)
2. cx-llvm is NOT required for modern WoW64 builds
3. D3DMetal (GPTK integration) code is x86_64-only
4. Homebrew's wine-crossover runs as x86_64 under Rosetta, which works

### Patches Applied

```bash
# In sources/wine/dlls/winemac.drv/cocoa_window.m:985
- CAMetalLayer *layer = [WineMetalLayer layer];
+ #if defined(__x86_64__)
+     CAMetalLayer *layer = [WineMetalLayer layer];
+ #else
+     CAMetalLayer *layer = [CAMetalLayer layer];
+ #endif

# In sources/wine/dlls/winemac.drv/event.c:402
+ #if defined(__x86_64__)
    case CLIENT_SURFACE_PRESENTED:
        macdrv_client_surface_presented(event);
        break;
+ #endif
```

---

## Next Steps

1. [x] Build CrossOver 26 from source - DONE (ARM64 build complete, but SIGKILL at runtime)
2. [x] Investigate ARM64 SIGKILL root cause - DONE (see "ARM64 Root Cause Analysis" below)
3. [x] Install isolated x86 Homebrew - DONE (`porthole/x86brew/`)
4. [x] Install x86_64 build dependencies via isolated Homebrew - DONE (bison, flex, pkgconf, freetype, gnutls, sdl2, mingw-w64)
5. [x] Rebuild CX26 Wine as x86_64 (runs under Rosetta) - DONE (`install-x86_64/usr/local/`)
6. [x] Verify Wine works - DONE (wineboot, cmd.exe, 32-bit WoW64 all functional)
7. [x] Test with display (Wine GUI, Mac driver) - DONE (notepad, wine-mono dialog all render correctly)
8. [x] Test Steam with working build - DONE (Steam installs/updates, UI is black due to CEF/DComposition, but auth works via blind login)
9. [x] Test Civ IV (32-bit) - DONE (Beyond the Sword runs! Audio stutters but game is playable)
10. [ ] Fix audio stuttering
11. [ ] Decide on Porthole UI vs CLI

---

## ARM64 Root Cause Analysis (2026-02-17)

The native ARM64 build gets SIGKILL for **multiple compounding reasons**, not just entitlements (which are already correct):

1. **No memory reservation**: `loader/main.c:52` reserves the low 8GB only for `__APPLE__ && __x86_64__`. On ARM64, `init_reserved_areas()` is a no-op — macOS frameworks allocate into the address ranges Wine needs.

2. **W^X violations**: `ntdll/unix/virtual.c` uses `mprotect()` to set `PROT_EXEC`, but macOS ARM64 forbids `PROT_WRITE|PROT_EXEC` on the same page. `MAP_JIT` and `pthread_jit_write_protect_np()` are not used anywhere in Wine.

3. **Missing ABI fixes**: CX26 is Wine 11.0, but Wine 11.1 added syscall wrappers (`f43402c`, `2a61baa`) that fix macOS ARM64's non-standard stack parameter packing.

4. **No x86 instruction translator**: Wine's WoW64 expects `xtajit.dll` on ARM64 to translate x86 instructions. No implementation exists in the open-source tree — CrossOver ships proprietary FEX-Emu integration (Linux ARM64 only).

**Conclusion**: Native ARM64 Wine on macOS is a multi-month engineering project (see `thoughts/shared/plans/2026-02-17-native-arm64-wine-macos.md`). The x86_64-under-Rosetta approach is the proven path for now.

**Future note**: Apple has signaled Rosetta 2 will be phased out starting macOS 27 (fall 2026). The native ARM64 path will eventually become necessary.

---

## Isolated x86 Homebrew

### Why Isolated?

Installing Intel Homebrew to `/usr/local` (its default) is risky because:
- `/usr/local/bin` is already on `$PATH` (line 46 of `~/.zprofile`)
- x86_64 binaries would shadow or mix with ARM64 binaries
- Hard to tell what's x86 vs ARM64 when debugging
- Cleanup is difficult — `/usr/local` is shared with other software

### Approach: Custom-Prefix Homebrew

Homebrew is self-contained — it's a git repo with a `bin/brew` binary. We clone it to a project-local directory that is **never on `$PATH`**:

```
/Users/james/personal/porthole/x86brew/
```

Everything is referenced by absolute path. The shell, `$PATH`, and ARM64 Homebrew at `/opt/homebrew` remain completely untouched.

| | Standard `/usr/local` | Our custom prefix |
|---|---|---|
| **PATH pollution** | Yes (already on PATH) | None |
| **Pre-built bottles** | Yes (fast installs) | No (compiles from source) |
| **Risk to ARM setup** | Medium | Zero |
| **Cleanup** | Need to uninstall carefully | `rm -rf x86brew/` |

### Step 1: Install Isolated x86 Homebrew

```bash
# Clone Homebrew to project-local directory under Rosetta
arch -x86_64 git clone https://github.com/Homebrew/brew /Users/james/personal/porthole/x86brew

# Alias for convenience (not in any profile — session only)
X86BREW=/Users/james/personal/porthole/x86brew/bin/brew
```

### Step 2: Install Build Dependencies

```bash
# All commands run under Rosetta via the x86 brew
arch -x86_64 $X86BREW install bison mingw-w64 pkgconfig flex
arch -x86_64 $X86BREW install gcenx/wine/cx-llvm
arch -x86_64 $X86BREW install freetype gnutls molten-vk sdl2
```

No `arch -x86_64` prefix is needed for `$X86BREW` itself (it's already an x86_64 binary), but we use it to ensure the entire shell session is x86_64.

Note: Without pre-built bottles, each formula compiles from source. This is slow but only happens once. We only need ~8 packages.

### Step 3: Set Build Environment

```bash
# All paths point into our isolated x86brew prefix
export X86PREFIX=/Users/james/personal/porthole/x86brew

export CC="$($X86BREW --prefix cx-llvm)/bin/clang"
export CXX="${CC}++"
export MACOSX_DEPLOYMENT_TARGET=10.14

export CFLAGS="-g -O2 -Wno-implicit-function-declaration -Wno-deprecated-declarations -Wno-format"
export LDFLAGS="-Wl,-headerpad_max_install_names"
export CROSSCFLAGS="-g -O2"

# Point pkg-config at x86 libraries only
export PKG_CONFIG_PATH="$X86PREFIX/lib/pkgconfig"
export PKG_CONFIG="$X86PREFIX/bin/pkg-config"

# Ensure x86brew tools are found (for this build session only)
export PATH="$X86PREFIX/bin:$PATH"
```

### Step 4: Build Wine as x86_64

```bash
# Use the existing CX26 source at sources/wine
cd /Users/james/personal/porthole

mkdir -p build/wine-x86_64-build
cd build/wine-x86_64-build

arch -x86_64 ../../sources/wine/configure \
    --enable-archs=i386,x86_64 \
    --disable-winedbg \
    --disable-tests \
    --with-mingw \
    --without-x \
    --without-alsa \
    --with-coreaudio \
    --with-freetype \
    --with-gnutls \
    --with-opencl \
    --with-opengl \
    --with-sdl

arch -x86_64 make -j$(sysctl -n hw.ncpu)
```

### Step 5: Install

```bash
cd /Users/james/personal/porthole/build/wine-x86_64-build
arch -x86_64 make install-lib DESTDIR=/Users/james/personal/porthole/install-x86_64
```

### Step 6: Test (on laptop with display)

```bash
# Set up Wine prefix
export WINEPREFIX=~/.wine-porthole
/Users/james/personal/porthole/install-x86_64/usr/local/bin/wine wineboot

# Test Steam
/Users/james/personal/porthole/install-x86_64/usr/local/bin/wine steam.exe
```

### Cleanup

To remove the isolated Homebrew completely:
```bash
rm -rf /Users/james/personal/porthole/x86brew
```

No shell profiles, PATH, or system state is affected.

---

## Build Details (x86_64 - 2026-02-17)

### Build script: `build_x86_64.sh`

Key discoveries during the build:
- **EGL not needed**: Wine's Mac driver uses CGL (Core OpenGL), not EGL. Removing `--with-opengl` avoids a fatal error.
- **No `--with-*` flags**: Using explicit `--with-X` flags makes missing deps fatal. Let configure auto-detect.
- **pkg-config paths**: `x86brew/lib/pkgconfig/` has correctly-prefixed .pc files (not the Cellar copies)
- **Vulkan fix**: `dlls/win32u/vulkan.c:3009` uses `SONAME_LIBVULKAN` unconditionally (CX HACK). Added `#ifdef SONAME_LIBVULKAN` guard.
- **cx-llvm NOT required**: System gcc (Rosetta) works fine for x86_64 builds
- **DYLD_FALLBACK_LIBRARY_PATH**: Required at runtime for freetype/gnutls/sdl2. Do NOT use `arch -x86_64` prefix (strips DYLD vars). Rosetta runs x86_64 binaries automatically.
- **Launcher script**: `run_wine.sh` sets up DYLD paths and runs wine

### What works:
- `wine wineboot --init` (creates prefix with 593 system32 DLLs)
- `wine cmd /c "echo hello"` (basic execution)
- 32-bit WoW64 (`syswow64/cmd.exe`)
- Windows version reports Windows 10.0.19045 / AMD64

### Display Testing Results (2026-02-18):
- Mac driver (winemac.drv) GUI — WORKS (notepad, wine-mono install dialog)
- Steam — installs and authenticates, but UI is BLACK (CEF/DComposition incompatibility)
- DXVK + MoltenVK — loads correctly, swap chains created
- Steam blind login — WORKS (type username → Tab → password → Enter into black window)
- Civ IV: Beyond the Sword (32-bit) — WORKS! Installed via SteamCMD, launched directly

### Steam Workaround (2026-02-18):
Steam's CEF UI doesn't render due to DCompositionCreateDevice3 being stubbed in Wine.
Workaround: use SteamCMD for game installation, blind login for Steam auth.

```bash
# Install game via SteamCMD
./run_wine.sh steamcmd/steamcmd.exe +@sSteamCmdForcePlatformType windows +login USERNAME +app_update 8800 validate +quit

# Login to Steam (blind - type into black window)
./run_wine.sh steam.exe -no-cef-sandbox
# Type: username → Tab → password → Enter

# Launch game (with Steam running in background)
./run_wine.sh "steamcmd/steamapps/common/Sid Meier's Civilization IV Beyond the Sword/Beyond the Sword/Civ4BeyondSword.exe"
```

### Key flags discovered:
- `-no-cef-sandbox` — disables Chromium kernel sandbox (required for Wine)
- DXVK DLLs (d3d11.dll, d3d10core.dll) installed to prefix for Vulkan rendering
- `WINEDLLOVERRIDES="d3d11,d3d10core=n"` to prefer native DXVK DLLs
- `ForceOpenGLBackingStore=y` registry key (HKCU\Software\Wine\Mac Driver)

---

## Stage 2: DirectComposition Implementation (Steam CEF Rendering)

### Problem

Steam's UI runs on Chromium Embedded Framework (CEF). CEF calls `DCompositionCreateDevice3` to set up GPU-accelerated compositing. Wine returns `E_NOTIMPL`, forcing CEF into a fallback path that produces a **black window** despite DXVK swap chains being created.

When DComp succeeds, CEF takes a completely different rendering path:
- **Path A (DComp works)**: swap chains via `CreateSwapChainForComposition` → DComp visual tree → `Commit()` → content reaches window
- **Path B (DComp fails)**: fallback with `CreateSwapChainForHwnd` → black window for unknown reasons

These are mutually exclusive. Implementing DComp (Path A) bypasses the broken fallback entirely.

### Debug Evidence

```
01e4:fixme:dcomp:DCompositionCreateDevice3 0000000002103590, {5f4633fe-1e08-4cb8-8c75-ce24333f5602}, 000000000010EA30.
```
- CEF requests `IDCompositionDesktopDevice` (IID `{5f4633fe-...}`)
- One call, immediate failure, CEF gives up
- Swap chains are created in fallback path (705x440) but window stays black

### Reference Implementation

Zhiyi Zhang (CodeWeavers) has a working partial implementation on his personal Wine branch (`directcomposition` branch at `gitlab.winehq.org/zhiyi/wine`). Key architecture decisions we adopt:

- **Three source files**: `device.c`, `target.c`, `visual.c`
- **Compositing via D2D1 + GDI BitBlt**: swap chain buffer → D2D1 bitmap → `BitBlt` to HWND
- **Background compositor thread**: `Commit()` starts a thread that continuously composites at display refresh rate
- **`SetContent(IDXGISwapChain1)`**: creates D2D1 device context from the swap chain's DXGI device

### What Zhiyi's Code Is Missing (For Our Needs)

1. **No `DCompositionCreateDevice3`** — only v1 and v2. CEF calls v3.
2. **No `IDCompositionDevice3` in QueryInterface** — CEF explicitly QIs for this after getting `IDCompositionDesktopDevice`
3. **Visual methods return `E_NOTIMPL`** — `SetOffsetX/Y`, `AddVisual`, `SetClip`, etc. CEF uses `CHECK_EQ(hr, S_OK)` and **crashes on failure**.
4. **No `CreateSwapChainForComposition` in DXVK** — CEF creates swap chains through this when DComp is active (not `CreateSwapChainForHwnd`).

### Implementation Plan

#### Phase 1: DComp COM Objects (Wine `dcomp.dll`)

Port Zhiyi's code into our CX26 source tree, then extend it:

**Files to create/modify:**
- `sources/wine/dlls/dcomp/device.c` — replace current stubs with full implementation
- `sources/wine/dlls/dcomp/target.c` — new file (from Zhiyi's branch)
- `sources/wine/dlls/dcomp/visual.c` — new file (from Zhiyi's branch)
- `sources/wine/dlls/dcomp/dcomp_private.h` — new file (from Zhiyi's branch)
- `sources/wine/dlls/dcomp/Makefile.in` — add new sources + imports

**Additions beyond Zhiyi's code:**
1. Add `DCompositionCreateDevice3()` — calls `create_device(3, iid, device)`
2. Add `IDCompositionDevice3` to QueryInterface (same object, version >= 3)
3. Change visual stubs from `E_NOTIMPL` to `S_OK` (store values, don't crash CEF):
   - `SetOffsetX/Y`, `SetTransform`, `SetClip`, `SetClipObject`
   - `SetBitmapInterpolationMode`, `SetBorderMode`, `SetOpacityMode`
   - `SetCompositeMode`, `SetBackFaceVisibility`, `SetEffect`
   - `AddVisual`, `RemoveVisual`, `RemoveAllVisuals`
4. `IDCompositionDevice5` QI → return `E_NOINTERFACE` (CEF handles this gracefully)

#### Phase 2: Composition Swap Chains (DXVK)

**File to modify:**
- `sources/dxvk/src/dxgi/dxgi_factory.cpp` — `CreateSwapChainForComposition`

**Approach**: Redirect `CreateSwapChainForComposition` to create a swap chain backed by a hidden helper window. DComp's `Commit()` (via BitBlt) will copy the rendered content to the real target HWND.

```
CreateSwapChainForComposition(device, desc, output, &swapchain)
  → Create hidden 1x1 HWND (or use a shared helper window)
  → Call CreateSwapChainForHwnd(device, hidden_hwnd, desc, NULL, output, &swapchain)
  → Return the swap chain (DXVK handles rendering to the hidden surface)
```

DComp's `do_composite()` then reads from this swap chain's back buffer and BitBlts to the real window.

#### Phase 3: Integration & Compositing Bridge

The compositing pipeline (from Zhiyi's code, adapted):

```
Commit()
  → Start compositor thread (if not running)
  → Thread loop (at display refresh rate):
      → For each target in device->targets:
          → Get root visual's content (IDXGISwapChain1)
          → Get swap chain's front buffer as IDXGISurface1
          → Create D2D1 bitmap from the surface
          → BitBlt to target HWND via GetDCEx
```

#### Phase 4: Build, Test, Iterate

**Rebuild cycle:**
```bash
cd /Users/james/personal/porthole/build/wine-x86_64-build
arch -x86_64 make -j$(sysctl -n hw.ncpu)
arch -x86_64 make install-lib DESTDIR=/Users/james/personal/porthole/install-x86_64
```

### Automated Testing

To avoid manual verification on each compile cycle, we write a C test program compiled with mingw and run under Wine:

**`tests/test_dcomp.c`** — automated pass/fail test:
1. Create a Win32 window (100x100)
2. Create D3D11 device + DXGI factory
3. Call `DCompositionCreateDevice3(dxgi_device, IID_IDCompositionDesktopDevice, &device)` — verify S_OK
4. QI for `IDCompositionDevice3` — verify S_OK
5. `CreateTargetForHwnd(hwnd)` — verify S_OK
6. `CreateVisual()` — verify S_OK
7. `CreateSwapChainForComposition()` — create a swap chain, render solid red (0xFF0000)
8. `SetContent(swap_chain)` on the visual
9. `target->SetRoot(visual)`
10. `device->Commit()`
11. Sleep 100ms (let compositor thread run)
12. `GetDC(hwnd)` + `GetPixel(0, 0)` — verify pixel is red
13. Exit 0 on success, 1 on failure

**Compile**: `x86_64-w64-mingw32-gcc -o test_dcomp.exe test_dcomp.c -ldxgi -ld3d11 -ldcomp -lole32 -luuid`
**Run**: `./run_wine.sh tests/test_dcomp.exe`

This gives a fully automated pass/fail for the entire pipeline (DComp objects → swap chain → compositing → pixel verification).

### Risks & Unknowns

- **D2D1 dependency**: Zhiyi's compositing uses `d2d1`. Wine's D2D1 may have issues under CX26/x86_64. If so, we can fall back to raw GDI or OpenGL blitting.
- **DXVK swap chain internals**: Redirecting `CreateSwapChainForComposition` to `CreateSwapChainForHwnd` with a hidden window may have side effects (resize handling, present timing).
- **CEF may call methods we haven't implemented**: The test program catches Phase 1-3 issues. Steam testing catches CEF-specific issues.
- **`CreateSwapChainForCompositionSurfaceHandle`**: Used by CEF for video overlays (`IDXGIFactoryMedia`). Not needed for main UI — defer.
- **Multi-visual trees**: Steam may use multiple child visuals. Initial implementation handles single root visual with content; iterate from logs.

### Sequence

1. [ ] Write automated test program (`tests/test_dcomp.c`)
2. [ ] Phase 1: Implement DComp COM objects (Wine dcomp.dll)
3. [ ] Verify test passes Phase 1 checks (device creation, QI, target/visual creation)
4. [ ] Phase 2: Implement `CreateSwapChainForComposition` (DXVK)
5. [ ] Phase 3: Implement compositing (Commit → BitBlt pipeline)
6. [ ] Verify full automated test passes (pixel check)
7. [ ] Test with Steam

---

## Open Questions (Updated)

1. ~~Does cx-llvm work with CrossOver 26?~~ **Not needed** - CX26 uses upstream WoW64
2. ~~Has CodeWeavers changed source structure?~~ **Yes** - Uses Wine 11.0, D3DMetal integrated
3. ~~Are DXVK patches needed?~~ **Possibly** - Need to check sources/dxvk
4. ~~Why does native ARM64 Wine get SIGKILL on macOS?~~ **Answered** - See "ARM64 Root Cause Analysis" above and `thoughts/shared/plans/2026-02-17-native-arm64-wine-macos.md`
5. ~~Is x86_64 build (via Rosetta) more practical than native ARM64?~~ **Yes** - For now. Native ARM64 is a multi-month project, but will be necessary when Rosetta 2 is phased out (~macOS 27, fall 2026)
6. ~~Will custom-prefix Homebrew successfully compile all needed x86 formulae from source?~~ **Yes** - All compiled successfully (p11-kit needed test skip, see local tap)
7. ~~Does `cx-llvm` from Gcenx's tap build from source correctly?~~ **Not needed** - System gcc works fine for x86_64 builds
