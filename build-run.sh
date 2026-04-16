#!/bin/bash
# Build and run script for Home S3 Storage
# Fixes for actual project structure

set -e  # Exit on error

# Change to script directory (project root)
cd "$(dirname "$0")"

echo "Building frontend..."
cd web
if [ ! -d "node_modules" ]; then
    echo "Installing npm dependencies..."
    npm install
fi
npm run build
cd ..

echo "Building backend..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo "Starting server..."
./s3_server --port 9000 --storage ./storage --no-auth