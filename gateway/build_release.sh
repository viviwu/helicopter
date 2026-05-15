#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Configuring (Release)..."
mkdir -p build_release && cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "==> Building all targets..."
cmake --build . -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "==> Done. Output:"
ls -lh ../output/
