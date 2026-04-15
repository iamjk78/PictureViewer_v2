#!/bin/bash

# Build script for macOS PictureViewer.app bundle
# This script builds the application and packages it as a proper macOS .app bundle
# with all Qt dependencies and document type registration.

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

# Check prerequisites
print_section "Checking prerequisites"

if ! command -v cmake &> /dev/null; then
    print_error "cmake not found. Install with: brew install cmake"
fi
print_success "cmake found: $(cmake --version | head -1)"

if ! command -v macdeployqt &> /dev/null; then
    # Try to find Qt's macdeployqt
    if [ -f "/opt/homebrew/opt/qt/libexec/macdeployqt" ]; then
        MACDEPLOYQT="/opt/homebrew/opt/qt/libexec/macdeployqt"
    elif [ -f "$(which qmake)/../../../libexec/macdeployqt" ]; then
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
echo "Running macdeployqt... (this may take a moment)"
"$MACDEPLOYQT" "$APP_PATH" -always-overwrite
print_success "Qt dependencies deployed"

# Fix executable permissions
print_section "Setting executable permissions"
chmod +x "$APP_PATH/Contents/MacOS/PictureViewer"
print_success "Permissions set"

# Create distribution directory
print_section "Creating distribution package"
mkdir -p "$DEPLOY_DIR"
rm -rf "$DEPLOY_DIR/PictureViewer.app"
cp -r "$APP_PATH" "$DEPLOY_DIR/"
print_success "Distribution app copied to: $DEPLOY_DIR/PictureViewer.app"

# Verify bundle signature (unsigned is okay for development)
print_section "Verifying bundle structure"
echo "Checking bundle structure..."
codesign -v "$APP_PATH" 2>&1 | grep -q "valid on disk" || echo "Note: App is unsigned (normal for development builds)"
print_success "Bundle structure verified"

# Display bundle info
print_section "Bundle information"
echo "Path: $APP_PATH"
echo "Size: $(du -sh "$APP_PATH" | cut -f1)"
echo "Executable: $APP_PATH/Contents/MacOS/PictureViewer"
echo "Icon: $APP_PATH/Contents/Resources/eye_icon.ico"
echo "Info.plist: $APP_PATH/Contents/Info.plist"

# Show CFBundleDocumentTypes from Info.plist
echo ""
echo "Supported file types:"
plutil -p "$APP_PATH/Contents/Info.plist" 2>/dev/null | grep -A 20 "CFBundleDocumentTypes" | head -20 || echo "  (Check Info.plist for details)"

# Summary
print_section "Build Summary"
echo -e "${GREEN}✓ Build successful!${NC}"
echo ""
echo "To run the application:"
echo "  1. From Finder:  Double-click dist/PictureViewer.app"
echo "  2. From terminal: open dist/PictureViewer.app"
echo "  3. Or directly:   dist/PictureViewer.app/Contents/MacOS/PictureViewer"
echo ""
echo "To set as default app for images:"
echo "  1. Right-click an image file in Finder"
echo "  2. Select 'Open With' → 'Other...'"
echo "  3. Navigate to dist/PictureViewer.app"
echo "  4. Check 'Always Open With'"
echo "  5. Click 'Open'"
echo ""
echo "To install in Applications folder:"
echo "  cp -r dist/PictureViewer.app ~/Applications/"
echo ""
echo -e "${GREEN}=== Build Complete ===${NC}\n"
