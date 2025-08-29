#!/bin/bash

# Chart Preview - macOS Release Build Script
# Builds macOS VST3 + AU plugins for release

set -e

# Configuration
PLUGIN_NAME="ChartPreview"
VERSION=${1:-"dev"}
RELEASE_DIR="./releases"
BUILD_CONFIG="Release"

echo "Building Chart Preview v${VERSION} for macOS..."

# Create release directory
mkdir -p "$RELEASE_DIR"
RELEASE_DIR=$(cd "$RELEASE_DIR" && pwd)  # Get absolute path

# Clean previous builds
echo "Cleaning previous builds..."
rm -rf "Builds/MacOSX/build"

# Navigate to project directory (parent of build-scripts)
cd "$(dirname "$0")/.."

# Regenerate project files if needed
echo "Checking Projucer files..."
if [ -f "ChartPreview.jucer" ] && [ "ChartPreview.jucer" -nt "Builds/MacOSX/ChartPreview.xcodeproj" ]; then
    echo "Regenerating Xcode project from Projucer..."
    "/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "ChartPreview.jucer"
fi

# Build macOS plugins
echo "Building macOS plugins..."
cd Builds/MacOSX

# Build VST3 for macOS
echo "  Building macOS VST3..."
xcodebuild -quiet -project ChartPreview.xcodeproj -target "ChartPreview - VST3" -configuration $BUILD_CONFIG

# Build AU for macOS  
echo "  Building macOS AU..."
xcodebuild -quiet -project ChartPreview.xcodeproj -target "ChartPreview - AU" -configuration $BUILD_CONFIG

# Create release bundle
echo "Creating macOS release bundle..."

# macOS Bundle
MACOS_BUNDLE="$RELEASE_DIR/ChartPreview-v${VERSION}-macOS"
mkdir -p "$MACOS_BUNDLE"

echo "  Packaging macOS bundle..."
cp -R "build/$BUILD_CONFIG/ChartPreview.vst3" "$MACOS_BUNDLE/"
cp -R "build/$BUILD_CONFIG/ChartPreview.component" "$MACOS_BUNDLE/"

# Create install script for macOS
cat > "$MACOS_BUNDLE/install-macos.sh" << 'EOF'
#!/bin/bash
echo "Installing Chart Preview plugins..."
cp -R ChartPreview.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R ChartPreview.component ~/Library/Audio/Plug-Ins/Components/
echo "Installation complete!"
echo "VST3 installed to: ~/Library/Audio/Plug-Ins/VST3/"
echo "AU installed to: ~/Library/Audio/Plug-Ins/Components/"
EOF
chmod +x "$MACOS_BUNDLE/install-macos.sh"

# Create README file
cat > "$MACOS_BUNDLE/README.txt" << EOF
Chart Preview v${VERSION} - macOS Release

This bundle contains:
- ChartPreview.vst3 (VST3 plugin)
- ChartPreview.component (Audio Unit plugin)
- install-macos.sh (automatic installer script)

Installation:
1. Run ./install-macos.sh for automatic installation
2. Or manually copy plugins to:
   - VST3: ~/Library/Audio/Plug-Ins/VST3/
   - AU: ~/Library/Audio/Plug-Ins/Components/

Requirements: macOS 10.15 or later
Built: $(date)
EOF

# Create ZIP archive
echo "Creating ZIP archive..."
cd "$RELEASE_DIR"
zip -r "ChartPreview-v${VERSION}-macOS.zip" "ChartPreview-v${VERSION}-macOS"

# Summary
echo ""
echo "✅ macOS build complete!"
echo "📦 Release bundle created:"
echo "   $RELEASE_DIR/ChartPreview-v${VERSION}-macOS.zip"
echo ""
echo "Bundle contents:"
echo "   VST3 + AU plugins with installer"
