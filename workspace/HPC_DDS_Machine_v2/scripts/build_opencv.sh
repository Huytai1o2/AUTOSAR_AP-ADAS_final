#!/bin/bash
set -e

# Configuration
OPENCV_VERSION="4.8.0"
SDK_PATH="/opt/para-sdk_raspi-raspios64_bookworm"
TOOLCHAIN_FILE="${SDK_PATH}/share/cmake/toolchain/aarch64-linux-gnu.toolchain.cmake"
INSTALL_PREFIX="$(pwd)/deps/opencv"
BUILD_DIR="build_opencv"

# Export toolchain path required by the toolchain file
export PARA_TOOLCHAIN_PATH="/opt/cross-pi-gcc-12.2.0-64"

# Clean previous build
rm -rf opencv ${BUILD_DIR}

# Download and extract OpenCV
echo "Downloading OpenCV ${OPENCV_VERSION}..."
wget -O opencv.tar.gz "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz"
echo "Extracting OpenCV..."
mkdir -p opencv
tar -xf opencv.tar.gz -C opencv --strip-components=1
rm opencv.tar.gz

# Create build directory
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure CMake
# We disable many modules to keep the build light and focus on core functionality
echo "Configuring OpenCV..."
cmake -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} \
      -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_PERF_TESTS=OFF \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_DOCS=OFF \
      -DBUILD_JAVA=OFF \
      -DBUILD_PYTHON3=OFF \
      -DWITH_GTK=OFF \
      -DWITH_QT=OFF \
      -DWITH_FFMPEG=OFF \
      -DWITH_GSTREAMER=OFF \
      -DWITH_V4L=OFF \
      ../opencv

# Build and Install
echo "Building OpenCV (this may take a while)..."
cmake --build . -j$(nproc)
cmake --install .

echo "OpenCV installed to ${INSTALL_PREFIX}"
