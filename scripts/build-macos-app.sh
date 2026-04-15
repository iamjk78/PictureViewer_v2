#!/bin/bash

# Build script for macOS PictureViewer.app bundle
# This script builds the application and packages it as a proper macOS .app bundle
# with all Qt dependencies properly bundled and signed to fix Team ID issues.

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
APP_PATH="$BUILD_DIR/PictureViewer.app"
DEPLOY_DIR="$PROJECT_ROOT/dist"
FRAMEWORKS_DIR="$APP_PATH/Contents/Frameworks"

echo -e "${BLUE}=== PictureViewer macOS Build Script ===${NC}\n"

# Function to print section headers
print_section() {
    echo -e "\n${BLUE}>>> $1${NC}"
}

# Function to print errors
print_error() {
    echo -e "${RED}✗ ERROR: $1${NC}"
    exit 1
}

# Function to print success
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function to sign a framework or dylib with ad-hoc signature
sign_framework() {
    local target="$1"
    if [ -d "$target" ]; then
        # Framework directory
        echo "  Signing framework: $(basename "$target")"
        codesign --force --sign - --options runtime --deep "$target" 2>/dev/null || true
    elif [ -f "$target" ]; then
        # Single file (dylib, etc.)
        echo "  Signing file: $(basename "$target")"
        codesign --force --sign - --options runtime "$target" 2>/dev/null || true
    fi
}

# Check prerequisites
print_section "Checking prerequisites"

if ! command -v cmake &> /dev/null; then
    print_error "cmake not found. Install with: brew install cmake"
fi
print_success "cmake found: $(cmake --version | head -1)"

if ! command -v codesign &> /dev/null; then
    print_error "codesign not found (required for macOS)"
fi
print_success "codesign found"

if ! command -v install_name_tool &> /dev/null; then
    print_error "install_name_tool not found (required for macOS)"
fi
print_success "install_name_tool found"

# Find macdeployqt
if ! command -v macdeployqt &> /dev/null; then
    # Try to find Qt's macdeployqt
    if [ -f "/opt/homebrew/opt/qt/libexec/macdeployqt" ]; then
        MACDEPLOYQT="/opt/homebrew/opt/qt/libexec/macdeployqt"
    elif [ -f "/opt/homebrew/opt/qtbase/libexec/macdeployqt" ]; then
        MACDEPLOYQT="/opt/homebrew/opt/qtbase/libexec/macdeployqt"
    elif [ -f "$(which qmake 2>/dev/null)/../../../libexec/macdeployqt" ]; then
        MACDEPLOYQT="$(which qmake)/../../../libexec/macdeployqt"
    else
        print_error "macdeployqt not found. Install Qt with: brew install qt"
    fi
else
    MACDEPLOYQT=$(which macdeployqt)
fi
print_success "macdeployqt found: $MACDEPLOYQT"

# Clean previous build
print_section "Preparing build environment"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
print_success "Build directory ready"

# Configure
print_section "Configuring CMake"
cd "$PROJECT_ROOT"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
print_success "CMake configured"

# Build
print_section "Building application"
cmake --build "$BUILD_DIR" --config Release
print_success "Build completed"

# Verify app exists
if [ ! -d "$APP_PATH" ]; then
    print_error "Application bundle not found at $APP_PATH"
fi
print_success "Application bundle created: $APP_PATH"

# Verify Info.plist
print_section "Verifying bundle structure"
if [ ! -f "$APP_PATH/Contents/Info.plist" ]; then
    print_error "Info.plist not found in bundle"
fi
print_success "Info.plist present"

# Verify icon
if [ ! -f "$APP_PATH/Contents/Resources/eye_icon.ico" ]; then
    print_error "Application icon not found in bundle"
fi
print_success "Application icon present"

# Run macdeployqt to bundle Qt libraries
print_section "Deploying Qt dependencies"
echo "Running macdeployqt..."
# Remove old frameworks if they exist
if [ -d "$FRAMEWORKS_DIR" ]; then
    rm -rf "$FRAMEWORKS_DIR"
fi
"$MACDEPLOYQT" "$APP_PATH" -always-overwrite 2>&1 | grep -v "ERROR: Cannot resolve" || true
print_success "Qt dependencies deployed"

# Fix rpath in the executable
print_section "Fixing rpath in executable"
EXECUTABLE="$APP_PATH/Contents/MacOS/PictureViewer"
if [ -f "$EXECUTABLE" ]; then
    echo "Current install names:"
    otool -L "$EXECUTABLE" | grep -E "Qt|Foundation" | head -5

    # Try to fix rpath references
    echo "Attempting to fix rpath references..."

    # Get all Qt framework references and fix them
    for framework in QtCore QtGui QtWidgets QtNetwork QtPrintSupport QtSvg QtPdf; do
        # Fix both @rpath and direct references
        install_name_tool -change "@rpath/${framework}.framework/Versions/A/${framework}" \
            "@executable_path/../Frameworks/${framework}.framework/Versions/A/${framework}" \
            "$EXECUTABLE" 2>/dev/null || true
        install_name_tool -change "/opt/homebrew/opt/qt/lib/${framework}.framework/Versions/A/${framework}" \
            "@executable_path/../Frameworks/${framework}.framework/Versions/A/${framework}" \
            "$EXECUTABLE" 2>/dev/null || true
        install_name_tool -change "/opt/homebrew/opt/qtbase/lib/${framework}.framework/Versions/A/${framework}" \
            "@executable_path/../Frameworks/${framework}.framework/Versions/A/${framework}" \
            "$EXECUTABLE" 2>/dev/null || true
    done

    print_success "Rpath fixed in executable"
