#!/bin/bash

# Chart Preview - VST2 Build Script for REAPER Integration
# This builds the VST2 version specifically for testing REAPER API integration

set -e

echo "=================================="
echo "Building Chart Preview VST2 for REAPER Integration"
echo "=================================="

# Check if we're in the right directory
if [ ! -f "ChartPreview.jucer" ]; then
    echo "Error: Must run from ChartPreview directory"
    exit 1
fi

# First regenerate the Xcode project with VST2 support
echo "Regenerating Xcode project with VST2 support..."
/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer --resave "ChartPreview.jucer"

# Clean previous builds
echo "Cleaning previous builds..."
rm -rf Builds/MacOSX/build

# Build VST2
echo "Building VST2 plugin..."
cd Builds/MacOSX

# Note: JUCE creates a target called "ChartPreview - VST" for VST2
xcodebuild -project ChartPreview.xcodeproj \
           -target "ChartPreview - VST" \
           -configuration Debug \
           build

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo "✅ VST2 build successful!"

    # Find the VST2 plugin
    VST2_PATH="build/Debug/ChartPreview.vst"

    if [ -d "$VST2_PATH" ]; then
        # Install to user VST folder
        echo "Installing VST2 to ~/Library/Audio/Plug-Ins/VST/..."
        mkdir -p ~/Library/Audio/Plug-Ins/VST/
        rm -rf ~/Library/Audio/Plug-Ins/VST/ChartPreview.vst
        cp -R "$VST2_PATH" ~/Library/Audio/Plug-Ins/VST/

        echo "✅ VST2 installed successfully!"
        echo ""
        echo "=================================="
        echo "VST2 REAPER Integration Ready!"
        echo "=================================="
        echo ""
        echo "The VST2 version has been built with REAPER API support."
        echo "Location: ~/Library/Audio/Plug-Ins/VST/ChartPreview.vst"
        echo ""
        echo "To test in REAPER:"
        echo "1. Open REAPER"
        echo "2. Scan for new plugins (Options > Preferences > VST)"
        echo "3. Load 'Chart Preview' as a VST2 plugin"
        echo "4. Look for green background tint indicating REAPER connection"
        echo ""
        echo "Debug output will show:"
        echo "- 'REAPER host detected via VST2!' when connected"
        echo "- Green background in debug builds"
        echo ""
    else
        echo "❌ Error: VST2 plugin not found at expected location"
        exit 1
    fi
else
    echo "❌ Build failed!"
    exit 1
fi

# Optionally open REAPER for testing
read -p "Would you like to open REAPER now for testing? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    # First check if REAPER test project exists
    REAPER_PROJECT="../../examples/reaper/reaper-test.rpp"
    if [ -f "$REAPER_PROJECT" ]; then
        echo "Opening REAPER test project..."
        open "$REAPER_PROJECT"
    else
        echo "Opening REAPER..."
        open -a REAPER
    fi
fi