# DComp Implementation - Handover Notes

## Status: Black screen persists, but root causes identified

## What Was Implemented

### Phase 1: DComp COM Objects (Working)
- `sources/wine/dlls/dcomp/device.c` - Full IDCompositionDevice/IDCompositionDesktopDevice with dual vtables
- `sources/wine/dlls/dcomp/visual.c` - IDCompositionVisual2 with visual tree (AddVisual/RemoveVisual/SetContent)
- `sources/wine/dlls/dcomp/target.c` - IDCompositionTarget with SetRoot
- `sources/wine/dlls/dcomp/dcomp_private.h` - Shared structs and declarations
- `sources/wine/dlls/dcomp/Makefile.in` - Build config with `IMPORTS = user32` and `--prefer-native`
- `tests/test_dcomp_minimal.c` - 26/26 tests passing (SSH-friendly, no D3D needed)

### Phase 2: CreateSwapChainForComposition (Working)
- `sources/wine/dlls/dxgi/factory.c` - Creates hidden WS_POPUP window, delegates to CreateSwapChainForHwnd
- Note: Wine's builtin dxgi.dll handles this, NOT DXVK (DXVK's dxgi.dll is not installed)

### Phase 3: Compositing Bridge (Broken - see problems below)
- `device.c` has `do_composite()` / `composite_thread_proc()` that reparents the swap chain's hidden window into the target HWND
- `device1_Commit()` spawns the compositor thread

## What the Logs Show

With `WINEDEBUG=+dcomp`, Steam's CEF successfully:
1. Creates DComp device via `DCompositionCreateDevice3` (version 3, returns IDCompositionDesktopDevice)
2. Creates target for HWND `0x2013E` (the Steam window)
3. Builds a 5-level visual tree with content at depth 4
4. Calls `SetContent` with an actual IDXGISwapChain (QI succeeds)
5. Calls `Commit` which triggers the compositor

The compositor finds the content and reparents the swap chain window (`0x2013C`, 1024x768) into the target (`0x2013E`).

## Two Critical Problems

### Problem 1: Deadlock (immediate showstopper)

```
0234:err:sync:RtlpWaitForCriticalSection section 0000000000311DF0 "?" wait timed out in thread 0234, blocked by 0440, retrying (60 sec)
```

- Thread **0440** (compositor): holds `device->cs`, calls `SetParent`/`SetWindowPos` which sends window messages to thread 0234
- Thread **0234** (CEF): tries to acquire `device->cs` for SetContent/Commit, blocked by 0440
- Classic lock-ordering deadlock: window operations under lock send messages to the thread that needs the same lock

**Fix:** Don't hold `device->cs` while calling window APIs. Copy needed data under the lock, release it, then do window operations. Or better: eliminate the compositor thread entirely and do reparenting synchronously in `Commit()` (outside the lock).

### Problem 2: Compositor re-reparents every 16ms

`do_composite` calls `SetParent`/`SetWindowLongW`/`SetWindowPos` on every loop iteration (60fps), even when already reparented. This is wasteful and widens the deadlock window.

**Fix:** Track whether each target has already been composited. Only re-reparent when content changes.

### Potential Problem 3: Vulkan surface may not follow reparenting

The Vulkan surface was created for the hidden `__wine_dcomp_swapchain` window. Reparenting moves the Win32 window, but on macOS/Wine the Metal layer (NSView) may not render correctly in the new parent. If the screen stays black after fixing the deadlock, this approach may be fundamentally flawed.

**Alternative approach:** Instead of hidden window + reparenting, create the swap chain directly for the target HWND. This requires deferring swap chain creation until `SetContent` links it to a visual with a known target, or passing the target HWND through some other mechanism.

## Unknown IID

CEF queries for `{df0c7cec-cdeb-4d4a-b91c-721bf22f4e6c}` which we return E_NOINTERFACE. This is likely IDCompositionDevice4 or similar. CEF continues working after this failure, so it's non-blocking.

## Build Notes

- Build from: `cd build/wine-x86_64-build && arch -x86_64 make dlls/dcomp/all`
- Install: `arch -x86_64 make install-lib DESTDIR=/Users/james/personal/porthole/install-x86_64`
- After Makefile.in changes: `arch -x86_64 ./config.status` in build dir
- Must use `/bin/bash` not `bash` (Homebrew bash is arm64-only)
- `initguid.h` must only be included in `device.c` (not the shared header)
- Vtable order must match IDL: SetTransformObject before SetTransform, SetClipObject before SetClip

## Key Files

| File | What |
|------|------|
| `sources/wine/dlls/dcomp/device.c` | DComp device, compositor, exported DCompositionCreateDevice* |
| `sources/wine/dlls/dcomp/visual.c` | Visual tree implementation |
| `sources/wine/dlls/dcomp/target.c` | Composition target |
| `sources/wine/dlls/dcomp/dcomp_private.h` | Shared structs/GUIDs |
| `sources/wine/dlls/dcomp/Makefile.in` | Build config |
| `sources/wine/dlls/dxgi/factory.c` | CreateSwapChainForComposition implementation |
| `tests/test_dcomp_minimal.c` | Unit tests (26/26 pass) |

## Next Steps

1. Fix the deadlock (highest priority - without this nothing else can be tested)
2. Make reparenting idempotent (only when content changes)
3. Test whether reparenting actually makes content visible on screen
4. If not visible: try creating swap chain directly for target HWND instead of hidden window
5. Consider whether `{df0c7cec-cdeb-4d4a-b91c-721bf22f4e6c}` matters
