#!/bin/bash

# Build script for HFT Trading System

set -e

echo "======================================"
echo "  Building HFT Trading System"
echo "======================================"
echo ""

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
echo ""
echo "Building..."
make -j$(nproc)

echo ""
echo "======================================"
echo "  Build Complete!"
echo "======================================"
echo ""
echo "Executables:"
echo "  ./build/hft_trading    - Main trading system"
echo "  ./build/benchmark      - Performance benchmarks"
echo "  ./build/tests          - Unit tests"
echo ""
echo "To run:"
echo "  cd build && ./hft_trading --config ../config/trading.conf"
echo ""
