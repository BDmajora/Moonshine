#!/bin/bash
# build.sh - Configure and compile Wine
set -e

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

echo "Configuring Wine with Win64 and Wayland support..."
../configure --enable-win64 --with-wayland

echo "Starting build with $(nproc) cores..."
make -j$(nproc)

echo "Build finished successfully!"