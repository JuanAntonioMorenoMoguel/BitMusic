#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-Debug}"

if [ -z "${VCPKG_ROOT:-}" ]; then
    echo "VCPKG_ROOT is not set. Point it at your vcpkg checkout before building." >&2
    exit 1
fi

cmake --preset default
cmake --build build --config "$CONFIG"
