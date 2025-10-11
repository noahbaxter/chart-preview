#!/bin/bash

# Chart Preview - Local Test Build Script
# Builds VST2 + VST3 for REAPER testing and installs them locally
#
# Usage: ./build-local-test.sh [--open-reaper]
#   --open-reaper: Force quits and reopens REAPER after build
#
# ⚠️  FOR LOCAL USE ONLY - Does not affect CI/CD
# This script is NOT used by GitHub Actions or any automated builds.
# It's purely for quick local testing on this machine.

set -e

# Parse arguments
OPEN_REAPER=false
for arg in "$@"; do
    if [ "$arg" = "--open-reaper" ]; then
        OPEN_REAPER=true
    fi
done

echo "========================================"
echo "Building Chart Preview for Local Testing"
echo "VST2 + VST3 with REAPER Integration"
echo "========================================"
echo ""

# Check if we're in the right directory
if [ ! -f "ChartPreview.jucer" ]; then
    echo "Error: Must run from ChartPreview directory"
    exit 1
fi

# Configuration
BUILD_CONFIG="Debug"  # Use Debug for faster builds and debug output
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

# Build VST2
echo ""
echo "1/2 Building VST2..."
xcodebuild -quiet -project ChartPreview.xcodeproj \
           -target "ChartPreview - VST" \
           -configuration $BUILD_CONFIG

if [ $? -eq 0 ]; then
    echo "✅ VST2 build successful"

    # Install VST2
    VST2_PATH="build/$BUILD_CONFIG/ChartPreview.vst"
    if [ -d "$VST2_PATH" ]; then
        echo "   Installing to ~/Library/Audio/Plug-Ins/VST/..."
        mkdir -p ~/Library/Audio/Plug-Ins/VST/
        rm -rf ~/Library/Audio/Plug-Ins/VST/ChartPreview.vst
        cp -R "$VST2_PATH" ~/Library/Audio/Plug-Ins/VST/
        echo "   ✅ VST2 installed"
    else
        echo "   ❌ VST2 plugin not found"
        exit 1
    fi
else
    echo "❌ VST2 build failed"
    exit 1
fi

# Build VST3
echo ""
echo "2/2 Building VST3..."
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
echo "Plugins installed:"
echo "  📦 VST2: ~/Library/Audio/Plug-Ins/VST/ChartPreview.vst"
echo "  📦 VST3: ~/Library/Audio/Plug-Ins/VST3/ChartPreview.vst3"
echo ""
echo "REAPER Integration:"
echo "  ✓ VST2: Direct REAPER API access"
echo "  ✓ VST3: IReaperHostApplication interface"
echo "  ✓ Timeline MIDI reading enabled"
echo ""
echo "Testing tips:"
echo "  - Green background = REAPER connection successful"
echo "  - Check debug output for '✅ REAPER API connected'"
echo "  - Both VST2 and VST3 should work identically"
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
    echo "💡 Tip: Use --open-reaper flag to automatically launch REAPER"
    echo "   ./build-scripts/build-local-test.sh --open-reaper"
fi
