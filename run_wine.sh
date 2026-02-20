#!/bin/bash
# Porthole Wine launcher script
# Runs CrossOver 26 Wine (x86_64) under Rosetta 2

PORTHOLE_DIR="$(cd "$(dirname "$0")" && pwd)"
WINE_DIR="$PORTHOLE_DIR/install-x86_64/usr/local"
X86PREFIX="$PORTHOLE_DIR/x86brew"

MOLTENVK="$X86PREFIX/Cellar/molten-vk/1.4.0"

export WINEPREFIX="${WINEPREFIX:-$HOME/.porthole-wine}"
export DYLD_FALLBACK_LIBRARY_PATH="$MOLTENVK/lib:$X86PREFIX/opt/freetype/lib:$X86PREFIX/opt/gnutls/lib:$X86PREFIX/opt/sdl2/lib:$X86PREFIX/lib:/usr/local/lib:/usr/lib"
export VK_ICD_FILENAMES="$MOLTENVK/etc/vulkan/icd.d/MoltenVK_icd.json"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:+$WINEDLLOVERRIDES;}d3d11,d3d10core=n"

exec "$WINE_DIR/bin/wine" "$@"
