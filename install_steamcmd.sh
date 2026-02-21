#!/bin/bash
# Download SteamCMD (Windows) for use with Wine
set -e

PORTHOLE_DIR="$(cd "$(dirname "$0")" && pwd)"
STEAMCMD_DIR="$PORTHOLE_DIR/steamcmd"
URL="https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip"

if [ -f "$STEAMCMD_DIR/steamcmd.exe" ]; then
    echo "steamcmd.exe already exists in $STEAMCMD_DIR"
    exit 0
fi

mkdir -p "$STEAMCMD_DIR"
cd "$STEAMCMD_DIR"

echo "Downloading SteamCMD..."
curl -LO "$URL"

echo "Extracting..."
unzip -o steamcmd.zip
rm steamcmd.zip

echo "SteamCMD installed to $STEAMCMD_DIR"
echo "Run with: ./run_wine.sh steamcmd/steamcmd.exe +login <username> +quit"
