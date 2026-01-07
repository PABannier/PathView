#!/bin/bash
set -e

echo "========================================="
echo "PathView Unit Test Suite"
echo "========================================="
echo ""

# Build tests
echo "Building unit tests..."
JOBS=1
if command -v getconf >/dev/null 2>&1; then
  JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
elif command -v sysctl >/dev/null 2>&1; then
  JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 1)
fi
cmake --build build --target unit_tests -j"${JOBS}" 2>&1 | grep -E "(Building|Linking|Built target unit_tests|error|warning)" || true
echo ""

# Run tests
echo "Running unit tests..."
cd build
./test/unit_tests --gtest_color=yes
