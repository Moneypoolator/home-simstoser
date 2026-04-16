#!/bin/bash
# Test runner script for Home S3 Storage
# Fixes for actual project structure

set -e  # Exit on error

# Change to script directory (project root)
cd "$(dirname "$0")"

echo "Building tests..."
mkdir -p build && cd build
cmake ..
make -j$(nproc)

echo "Running unit tests..."
./backend/tests/unit_tests

echo "Running integration tests..."
./backend/tests/integration_tests

# Optional: detailed output
# ./backend/tests/unit_tests --gtest_output=xml:unit_tests.xml
# ./backend/tests/integration_tests --gtest_filter="ServerIntegrationTest.*"