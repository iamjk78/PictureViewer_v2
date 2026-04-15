#!/bin/bash

# Build script for macOS PictureViewer.app bundle
# Proper Qt 6 framework bundling with symlink preservation and correct ad-hoc signing order.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
APP_PATH="$BUILD_DIR/PictureViewer.app"
DEPLOY_DIR="$PROJECT_ROOT/dist"
DEPLOY_APP="$DEPLOY_DIR/PictureViewer.app"

print_section() { echo -e "\n${BLUE}>>> $1${NC}"; }
print_ok()      { echo -e "${GREEN}✓ $1${NC}"; }
print_err()     { echo -e "${RED}✗ ERROR: $1${NC}"; exit 1; }

# ── Prerequisites ────────────────────────────────────────────────────────────
print_section "Checking prerequisites"

command -v cmake        &>/dev/null || print_err "cmake not found (brew install cmake)"
command -v codesign     &>/dev/null || print_err "codesign not found"
command -v install_name_tool &>/dev/null || print_err "install_name_tool not found"
command -v rsync        &>/dev/null || print_err "rsync not found"

if command -v macdeployqt &>/dev/null; then
    MACDEPLOYQT=$(which macdeployqt)
elif [ -f "/opt/homebrew/opt/qt/libexec/macdeployqt" ]; then
    MACDEPLOYQT="/opt/homebrew/opt/qt/libexec/macdeployqt"
elif [ -f "/opt/homebrew/opt/qtbase/libexec/macdeployqt" ]; then
    MACDEPLOYQT="/opt/homebrew/opt/qtbase/libexec/macdeployqt"
else
    print_err "macdeployqt not found (brew install qt)"
fi
print_ok "macdeployqt: $MACDEPLOYQT"

# ── Build ────────────────────────────────────────────────────────────────────
print_section "Preparing build environment"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

print_section "Configuring CMake"
cd "$PROJECT_ROOT"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
print_ok "CMake configured"

print_section "Building application"
cmake --build "$BUILD_DIR" --config Release
print_ok "Build complete"

[ -d "$APP_PATH" ]                        || print_err "App bundle not found: $APP_PATH"
[ -f "$APP_PATH/Contents/Info.plist" ]    || print_err "Info.plist missing"
[ -f "$APP_PATH/Contents/Resources/eye_icon.ico" ] || print_err "Icon missing"
print_ok "Bundle structure OK"

# ── Deploy Qt (macdeployqt preserves framework symlinks inside the build dir) ─
print_section "Deploying Qt dependencies with macdeployqt"
"$MACDEPLOYQT" "$APP_PATH" -always-overwrite 2>&1 | grep -v "^$" || true
print_ok "Qt dependencies deployed"

EXECUTABLE="$APP_PATH/Contents/MacOS/PictureViewer"
FRAMEWORKS_DIR="$APP_PATH/Contents/Frameworks"
PLUGINS_DIR="$APP_PATH/Contents/PlugIns"

# ── Fix rpath just in case macdeployqt didn't ────────────────────────────────
print_section "Fixing rpath"
for fw in QtCore QtGui QtWidgets QtNetwork QtPrintSupport QtSvg QtPdf; do
    for prefix in "@rpath" "/opt/homebrew/opt/qt/lib" "/opt/homebrew/opt/qtbase/lib"; do
        install_name_tool -change \
            "${prefix}/${fw}.framework/Versions/A/${fw}" \
            "@executable_path/../Frameworks/${fw}.framework/Versions/A/${fw}" \
            "$EXECUTABLE" 2>/dev/null || true
    done
done
print_ok "rpath references updated"

# ── Remove ALL existing signatures and xattrs before re-signing ───────────────
print_section "Removing old signatures and extended attributes"
find "$APP_PATH" -name "_CodeSignature" -exec rm -rf {} + 2>/dev/null || true
xattr -cr "$APP_PATH" 2>/dev/null || true
print_ok "Old signatures and xattrs removed"

# ── Sign in the mandatory order ───────────────────────────────────────────────
# Rule: inner components before outer; dylibs before .framework bundles;
#       plugins before the main executable; executable before the .app wrapper.
print_section "Code signing (ad-hoc) in correct order"

# 1. Standalone .dylib files in Frameworks
echo "  [1/5] Signing .dylib files in Frameworks…"
while IFS= read -r f; do
    codesign --force --sign - "$f" 2>/dev/null && echo "    signed: $(basename "$f")" || true
done < <(find "$FRAMEWORKS_DIR" -type f -name "*.dylib" 2>/dev/null)

