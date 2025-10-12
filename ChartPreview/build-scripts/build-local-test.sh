#!/bin/bash

# Chart Preview - Local Test Build Script
# Builds VST3 for REAPER testing and installs it locally
#
# Usage: ./build-local-test.sh [--reaper]
#   --reaper: Force quits and reopens REAPER after build
#
# ⚠️  FOR LOCAL USE ONLY - Does not affect CI/CD
# This script is NOT used by GitHub Actions or any automated builds.
# It's purely for quick local testing on this machine.

set -e

# Parse arguments
OPEN_REAPER=false
BUILD_CONFIG="Debug"
for arg in "$@"; do
    if [ "$arg" = "--reaper" ]; then
        OPEN_REAPER=true
    fi

    if [ "$arg" = "--release" ]; then
        BUILD_CONFIG="Release"
    fi

    if [ "$arg" = "--debug" ]; then
        BUILD_CONFIG="Debug"
    fi

    if [ "$arg" = "--help" ] || [ "$arg" = "-h" ]; then
        echo "Usage: ./build-local-test.sh [--reaper] [--release]"
        echo "  --reaper: Force quits and reopens REAPER after build"
        echo "  --release: Build in Release mode (default is Debug)"
        exit 0
    fi
done

echo "========================================"
echo "Building Chart Preview for Local Testing"
echo "VST3 with REAPER Integration"
echo "========================================"
echo ""

# Check if we're in the right directory
if [ ! -f "ChartPreview.jucer" ]; then
    echo "Error: Must run from ChartPreview directory"
    exit 1
fi

# Configuration
REAPER_TEST_PROJECT="../examples/reaper/reaper-test.RPP"

# Force quit REAPER if running (to avoid save prompts)
if [ "$OPEN_REAPER" = true ]; then
    echo "Force quitting REAPER if running..."
    killall -9 REAPER 2>/dev/null || true
    sleep 0.5  # Brief delay to ensure clean shutdown
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

# Build VST3
echo ""
echo "Building VST3..."
xcodebuild -quiet -project ChartPreview.xcodeproj \
           -target "ChartPreview - VST3" \
           -configuration $BUILD_CONFIG

if [ $? -eq 0 ]; then
    echo "✅ VST3 build successful"

    # Install VST3
    VST3_PATH="build/$BUILD_CONFIG/ChartPreview.vst3"
    if [ -d "$VST3_PATH" ]; then
        echo "   Installing to ~/Library/Audio/Plug-Ins/VST3/..."
        mkdir -p ~/Library/Audio/Plug-Ins/VST3/
        rm -rf ~/Library/Audio/Plug-Ins/VST3/ChartPreview.vst3
        cp -R "$VST3_PATH" ~/Library/Audio/Plug-Ins/VST3/
        echo "   ✅ VST3 installed"
    else
        echo "   ❌ VST3 plugin not found"
        exit 1
    fi
else
    echo "❌ VST3 build failed"
    exit 1
fi

# Success summary
echo ""
echo "========================================"
echo "✅ Build & Install Complete!"
echo "========================================"
echo ""
echo "Plugin installed:"
echo "  📦 VST3: ~/Library/Audio/Plug-Ins/VST3/ChartPreview.vst3"
echo ""
echo "REAPER Integration:"
echo "  ✓ VST3: IReaperHostApplication interface"
echo "  ✓ Timeline MIDI reading enabled"
echo ""

# Open REAPER if requested
if [ "$OPEN_REAPER" = true ]; then
    cd ../..
    if [ -f "$REAPER_TEST_PROJECT" ]; then
        echo "Opening REAPER with test project..."
        open -a "REAPER" "$REAPER_TEST_PROJECT"
    else
        echo "Opening REAPER..."
        open -a "REAPER"
    fi
    echo ""
    echo "🎸 REAPER launched - Ready to test!"
else
    echo "💡 Tip: Use --reaper flag to automatically launch REAPER"
    echo "   ./build-scripts/build-local-test.sh --reaper"
fi
