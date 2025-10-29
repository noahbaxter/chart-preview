#!/bin/bash

# Chart Preview - Local AU Build Script
# Builds AU (Audio Unit) format for testing
#
# Usage: ./build-local-au.sh [--release]
#   --release: Build in Release mode (default is Debug)
#
# ⚠️  FOR LOCAL USE ONLY - Does not affect CI/CD
# This script is NOT used by GitHub Actions or any automated builds.
# It's purely for quick local testing and debugging on this machine.
# Use this to reproduce CI AU build failures locally.

set -e

# Parse arguments
BUILD_CONFIG="Debug"
for arg in "$@"; do
    if [ "$arg" = "--release" ]; then
        BUILD_CONFIG="Release"
    fi

    if [ "$arg" = "--debug" ]; then
        BUILD_CONFIG="Debug"
    fi

    if [ "$arg" = "--help" ] || [ "$arg" = "-h" ]; then
        echo "Usage: ./build-local-au.sh [--release] [--debug]"
        echo "  --release: Build in Release mode (default is Debug)"
        echo "  --debug:   Build in Debug mode (default)"
        exit 0
    fi
done

echo "========================================"
echo "Building Chart Preview AU Format"
echo "Testing with CI optimization flags"
echo "========================================"
echo ""

# Check if we're in the right directory
if [ ! -f "ChartPreview.jucer" ]; then
    echo "Error: Must run from ChartPreview directory"
    exit 1
fi

# Regenerate Xcode project if needed
if [ "ChartPreview.jucer" -nt "Builds/MacOSX/ChartPreview.xcodeproj" ]; then
    echo "Regenerating Xcode project from Projucer..."
    "/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "ChartPreview.jucer"
fi

# Clean previous builds
echo "Cleaning previous builds..."
rm -rf Builds/MacOSX/build

cd Builds/MacOSX

# Build AU
echo ""
echo "Building AU (with CI optimization flags to match remote build)..."
echo "Note: This uses GCC_OPTIMIZATION_LEVEL=3 which enables LTO (Link-Time Optimization)"
echo "This may reproduce CI build failures locally for debugging."
echo ""

xcodebuild -project ChartPreview.xcodeproj \
           -target "ChartPreview - AU" \
           -configuration $BUILD_CONFIG \
           -destination "generic/platform=macOS" \
           ARCHS="arm64 x86_64" \
           VALID_ARCHS="arm64 x86_64" \
           ONLY_ACTIVE_ARCH=NO \
           MACOSX_DEPLOYMENT_TARGET=10.15 \
           GCC_OPTIMIZATION_LEVEL=3 \
           SWIFT_OPTIMIZATION_LEVEL="-O"

if [ $? -eq 0 ]; then
    echo "✅ AU build successful"

    # Install AU
    AU_PATH="build/$BUILD_CONFIG/ChartPreview.component"
    if [ -d "$AU_PATH" ]; then
        echo "   Installing to ~/Library/Audio/Plug-Ins/Components/..."
        mkdir -p ~/Library/Audio/Plug-Ins/Components/
        rm -rf ~/Library/Audio/Plug-Ins/Components/ChartPreview.component
        cp -R "$AU_PATH" ~/Library/Audio/Plug-Ins/Components/
        echo "   ✅ AU installed"
    else
        echo "   ❌ AU plugin not found at $AU_PATH"
        echo "   Checking build directory..."
        find build -name "*.component" -o -name "*.vst3" 2>/dev/null | head -10
        exit 1
    fi
else
    echo "❌ AU build failed"
    echo ""
    echo "This likely means the CI build will also fail."
    echo "Check the error messages above to debug."
    exit 1
fi

# Success summary
echo ""
echo "========================================"
echo "✅ Build & Install Complete!"
echo "========================================"
echo ""
echo "Plugin installed:"
echo "  📦 AU: ~/Library/Audio/Plug-Ins/Components/ChartPreview.component"
echo ""
echo "💡 Next steps:"
echo "  - Test AU in your DAW (Logic Pro, Reaper with AU support, etc.)"
echo "  - If this build fails, the CI build will also fail with the same error"
echo "  - Use this script to iterate on fixes locally"
echo ""
