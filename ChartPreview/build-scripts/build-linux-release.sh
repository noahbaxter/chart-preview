#!/bin/bash

# Chart Preview - Linux Release Build Script
# Builds Linux VST3 plugin for release

set -e

# Configuration
PLUGIN_NAME="ChartPreview"
VERSION=${1:-"dev"}
RELEASE_DIR="./releases"
BUILD_CONFIG="Release"

echo "Building Chart Preview v${VERSION} for Linux..."

# Create release directory
mkdir -p "$RELEASE_DIR"
RELEASE_DIR=$(cd "$RELEASE_DIR" && pwd)  # Get absolute path

# Clean previous builds
echo "Cleaning previous builds..."
rm -rf "Builds/LinuxMakefile/build"

# Navigate to project directory (parent of build-scripts)
cd "$(dirname "$0")/.."

# Regenerate project files if needed
echo "Checking Projucer files..."
if [ -f "ChartPreview.jucer" ] && [ "ChartPreview.jucer" -nt "Builds/LinuxMakefile/Makefile" ]; then
    echo "Regenerating Linux Makefile from Projucer..."
    # Note: This would need Projucer installed on Linux
    # "/path/to/Projucer" --resave "ChartPreview.jucer"
fi

# Build Linux plugin
echo "Building Linux VST3..."
cd Builds/LinuxMakefile

make clean
make -j$(nproc) CONFIG=Release

# Create release bundle
echo "Creating Linux release bundle..."

# Linux Bundle
LINUX_BUNDLE="$RELEASE_DIR/ChartPreview-v${VERSION}-Linux"
mkdir -p "$LINUX_BUNDLE"

echo "  Packaging Linux bundle..."
cp -R "build/ChartPreview.vst3" "$LINUX_BUNDLE/"

# Create install script for Linux
cat > "$LINUX_BUNDLE/install-linux.sh" << 'EOF'
#!/bin/bash
echo "Installing Chart Preview VST3..."
mkdir -p ~/.vst3
cp -R ChartPreview.vst3 ~/.vst3/
echo "Installation complete!"
echo "VST3 installed to: ~/.vst3/"
EOF
chmod +x "$LINUX_BUNDLE/install-linux.sh"

# Create README file
cat > "$LINUX_BUNDLE/README.txt" << EOF
Chart Preview v${VERSION} - Linux Release  

This bundle contains:
- ChartPreview.vst3 (VST3 plugin)
- install-linux.sh (automatic installer script)

Installation:
1. Run ./install-linux.sh for automatic installation  
2. Or manually copy ChartPreview.vst3 to ~/.vst3/

Requirements: Linux x64 with ALSA/JACK
Built: $(date)
EOF

# Create ZIP archive
echo "Creating ZIP archive..."
cd "$RELEASE_DIR"
zip -r "ChartPreview-v${VERSION}-Linux.zip" "ChartPreview-v${VERSION}-Linux"

# Summary
echo ""
echo "✅ Linux build complete!"
echo "📦 Release bundle created:"
echo "   $RELEASE_DIR/ChartPreview-v${VERSION}-Linux.zip"
echo ""
echo "Bundle contents:"
echo "   VST3 plugin with installer"