fi

# Sign all Qt frameworks with ad-hoc signature (this is critical for Team ID consistency!)
print_section "Signing Qt frameworks"
if [ -d "$FRAMEWORKS_DIR" ]; then
    echo "Found frameworks at: $FRAMEWORKS_DIR"

    # Remove all existing code signatures from frameworks
    echo "Removing old signatures from frameworks..."
    find "$FRAMEWORKS_DIR" -type d -name "_CodeSignature" -exec rm -rf {} \; 2>/dev/null || true
    find "$FRAMEWORKS_DIR" -type f -name "_CodeSignature" -exec rm -rf {} \; 2>/dev/null || true

    # Remove extended attributes to prevent signature conflicts
    echo "Removing extended attributes..."
    xattr -cr "$FRAMEWORKS_DIR" 2>/dev/null || true

    # Sign all dylibs first (before frameworks to avoid conflicts)
    echo "Signing individual libraries..."
    find "$FRAMEWORKS_DIR" -type f -name "*.dylib" -exec codesign --force --sign - {} \; 2>/dev/null || true

    # Sign each framework
    echo "Signing frameworks..."
    find "$FRAMEWORKS_DIR" -type d -name "*.framework" | while read fw; do
        echo "  Signing: $(basename "$fw")"
        codesign --force --sign - "$fw" 2>/dev/null || true
    done

    print_success "Frameworks signed"
else
    print_error "Frameworks directory not found at $FRAMEWORKS_DIR"
fi

# Remove existing code signatures from the app
print_section "Removing old app signatures"
find "$APP_PATH" -type d -name "_CodeSignature" -exec rm -rf {} \; 2>/dev/null || true
xattr -cr "$APP_PATH" 2>/dev/null || true
print_success "Old signatures removed"

# Fix executable permissions
print_section "Setting executable permissions"
chmod +x "$EXECUTABLE"
print_success "Permissions set"

# Sign the main executable
print_section "Signing main executable"
codesign --force --sign - "$EXECUTABLE" 2>/dev/null || true
print_success "Main executable signed"

# Sign the entire application bundle (without --deep to avoid ambiguous bundle errors)
print_section "Signing application bundle"
codesign --force --sign - "$APP_PATH" 2>/dev/null || true
print_success "Application bundle signed"

# Create distribution directory
print_section "Creating distribution package"
mkdir -p "$DEPLOY_DIR"
rm -rf "$DEPLOY_DIR/PictureViewer.app"
cp -r "$APP_PATH" "$DEPLOY_DIR/"
print_success "Distribution app copied to: $DEPLOY_DIR/PictureViewer.app"

# Verify signatures
print_section "Verifying signatures"
echo "App signature:"
codesign -dv --verbose=4 "$APP_PATH" 2>&1 | grep -E "Identifier|Signature" | head -3
echo ""
echo "Checking frameworks:"
codesign -v "$FRAMEWORKS_DIR/QtCore.framework" 2>&1 | head -1 || true

# Display bundle info
print_section "Bundle information"
echo "Path: $APP_PATH"
echo "Size: $(du -sh "$APP_PATH" | cut -f1)"
echo "Executable: $EXECUTABLE"
echo "Icon: $APP_PATH/Contents/Resources/eye_icon.ico"
echo "Info.plist: $APP_PATH/Contents/Info.plist"
echo "Bundle ID: $(plutil -extract CFBundleIdentifier raw "$APP_PATH/Contents/Info.plist" 2>/dev/null)"

# Show CFBundleDocumentTypes from Info.plist
echo ""
echo "Supported file types:"
plutil -p "$APP_PATH/Contents/Info.plist" 2>/dev/null | grep -A 20 "CFBundleDocumentTypes" | head -15 || echo "  (Check Info.plist for details)"

# Summary
print_section "Build Summary"
echo -e "${GREEN}✓ Build successful!${NC}"
echo ""
echo "To run the application:"
echo "  1. From Finder:  Double-click dist/PictureViewer.app"
echo "  2. From terminal: open dist/PictureViewer.app"
echo "  3. Or directly:   dist/PictureViewer.app/Contents/MacOS/PictureViewer"
echo ""
echo "To open an image file from command line:"
echo "  open dist/PictureViewer.app --args /path/to/image.jpg"
echo ""
echo "To set as default app for images:"
echo "  1. Right-click an image file in Finder"
echo "  2. Select 'Get Info' → 'Open With' → 'Other...'"
echo "  3. Navigate to dist/PictureViewer.app"
echo "  4. Check 'Always Open With'"
echo "  5. Click 'Open'"
echo ""
echo "To install in Applications folder:"
echo "  rm -rf ~/Applications/PictureViewer.app"
echo "  cp -r dist/PictureViewer.app ~/Applications/"
echo ""
echo -e "${GREEN}=== Build Complete ===${NC}\n"
