#!/bin/bash

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
DIST_APP="$PROJECT_DIR/dist/PictureViewer.app"
TARGET_APP="/Applications/PictureViewer.app"

echo ">>> Building macOS app"
"$PROJECT_DIR/scripts/build-macos-app.sh"

echo ">>> Killing running PictureViewer"
pkill -f PictureViewer.app || true

echo ">>> Removing old version"
sudo rm -rf "$TARGET_APP"

echo ">>> Installing new version"
sudo cp -RP "$DIST_APP" "$TARGET_APP"

echo ">>> Clearing quarantine"
sudo xattr -dr com.apple.quarantine "$TARGET_APP"

echo ">>> Launching new version"
open "$TARGET_APP"

echo ">>> Done – running latest version"
