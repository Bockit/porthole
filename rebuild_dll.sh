#!/bin/bash
# Rebuild a single Wine DLL and install it.
# Usage: ./rebuild_dll.sh dlls/dcomp
# If no argument given, just rebuilds dcomp.

set -e

if [ "$(uname -m)" != "x86_64" ]; then
    exec arch -x86_64 /bin/bash "$0" "$@"
fi

PORTHOLE_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${1:-dlls/dcomp}"

X86PREFIX="$PORTHOLE_DIR/x86brew"
MINGW_I686=$X86PREFIX/Cellar/mingw-w64/13.0.0_2/toolchain-i686/bin
MINGW_X64=$X86PREFIX/Cellar/mingw-w64/13.0.0_2/toolchain-x86_64/bin
BISON=$X86PREFIX/opt/bison/bin
FLEX=$X86PREFIX/opt/flex/bin

export PATH="$MINGW_I686:$MINGW_X64:$BISON:$FLEX:$X86PREFIX/bin:$PATH"
export PKG_CONFIG="$X86PREFIX/bin/pkg-config"
export PKG_CONFIG_PATH="$X86PREFIX/lib/pkgconfig:$X86PREFIX/opt/freetype/lib/pkgconfig:$X86PREFIX/opt/gnutls/lib/pkgconfig:$X86PREFIX/opt/sdl2/lib/pkgconfig:$X86PREFIX/opt/openssl@3/lib/pkgconfig"
export MACOSX_DEPLOYMENT_TARGET=10.14

BUILDDIR="$PORTHOLE_DIR/build/wine-x86_64-build"

echo "=== Building $TARGET ==="
make -C "$BUILDDIR" "${TARGET}/all"

echo "=== Installing ==="
make -C "$BUILDDIR" install-lib DESTDIR="$PORTHOLE_DIR/install-x86_64"

echo "=== Done ==="
