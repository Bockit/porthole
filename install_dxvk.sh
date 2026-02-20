#!/bin/bash
# Install DXVK DLLs into the Wine prefix
set -e

PORTHOLE_DIR="$(cd "$(dirname "$0")" && pwd)"
WINEPREFIX="${WINEPREFIX:-$HOME/.porthole-wine}"

echo "Installing DXVK into $WINEPREFIX"

# Copy x64 DLLs to system32
cp -v "$PORTHOLE_DIR/dxvk/x64/d3d11.dll" "$WINEPREFIX/drive_c/windows/system32/"
cp -v "$PORTHOLE_DIR/dxvk/x64/d3d10core.dll" "$WINEPREFIX/drive_c/windows/system32/"

# Copy x32 DLLs to syswow64
cp -v "$PORTHOLE_DIR/dxvk/x32/d3d11.dll" "$WINEPREFIX/drive_c/windows/syswow64/"
cp -v "$PORTHOLE_DIR/dxvk/x32/d3d10core.dll" "$WINEPREFIX/drive_c/windows/syswow64/"

# Set DLL overrides in registry
"$PORTHOLE_DIR/run_wine.sh" reg add 'HKCU\Software\Wine\DllOverrides' /v d3d11 /d native /f
"$PORTHOLE_DIR/run_wine.sh" reg add 'HKCU\Software\Wine\DllOverrides' /v d3d10core /d native /f

echo "DXVK installed successfully"
