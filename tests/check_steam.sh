#!/bin/bash
# Detect whether the Steam window is currently rendering or black.
# Captures a screenshot (full screen by default) and runs
# check_pixels.py to classify.
#
# Usage: tests/check_steam.sh [--region X,Y,W,H] [--out path.bmp]
# Exit codes match check_pixels.py: 0 rendered, 1 black, 2 unclear/error.

set -u

PORTHOLE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
REGION=""
TITLE="Steam"
OUT="/tmp/porthole-steam-$(date +%Y%m%d-%H%M%S).bmp"
AUTO_REGION=1

while [ $# -gt 0 ]; do
    case "$1" in
        --region)  REGION="$2"; AUTO_REGION=0; shift 2 ;;
        --title)   TITLE="$2"; shift 2 ;;
        --no-auto) AUTO_REGION=0; shift ;;
        --out)     OUT="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 3 ;;
    esac
done

if [ "$AUTO_REGION" = "1" ] && [ -z "$REGION" ]; then
    if [ -x "$PORTHOLE_DIR/tests/find_window" ]; then
        REGION=$("$PORTHOLE_DIR/tests/find_window" --title "$TITLE" --bounds 2>/dev/null || true)
        if [ -n "$REGION" ]; then
            echo "[check_steam] auto-region for title='$TITLE': $REGION" >&2
        else
            echo "[check_steam] no window matched title='$TITLE', using full screen" >&2
        fi
    fi
fi

screencapture -t bmp -x "$OUT" || { echo "screencapture failed" >&2; exit 3; }

ARGS=("$OUT")
[ -n "$REGION" ] && ARGS+=(--region "$REGION")

"$PORTHOLE_DIR/tests/check_pixels.py" "${ARGS[@]}"
RC=$?
echo "[check_steam] capture=$OUT verdict_rc=$RC" >&2
exit $RC
