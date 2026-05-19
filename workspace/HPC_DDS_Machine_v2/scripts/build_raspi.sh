# #!/bin/bash
# set -e

# # Environment setup for para-sdk_raspi-raspios64_bookworm
# source /opt/para-sdk_raspi-raspios64_bookworm/para-env-setup.sh

# # Export Toolchain Path
# export PARA_TOOLCHAIN_PATH=/opt/cross-pi-gcc-12.2.0-64

# # CMake configuration
# echo "Cleaning build directory..."
# rm -rf build
# echo "Configuring CMake..."
# cmake -B build -DPARA_DDS_IMPLEMENTATION=cyclonedds-cxx .
# # cmake -B build .

# # Build and install
# echo "Building and Installing..."
# cmake --build build --target install -j$(nproc)


#!/bin/bash
set -e

# Environment setup for para-sdk_raspi-raspios64_bookworm
source /opt/para-sdk_raspi-raspios64_bookworm/para-env-setup.sh

# Export Toolchain Path
export PARA_TOOLCHAIN_PATH=/opt/cross-pi-gcc-12.2.0-64

# CycloneDDS runtime library path (để idlc chạy được)
export LD_LIBRARY_PATH=/opt/para-sdk_raspi-raspios64_bookworm/share/cyclonedds/lib:$LD_LIBRARY_PATH

# CMake configuration
echo "Cleaning build directory..."
rm -rf build

echo "Configuring CMake..."
cmake -B build \
    -DPARA_DDS_IMPLEMENTATION=cyclonedds-cxx \
    .

# Build and install
echo "Building and Installing..."
cmake --build build --target install -j$(nproc)
