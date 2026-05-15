#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Cleaning..."
rm -rf build build_release output

echo "==> Rebuilding all (Debug)..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "==> Done. Output:"
ls -lh ../output/
