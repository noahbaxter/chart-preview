#!/bin/bash

# Quick build + REAPER restart for iterative testing.
# Runs build.sh, then quits REAPER cleanly and reopens it with the test project.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REAPER_TEST_PROJECT="$SCRIPT_DIR/examples/reaper/reaper-test.RPP"
REAPER_BIN="/Applications/REAPER.app/Contents/MacOS/REAPER"
ATTACHED=false

BUILD_ARGS=()
for arg in "$@"; do
    if [ "$arg" = "--attached" ]; then
        ATTACHED=true
    else
        BUILD_ARGS+=("$arg")
    fi
done

# Build (pass through any args like "release", "--vst3-only", etc.)
"$SCRIPT_DIR/build.sh" "${BUILD_ARGS[@]}"

# Force-quit REAPER — skips save prompts that block the graceful path
if pgrep -x REAPER > /dev/null; then
    echo "Killing REAPER..."
    pkill -9 -x REAPER
    sleep 0.5
fi

# Reopen
if [ -f "$REAPER_TEST_PROJECT" ]; then
    echo "Opening REAPER with test project..."
    if [ "$ATTACHED" = true ]; then
        exec "$REAPER_BIN" "$REAPER_TEST_PROJECT"
    else
        open -a "REAPER" "$REAPER_TEST_PROJECT"
    fi
else
    echo "Warning: test project not found at $REAPER_TEST_PROJECT"
    echo "Opening REAPER without project..."
    if [ "$ATTACHED" = true ]; then
        exec "$REAPER_BIN"
    else
        open -a "REAPER"
    fi
fi
