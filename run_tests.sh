#!/bin/bash
set -e

echo "========================================="
echo "PathView Unit Test Suite"
echo "========================================="
echo ""

# Build tests
echo "Building unit tests..."
cmake --build build --target unit_tests -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "(Building|Linking|Built target unit_tests|error|warning)" || true
echo ""

# Run tests
echo "Running unit tests..."
cd build
./test/unit_tests --gtest_color=yes
