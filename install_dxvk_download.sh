#!/bin/bash
# Download pre-built DXVK DLLs (Gcenx macOS async build) and install into dxvk/
set -e

PORTHOLE_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="v1.10.3-20230507"
TARBALL="dxvk-macOS-async-${VERSION}.tar.gz"
URL="https://github.com/Gcenx/DXVK-macOS/releases/download/${VERSION}/${TARBALL}"
EXTRACTED="dxvk-macOS-async-${VERSION}"

cd "$PORTHOLE_DIR"

echo "Downloading DXVK ${VERSION}..."
curl -LO "$URL"

echo "Extracting..."
tar xf "$TARBALL"

mkdir -p dxvk/x32 dxvk/x64
cp "${EXTRACTED}/x32/d3d11.dll"     dxvk/x32/
cp "${EXTRACTED}/x32/d3d10core.dll" dxvk/x32/
cp "${EXTRACTED}/x64/d3d11.dll"     dxvk/x64/
cp "${EXTRACTED}/x64/d3d10core.dll" dxvk/x64/

rm -rf "$TARBALL" "$EXTRACTED"

echo "DXVK ${VERSION} installed to dxvk/"
