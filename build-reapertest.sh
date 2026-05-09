#!/bin/bash

# Quick build + REAPER restart for iterative testing.
# Just run ./build-reapertest.sh — no arguments needed.
# Auto-cleans when CMake config is stale, always launches REAPER attached.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REAPER_TEST_PROJECT="$SCRIPT_DIR/examples/reaper/reaper-test.RPP"
REAPER_BIN="/Applications/REAPER.app/Contents/MacOS/REAPER"
BUILD_DIR="$SCRIPT_DIR/build"
CMAKE_CACHE="$BUILD_DIR/CMakeCache.txt"

# Auto-clean: if any CMakeLists.txt is newer than the cache, nuke build dir
if [ -f "$CMAKE_CACHE" ]; then
    if find "$SCRIPT_DIR" -name "CMakeLists.txt" -newer "$CMAKE_CACHE" | grep -q .; then
        echo "CMake config changed — cleaning build dir..."
        rm -rf "$BUILD_DIR"
    fi
fi

# Build
"$SCRIPT_DIR/build.sh" "$@"

# Kill REAPER
if pgrep -x REAPER > /dev/null; then
    echo "Killing REAPER..."
    pkill -9 -x REAPER
    sleep 0.5
fi

# Launch attached
if [ -f "$REAPER_TEST_PROJECT" ]; then
    echo "Opening REAPER with test project..."
    exec "$REAPER_BIN" "$REAPER_TEST_PROJECT"
else
    echo "Warning: test project not found at $REAPER_TEST_PROJECT"
    echo "Opening REAPER without project..."
    exec "$REAPER_BIN"
fi
