#!/bin/bash
# Launch Steam through our Wine build with configurable CEF flags
# for the black-window investigation. Idempotent: kills any prior
# Steam first, snapshots the log, returns after the main window
# should have appeared.
#
# Usage: tests/repro_steam.sh [extra steam flags...]
#
# Env:
#   STEAM_WAIT_SECS   seconds to wait for window after launch (default 30)
#   WINEDEBUG         extra debug channels appended to defaults
#   PORTHOLE_LOG_DIR  where to write logs (default tests/logs)
#
# Outputs:
#   $PORTHOLE_LOG_DIR/steam-<timestamp>.log  full stderr/stdout
#   $PORTHOLE_LOG_DIR/steam-<timestamp>.cmd  exact command used
#   prints final log path to stdout

set -u

PORTHOLE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="${PORTHOLE_LOG_DIR:-$PORTHOLE_DIR/tests/logs}"
WAIT_SECS="${STEAM_WAIT_SECS:-30}"
WINEDEBUG_EXTRA="${WINEDEBUG:-}"
STEAM_EXE='/Users/james/.porthole-wine/drive_c/Program Files (x86)/Steam/steam.exe'

mkdir -p "$LOG_DIR"

# Kill any running Steam/wine first so we get a clean session.
pkill -f -- 'steam.exe' 2>/dev/null
pkill -f -- 'steamwebhelper' 2>/dev/null
pkill -f -- 'wine64-preloader' 2>/dev/null
pkill -f -- 'wineserver' 2>/dev/null
sleep 2

if [ ! -f "$STEAM_EXE" ]; then
    echo "ERROR: Steam not found at $STEAM_EXE" >&2
    exit 3
fi

TS=$(date +%Y%m%d-%H%M%S)
LOG="$LOG_DIR/steam-$TS.log"
CMD_FILE="$LOG_DIR/steam-$TS.cmd"

# Defaults baked in: -no-cef-sandbox is required, always.
STEAM_FLAGS=(-no-cef-sandbox "$@")

# Combine WINEDEBUG: always include +dcomp so we can grep for our path
COMBINED_DEBUG="+dcomp"
if [ -n "$WINEDEBUG_EXTRA" ]; then
    COMBINED_DEBUG="$WINEDEBUG_EXTRA,$COMBINED_DEBUG"
fi

{
    echo "WINEDEBUG=$COMBINED_DEBUG"
    echo "run_wine.sh $STEAM_EXE ${STEAM_FLAGS[*]}"
    echo "ts=$TS"
} > "$CMD_FILE"

echo "[repro] launching with flags: ${STEAM_FLAGS[*]}" >&2
echo "[repro] log: $LOG" >&2

# Launch detached; let it write to the log file.
WINEDEBUG="$COMBINED_DEBUG" "$PORTHOLE_DIR/run_wine.sh" "$STEAM_EXE" "${STEAM_FLAGS[@]}" \
    > "$LOG" 2>&1 &
STEAM_PID=$!

echo "[repro] wine pid=$STEAM_PID, sleeping ${WAIT_SECS}s for window..." >&2
sleep "$WAIT_SECS"

# Don't kill — caller (check_steam.sh) screenshots while it's running.
echo "$LOG"
