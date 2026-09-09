#!/bin/bash
# Common helpers for scripts
# Source this file: source "$(dirname "$0")/_common.sh"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"
TESTS_DIR="$PROJECT_ROOT/tests"
VENV_DIR="$PROJECT_ROOT/.venv"
PLUGIN_NAME="Chartchotic"

# Ensure Python venv exists and is activated
ensure_venv() {
    if [ ! -d "$VENV_DIR" ]; then
        echo -e "${YELLOW}Creating Python virtual environment...${NC}"
        python3 -m venv "$VENV_DIR"
    fi

    source "$VENV_DIR/bin/activate"

    if [ -f "$PROJECT_ROOT/requirements.txt" ]; then
        pip install -q -r "$PROJECT_ROOT/requirements.txt"
    fi
}

# Get CPU count for parallel builds
cpu_count() {
    sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4
}
