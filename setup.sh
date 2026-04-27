#!/bin/bash
# setup.sh - Install build dependencies for Wine-Wayland
set -e

echo "Updating package lists..."
sudo apt update

echo "Installing build dependencies..."
sudo apt install -y \
  build-essential flex bison pkg-config \
  mingw-w64 wayland-protocols \
  libwayland-dev libxkbcommon-dev libxkbregistry-dev \
  libvulkan-dev libegl1-mesa-dev libgbm-dev \
  libfreetype-dev libfontconfig-dev libgnutls28-dev \
  libdbus-1-dev libncurses-dev libunwind-dev \
  gcc-multilib g++-multilib \
  libmpfr-dev libgmp-dev

echo "Setup complete!"