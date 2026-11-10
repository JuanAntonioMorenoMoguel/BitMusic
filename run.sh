#!/usr/bin/env bash
set -euo pipefail

EXE=$(find build -maxdepth 2 -type f -name "BitMusic" 2>/dev/null | head -n1)

if [ -z "$EXE" ]; then
    echo "Executable not found. Run build.sh first." >&2
    exit 1
fi

# The exe finds assets/ and lang/ in the source folder while developing, or next to itself
# (where CMake copies them post-build).
EXE_DIR=$(dirname "$EXE")
EXE_NAME=$(basename "$EXE")
(cd "$EXE_DIR" && "./$EXE_NAME" "$@")
