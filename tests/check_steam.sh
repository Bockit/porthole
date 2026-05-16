#!/bin/bash
# Detect whether a Wine/Steam window is currently rendering or black.
#
# Captures the named window by CGWindowID (screencapture -l), which grabs
# the window's own content regardless of stacking. Then runs
# check_pixels.py to classify on Rec.601 luminance mean/stddev.
#
# Usage:
#   tests/check_steam.sh                       # find window titled "Steam"
#   tests/check_steam.sh --title "Special Offers"
#   tests/check_steam.sh --id 15133            # capture a specific window
#
# Exit codes match check_pixels.py: 0 rendered, 1 black, 2 unclear/error.
# Also prints capture path and verdict JSON to stderr.

set -u

PORTHOLE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TITLE="Steam"
OWNER=""
WIN_ID=""
OUT_BMP="/tmp/porthole-steam-$(date +%Y%m%d-%H%M%S-%N).bmp"
OUT_PNG=""

while [ $# -gt 0 ]; do
    case "$1" in
        --title) TITLE="$2"; shift 2 ;;
        --owner) OWNER="$2"; shift 2 ;;
        --id)    WIN_ID="$2"; shift 2 ;;
        --out)   OUT_BMP="$2"; shift 2 ;;
        --png)   OUT_PNG="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 3 ;;
    esac
done

if [ -z "$WIN_ID" ]; then
    if [ ! -x "$PORTHOLE_DIR/tests/find_window" ]; then
        echo "[check_steam] tests/find_window not built" >&2
        exit 3
    fi
    FW_ARGS=(--title "$TITLE")
    [ -n "$OWNER" ] && FW_ARGS=(--owner "$OWNER" "${FW_ARGS[@]}")
    WIN_ID=$("$PORTHOLE_DIR/tests/find_window" "${FW_ARGS[@]}" --id 2>/dev/null || true)
    if [ -z "$WIN_ID" ]; then
        echo "[check_steam] no window matched title='$TITLE' owner='$OWNER'" >&2
        exit 2
    fi
fi

screencapture -t bmp -x -l "$WIN_ID" "$OUT_BMP" \
    || { echo "[check_steam] screencapture -l $WIN_ID failed" >&2; exit 3; }

if [ ! -s "$OUT_BMP" ]; then
    echo "[check_steam] capture is empty for window $WIN_ID" >&2
    exit 3
fi

# Optional PNG companion for visual inspection.
if [ -n "$OUT_PNG" ]; then
    sips -s format png "$OUT_BMP" --out "$OUT_PNG" >/dev/null 2>&1 || true
fi

VERDICT_JSON=$("$PORTHOLE_DIR/tests/check_pixels.py" "$OUT_BMP")
RC=$?
echo "[check_steam] win=$WIN_ID bmp=$OUT_BMP rc=$RC json=$VERDICT_JSON" >&2
echo "$VERDICT_JSON"
exit $RC
