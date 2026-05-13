#!/bin/bash
set -e

mkdir -p build
mkdir -p logs

cd build
cmake ..
make -j$(nproc)
cd ..

echo "Build success. Run:"
echo "  ./build/gateway       # start the gateway server"
echo "  ./build/api_cli --demo # test with demo scenarios"
