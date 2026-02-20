#!/bin/bash
set -e

# Ensure we're running under Rosetta (x86_64)
if [ "$(uname -m)" != "x86_64" ]; then
    echo "Re-launching under Rosetta..."
    exec arch -x86_64 /bin/bash "$0" "$@"
fi

PORTHOLE_DIR="$(cd "$(dirname "$0")" && pwd)"

X86PREFIX="$PORTHOLE_DIR/x86brew"
MOLTENVK=$X86PREFIX/Cellar/molten-vk/1.4.0
MINGW_I686=$X86PREFIX/Cellar/mingw-w64/13.0.0_2/toolchain-i686/bin
MINGW_X64=$X86PREFIX/Cellar/mingw-w64/13.0.0_2/toolchain-x86_64/bin
BISON=$X86PREFIX/opt/bison/bin
FLEX=$X86PREFIX/opt/flex/bin

export PATH="$MINGW_I686:$MINGW_X64:$BISON:$FLEX:$X86PREFIX/bin:$PATH"
export PKG_CONFIG_PATH="$X86PREFIX/lib/pkgconfig:$X86PREFIX/opt/freetype/lib/pkgconfig:$X86PREFIX/opt/gnutls/lib/pkgconfig:$X86PREFIX/opt/sdl2/lib/pkgconfig:$X86PREFIX/opt/openssl@3/lib/pkgconfig"
export MACOSX_DEPLOYMENT_TARGET=10.14
export CFLAGS="-g -O2 -Wno-implicit-function-declaration -Wno-deprecated-declarations -Wno-format -I$MOLTENVK/libexec/include"
export LDFLAGS="-Wl,-headerpad_max_install_names -L$X86PREFIX/lib -L$X86PREFIX/opt/gnutls/lib -L$X86PREFIX/opt/freetype/lib -L$X86PREFIX/opt/sdl2/lib -L$MOLTENVK/lib"
export CPPFLAGS="-I$X86PREFIX/include -I$X86PREFIX/opt/gnutls/include -I$X86PREFIX/opt/freetype/include -I$X86PREFIX/opt/sdl2/include -I$MOLTENVK/libexec/include"
export CROSSCFLAGS="-g -O2"

BUILDDIR="$PORTHOLE_DIR/build/wine-x86_64-build"
SRCDIR="$PORTHOLE_DIR/sources/wine"

mkdir -p "$BUILDDIR"
cd "$BUILDDIR"

echo "=== Configuring Wine x86_64 ==="
"$SRCDIR/configure" \
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
    --with-sdl \
    --with-vulkan \
    --prefix=/usr/local

echo "=== Building Wine x86_64 ==="
make -j$(sysctl -n hw.ncpu)

echo "=== Installing Wine x86_64 ==="
make install-lib DESTDIR="$PORTHOLE_DIR/install-x86_64"

echo "=== Done ==="
file "$PORTHOLE_DIR/install-x86_64/usr/local/bin/wine"
