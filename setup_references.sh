#!/bin/bash
# Clone reference repositories used for architecture research.
# These are gitignored and not part of the Porthole source tree.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

clone_if_missing() {
    local dir="$1"
    local url="$2"
    if [ -d "$dir" ]; then
        echo "  $dir/ already exists, skipping"
    else
        echo "  Cloning $url -> $dir/"
        git clone "$url" "$dir"
    fi
}

echo "Setting up reference repositories..."
echo

# Whisky - macOS Wine GUI app (SwiftUI architecture reference)
clone_if_missing whisky https://github.com/Whisky-App/Whisky.git

# Gcenx's Wine fork - CrossOver/Wine macOS builds
clone_if_missing references/winecx https://github.com/Gcenx/winecx.git

# Cloud builder - CI template for building CrossOver Wine on macOS
clone_if_missing references/macos-crossover-wine-cloud-builder https://github.com/GabLeRoux/macos-crossover-wine-cloud-builder.git

echo
echo "Done. Reference repos are gitignored and won't be committed."