# 2. .framework bundles (sign the bundle root, not just the binary inside)
echo "  [2/5] Signing .framework bundles…"
while IFS= read -r fw; do
    codesign --force --sign - "$fw" 2>/dev/null && echo "    signed: $(basename "$fw")" || true
done < <(find "$FRAMEWORKS_DIR" -mindepth 1 -maxdepth 1 -type d -name "*.framework" 2>/dev/null)

# 3. Plugin .dylib files
echo "  [3/5] Signing plugin .dylib files…"
while IFS= read -r f; do
    codesign --force --sign - "$f" 2>/dev/null && echo "    signed: $(basename "$f")" || true
done < <(find "$PLUGINS_DIR" -type f -name "*.dylib" 2>/dev/null)

# 4. Main executable
echo "  [4/5] Signing main executable…"
chmod +x "$EXECUTABLE"
codesign --force --sign - "$EXECUTABLE"
print_ok "Executable signed"

# 5. The .app bundle itself
echo "  [5/5] Signing .app bundle…"
codesign --force --sign - "$APP_PATH"
print_ok "App bundle signed"

# ── Verify signature on the build artefact ───────────────────────────────────
print_section "Verifying build-dir signature"
codesign -v "$APP_PATH" && print_ok "codesign -v passed" || print_err "codesign -v FAILED"
codesign -dv "$APP_PATH" 2>&1 | grep -E "Identifier|Signature"

# ── Copy to dist/ preserving ALL symlinks ────────────────────────────────────
print_section "Copying to dist/ (rsync, symlinks preserved)"
rm -rf "$DEPLOY_APP"
mkdir -p "$DEPLOY_DIR"
# rsync -a copies symlinks as symlinks, preserves permissions and timestamps
rsync -a "$APP_PATH/" "$DEPLOY_APP/"
print_ok "Copied to $DEPLOY_APP"

# ── Re-sign the dist copy (rsync may reset signatures on some macOS versions) ─
print_section "Re-signing dist copy"

while IFS= read -r f; do
    codesign --force --sign - "$f" 2>/dev/null || true
done < <(find "$DEPLOY_APP/Contents/Frameworks" -type f -name "*.dylib" 2>/dev/null)

while IFS= read -r fw; do
    codesign --force --sign - "$fw" 2>/dev/null || true
done < <(find "$DEPLOY_APP/Contents/Frameworks" -mindepth 1 -maxdepth 1 -type d -name "*.framework" 2>/dev/null)

while IFS= read -r f; do
    codesign --force --sign - "$f" 2>/dev/null || true
done < <(find "$DEPLOY_APP/Contents/PlugIns" -type f -name "*.dylib" 2>/dev/null)

DEPLOY_EXE="$DEPLOY_APP/Contents/MacOS/PictureViewer"
chmod +x "$DEPLOY_EXE"
codesign --force --sign - "$DEPLOY_EXE"
codesign --force --sign - "$DEPLOY_APP"
print_ok "dist copy re-signed"

# ── Final verification ────────────────────────────────────────────────────────
print_section "Final verification"
echo "codesign -v:"
codesign -v "$DEPLOY_APP" && print_ok "Signature valid" || echo "  (expected for ad-hoc — see note below)"
echo ""
echo "codesign -dv:"
codesign -dv "$DEPLOY_APP" 2>&1 | grep -E "Identifier|Signature"
echo ""
echo "spctl:"
spctl -a -vv "$DEPLOY_APP" 2>&1 || true
echo ""
echo "NOTE: spctl 'rejected' is expected for ad-hoc (no Developer ID)."
echo "      The app will still launch; Gatekeeper can be bypassed with:"
echo "      sudo spctl --master-disable   OR   right-click → Open in Finder"

# ── Bundle info ───────────────────────────────────────────────────────────────
print_section "Bundle information"
echo "Path : $DEPLOY_APP"
echo "Size : $(du -sh "$DEPLOY_APP" | cut -f1)"
echo "BundleID: $(plutil -extract CFBundleIdentifier raw "$DEPLOY_APP/Contents/Info.plist" 2>/dev/null)"
echo ""
echo "Framework symlinks check (QtCore):"
ls -la "$DEPLOY_APP/Contents/Frameworks/QtCore.framework/" 2>/dev/null

print_section "Next steps"
cat <<'EOF'
Build OK. To install and test:

  sudo rm -rf /Applications/PictureViewer.app
  sudo cp -RP dist/PictureViewer.app /Applications/

  # Verify
  codesign -v /Applications/PictureViewer.app

  # Test file opening
  open -a /Applications/PictureViewer.app "/Users/jirikrejci/Downloads/Plana nad Luznici.jpeg"
EOF

echo -e "\n${GREEN}=== Build complete ===${NC}\n"
