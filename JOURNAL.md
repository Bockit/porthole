# Porthole Debug Journal

## Background

Steam's CEF GPU process (steamwebhelper --type=gpu-process) crashes with `exit_code=-2147483645` (STATUS_BREAKPOINT / 0x80000003) before DComp or ANGLE initialization. Zero DComp trace messages appear. The crash started on Feb 21 after installing DXVK d3d11.dll to the Wine prefix.

### Key Evidence

**Feb 18-20 (GPU process WORKED):**
- ANGLE initialized successfully: `Renderer11.cpp:1108 (rx::Renderer11::populateRenderer11DeviceCaps): Error querying driver version from DXGI Adapter.` (non-fatal warning)
- No GPU process crashes
- Wine's built-in d3d11.dll (wined3d) was in use — DXVK d3d11.dll was NOT yet installed to prefix

**Feb 21 (GPU process CRASHES):**
- NO ANGLE messages at all — crash happens before D3D11/ANGLE init
- GPU crashes 3-6 times, then falls back to software renderer (black screen)
- DXVK d3d11.dll installed to prefix at 13:04 Feb 21
- d3d10core.dll installed at 12:41 Feb 21
- `WINEDLLOVERRIDES="d3d11,d3d10core=n"` causes Wine to load DXVK's native DLLs instead of built-in wined3d

### Working Theory

DXVK d3d11.dll crashes during Vulkan initialization in the GPU subprocess. The GPU process is a child Wine process — it may fail to find MoltenVK/Vulkan because `VK_ICD_FILENAMES` or `DYLD_FALLBACK_LIBRARY_PATH` aren't properly inherited, or DXVK hits an assertion during early init.

---

## Experiments

### Experiment 1: Confirm DXVK is the culprit

**Hypothesis:** Removing the DXVK d3d11.dll override will restore GPU process to working state (Feb 18 behavior with ANGLE/wined3d).

**Change:** Run Steam WITHOUT `d3d11,d3d10core=n` in WINEDLLOVERRIDES, so Wine uses its built-in wined3d d3d11.dll.

**Command:**
```bash
WINEDEBUG=+dcomp WINEDLLOVERRIDES="" ./run_wine.sh "$HOME/.porthole-wine/drive_c/Program Files (x86)/Steam/steam.exe" 2>&1 | tee ~/steam_exp1.log
```

**Result:** INVALID — run_wine.sh always appends `d3d11,d3d10core=n` to WINEDLLOVERRIDES regardless of what we set. DXVK was still active. GPU process still crashed (CEF log lines 521-547 confirm same `exit_code=-2147483645` pattern). Need to bypass run_wine.sh or rename DLLs.

---

### Experiment 2: Properly disable DXVK to confirm it's the culprit

**Hypothesis:** Same as Exp 1 — GPU process crashes because DXVK d3d11.dll crashes during Vulkan init. Removing the native DLLs will force Wine to use built-in wined3d, restoring Feb 18 behavior.

**Changes:**
1. Rename DXVK DLLs out of the way in the prefix
2. Run without d3d11/d3d10core override (call Wine directly, bypassing run_wine.sh)

**Commands:**
```bash
# Step 1: Move DXVK DLLs aside
/bin/mv ~/.porthole-wine/drive_c/windows/system32/d3d11.dll ~/.porthole-wine/drive_c/windows/system32/d3d11.dll.dxvk
/bin/mv ~/.porthole-wine/drive_c/windows/system32/d3d10core.dll ~/.porthole-wine/drive_c/windows/system32/d3d10core.dll.dxvk

# Step 2: Run Wine directly (no DXVK override)
WINEPREFIX="$HOME/.porthole-wine" \
DYLD_FALLBACK_LIBRARY_PATH="/Users/jameshiscock/personal/porthole/x86brew/Cellar/molten-vk/1.4.0/lib:/Users/jameshiscock/personal/porthole/x86brew/opt/freetype/lib:/Users/jameshiscock/personal/porthole/x86brew/opt/gnutls/lib:/Users/jameshiscock/personal/porthole/x86brew/opt/sdl2/lib:/Users/jameshiscock/personal/porthole/x86brew/lib:/usr/local/lib:/usr/lib" \
VK_ICD_FILENAMES="/Users/jameshiscock/personal/porthole/x86brew/Cellar/molten-vk/1.4.0/etc/vulkan/icd.d/MoltenVK_icd.json" \
WINEDEBUG=+dcomp \
/Users/jameshiscock/personal/porthole/install-x86_64/usr/local/bin/wine "$HOME/.porthole-wine/drive_c/Program Files (x86)/Steam/steam.exe" 2>&1 | tee ~/steam_exp2.log
```

**What to look for:** CEF log should show ANGLE `Renderer11.cpp` messages instead of GPU crash. No `exit_code=-2147483645`.

**Result:** GPU process STILL crashes with same `exit_code=-2147483645`. No ANGLE messages. **DXVK is NOT the culprit.** CEF log lines 553-580 confirm identical crash pattern.

### Key Insight: What actually changed between Feb 20 and Feb 21?

