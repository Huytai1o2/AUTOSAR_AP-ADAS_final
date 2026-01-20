#!/bin/bash
set -e

# Environment setup for para-sdk_raspi-raspios64_bookworm
source /opt/para-sdk_raspi-raspios64_bookworm/para-env-setup.sh

# Export Toolchain Path
export PARA_TOOLCHAIN_PATH=/opt/cross-pi-gcc-12.2.0-64

# CMake configuration
echo "Cleaning build directory..."
rm -rf build
echo "Configuring CMake..."
cmake -B build .

# Build and install
echo "Building and Installing..."
cmake --build build --target install -j$(nproc)
