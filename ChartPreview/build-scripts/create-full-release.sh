#!/bin/bash

# Chart Preview - Full Release Creation Script
# Combines builds from macOS and Windows machines into a single release

set -e

VERSION=${1:-"dev"}
RELEASE_DIR="../releases"
FINAL_RELEASE_DIR="$RELEASE_DIR/ChartPreview-v${VERSION}-Complete"

echo "Creating Chart Preview v${VERSION} complete release bundle..."

# Check if all platform builds exist
REQUIRED_FILES=(
    "$RELEASE_DIR/ChartPreview-v${VERSION}-macOS.zip"
    "$RELEASE_DIR/ChartPreview-v${VERSION}-Linux.zip" 
    "$RELEASE_DIR/ChartPreview-v${VERSION}-Windows.zip"
)

echo "Checking for required build files..."
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "❌ Missing: $file"
        echo ""
        echo "Please build all platforms first:"
        echo "  macOS + Linux: ./build-release-macos.sh $VERSION"
        echo "  Windows: build-release-windows.bat $VERSION"
        exit 1
    else
        echo "✅ Found: $(basename "$file")"
    fi
done

# Create final release directory
rm -rf "$FINAL_RELEASE_DIR"
mkdir -p "$FINAL_RELEASE_DIR"

# Copy all platform builds
echo ""
echo "Creating complete release bundle..."
cp "$RELEASE_DIR/ChartPreview-v${VERSION}-macOS.zip" "$FINAL_RELEASE_DIR/"
cp "$RELEASE_DIR/ChartPreview-v${VERSION}-Linux.zip" "$FINAL_RELEASE_DIR/"
cp "$RELEASE_DIR/ChartPreview-v${VERSION}-Windows.zip" "$FINAL_RELEASE_DIR/"

# Create master README
cat > "$FINAL_RELEASE_DIR/README.txt" << EOF
Chart Preview v${VERSION} - Complete Release Package

This package contains Chart Preview builds for all supported platforms:

📦 Platform Packages:
- ChartPreview-v${VERSION}-macOS.zip    - macOS VST3 + AU plugins
- ChartPreview-v${VERSION}-Linux.zip    - Linux VST3 plugin  
- ChartPreview-v${VERSION}-Windows.zip  - Windows VST3 plugin

🎯 Quick Start:
1. Download the appropriate package for your platform
2. Extract the ZIP file
3. Run the included install script (install-*.sh or install-*.bat)
4. Load Chart Preview in your DAW

💻 System Requirements:
- macOS: 10.15 or later (Universal Binary: Intel + Apple Silicon)
- Linux: x64 with ALSA/JACK support
- Windows: 10 or later (x64)

🎸 Supported Features:
- Guitar: Expert/Hard/Medium/Easy difficulties
- Drums: Pro Drums, Normal Drums with ghost notes and accents  
- Real-time MIDI visualization
- Customizable latency compensation
- Star Power, HOPOs, sustain notes
- Multiple zoom levels and frame rates

📖 Documentation & Support:
- GitHub: https://github.com/noahbaxter/chart-preview
- Report issues: https://github.com/noahbaxter/chart-preview/issues

Built: $(date)
Builder: $(whoami)@$(hostname)
EOF

# Create version info file
cat > "$FINAL_RELEASE_DIR/VERSION.txt" << EOF
Chart Preview Release v${VERSION}

Build Information:
- Version: ${VERSION}
- Build Date: $(date)
- Build Host: $(whoami)@$(hostname)
- Git Commit: $(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
- Git Branch: $(git branch --show-current 2>/dev/null || echo "unknown")

Platform Support:
- macOS: Universal Binary (Intel + Apple Silicon)
- Linux: x86_64
- Windows: x64

Package Contents:
$(ls -la "$FINAL_RELEASE_DIR"/*.zip | awk '{print "- " $9 " (" $5 " bytes)"}')
EOF

# Create final archive
echo "Creating final release archive..."
cd "$RELEASE_DIR"
zip -r "ChartPreview-v${VERSION}-Complete.zip" "ChartPreview-v${VERSION}-Complete"

# Calculate sizes
TOTAL_SIZE=$(du -sh "ChartPreview-v${VERSION}-Complete" | cut -f1)
ARCHIVE_SIZE=$(du -sh "ChartPreview-v${VERSION}-Complete.zip" | cut -f1)

echo ""
echo "🎉 Complete release created!"
echo ""
echo "📦 Final Release:"
echo "   Location: $RELEASE_DIR/ChartPreview-v${VERSION}-Complete.zip"
echo "   Bundle Size: $TOTAL_SIZE"
echo "   Archive Size: $ARCHIVE_SIZE"
echo ""
echo "📋 Contents:"
echo "   - macOS VST3 + AU plugins"
echo "   - Linux VST3 plugin"  
echo "   - Windows VST3 plugin"
echo "   - Installation scripts for all platforms"
echo "   - Documentation and version info"
echo ""
echo "✅ Ready for distribution!"