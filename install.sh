#!/bin/bash
# install.sh - Install the compiled binary
set -e

if [ ! -d "build" ]; then
    echo "Error: Build directory not found. Run ./build.sh first."
    exit 1
fi

cd build
echo "Installing Wine..."
sudo make install

echo "Installation complete!"