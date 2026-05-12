#!/bin/bash
set -e

mkdir -p build
mkdir -p logs

cd build
cmake ..
make -j$(nproc)
cd ..

echo "Build success. Run ./build/gateway"