The only Wine source changes are in `dcomp/` (3 files) and `dxgi/factory.c`. But the FULL Wine was rebuilt. However — the main Steam process, steamwebhelper browser process, and all other child processes work fine. Only the GPU subprocess crashes.

**New theory:** Our DComp implementation is causing the crash. On Feb 18-20, `DCompositionCreateDevice` etc returned `E_NOTIMPL` (stubs). CEF probed for DComp, got failure, and fell back to non-DComp ANGLE path. On Feb 21, our DComp returns `S_OK` — CEF takes the DComp code path, but our implementation doesn't fully satisfy what CEF expects (e.g., IDCompositionDevice3 QI returns wrong vtable), hitting a Chromium `CHECK()` → STATUS_BREAKPOINT crash.

**Evidence supporting this:**
- GPU process crashes BEFORE ANGLE init — DComp probing happens before ANGLE
- Chromium 126 probes for DComp availability by calling `DCompositionCreateDevice2`
- Our device QI returns `IDCompositionDesktopDevice` pointer for `IID_IDCompositionDevice3` — wrong vtable if Device3-specific methods are called
- Zero DComp TRACE messages — crash may happen during DComp device creation or QI, before any TRACE fires

---

### Experiment 3: Confirm DComp implementation is the culprit

**Hypothesis:** Making DComp exports return E_NOTIMPL (reverting to stub behavior) will restore GPU process to working state.

**Change:** Modify `DCompositionCreateDevice`, `DCompositionCreateDevice2`, `DCompositionCreateDevice3` in device.c to return E_NOTIMPL. Rebuild dcomp.dll and reinstall. Also no DXVK (DLLs renamed from exp 2).

**Result:** CONFIRMED — GPU process DID NOT CRASH. CEF log lines 582-587 show a new session with zero `exit_code=-2147483645` errors. However, Steam self-updated during this run (19960 lines of MoveFileTransactedW), and after restart the child process lost `DYLD_FALLBACK_LIBRARY_PATH` → `Failed to load libMoltenVK.dylib` + `FreeType not found`. No window appeared because of the restart, not because of GPU crash.

**Conclusion:** Our DComp implementation causes the GPU process crash. When DComp returns E_NOTIMPL, CEF falls back to non-DComp path and the GPU process works.

---

## Root Cause Analysis

The GPU process calls `DCompositionCreateDevice2` (or similar) early in initialization to probe for DComp support. When our implementation returns S_OK, CEF takes the DirectComposition code path. Something in our DComp device/visual/target implementation then causes a Chromium CHECK() failure → STATUS_BREAKPOINT crash.

Possible specific causes:
1. **IDCompositionDevice3 vtable mismatch** — QI for IID_IDCompositionDevice3 returns IDCompositionDesktopDevice pointer (wrong vtable, calling Device3 methods would hit wrong slots)
2. **Missing interface** — CEF may QI for an interface we don't support, and our stub doesn't handle it correctly
3. **Method behavior** — CEF may call methods that our stubs handle incorrectly (returning wrong values, missing side effects)

Next step: Instead of fully stubbing DComp, selectively narrow down which part causes the crash. Approach: enable DComp device creation but make it fail on specific operations to find exactly what CEF expects.

---

### Experiment 4: Restore DComp but reject IDCompositionDevice3

**Hypothesis:** CEF requests IDCompositionDevice3, we return IDCompositionDesktopDevice vtable, CEF calls a Device3-specific method via wrong vtable slot → crash. Rejecting Device3 (returning E_NOINTERFACE) should fix the crash while keeping Device1/Device2 working.

**Result:** GPU still crashes. Zero DComp traces. Device3 QI is not the issue.

### CRITICAL DISCOVERY: Stale prefix DLL

Our dcomp.dll Makefile.in has `EXTRADLLFLAGS = -Wb,--prefer-native`. This means Wine loads the DLL from the PREFIX (`~/.porthole-wine/drive_c/windows/system32/dcomp.dll`) instead of the built-in from the install directory. The prefix copy was created during the initial Wine build (Feb 21 11:46) and was **never updated** by our subsequent rebuilds. All experiments 1-4 were running the ORIGINAL dcomp.dll code, not our fixes.

Evidence:
- `system32/dcomp.dll`: 386687 bytes, Feb 21 11:46 (stale)
- `syswow64/dcomp.dll`: 317686 bytes, Feb 21 11:46 (stale)
- Our rebuilt `install-x86_64/.../dcomp.dll`: updated multiple times but never used

Fix: Removed stale copies from prefix (renamed to `.old`). Wine will now load the built-in from install directory.

---

### Experiment 5: Test with updated dcomp.dll actually loading

**Hypothesis:** With stale prefix copy removed, our Device3 QI fix and cross-process HWND fix will actually take effect. May or may not fix the GPU crash, but at minimum we should see DComp TRACE messages.

**Command:**
```bash
WINEDEBUG=+dcomp ./run_wine.sh "$HOME/.porthole-wine/drive_c/Program Files (x86)/Steam/steam.exe" 2>&1 | tee ~/steam_exp5.log
```

**Result:** (pending)
