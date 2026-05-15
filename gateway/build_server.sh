#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Configuring..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

echo "==> Building server..."
cmake --build . --target server -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "==> Done. Output:"
ls -lh ../output/server
