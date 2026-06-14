#!/bin/bash
set -e

# ─────────────────────────── CONFIGURATION ────────────────────────────────
OPENCV_VERSION="4.8.0"
FFMPEG_VERSION="6.1.2"

SDK_PATH="/opt/para-r2011_25.10.31_with-ucm-ucm_sm-vucm_native-linux-gcc_release"

DEPS_DIR="$(pwd)/deps"
FFMPEG_PREFIX="${DEPS_DIR}/ffmpeg"
OPENCV_PREFIX="${DEPS_DIR}/opencv"
OPENCV_BUILD_DIR="build_opencv"

# ─────────────────────────── STEP 0: ENVIRONMENT SETUP ────────────────────
echo "=== Setting up PARA SDK Environment ==="
if [ ! -f "${SDK_PATH}/para-env-setup.sh" ]; then
    echo "ERROR: SDK not found at ${SDK_PATH}"
    exit 1
fi
source "${SDK_PATH}/para-env-setup.sh"

# Patch DDSCXX headers to fix compilation issues with newer C++ standards
echo "=== Patching DDSCXX headers in SDK ==="
sed -i 's/~Reference<DELEGATE>()/~Reference()/g' "${SDK_PATH}/external/include/ddscxx/dds/core/detail/ReferenceImpl.hpp"
sed -i 's/~Topic<T>()/~Topic()/g' "${SDK_PATH}/external/include/ddscxx/dds/topic/detail/TTopicImpl.hpp"
sed -i 's/~DataReader<T>()/~DataReader()/g' "${SDK_PATH}/external/include/ddscxx/dds/sub/detail/TDataReaderImpl.hpp"
sed -i 's/~DataWriter<T>()/~DataWriter()/g' "${SDK_PATH}/external/include/ddscxx/dds/pub/detail/DataWriterImpl.hpp"

# CycloneDDS runtime library path (so idlc can run)
export LD_LIBRARY_PATH="${SDK_PATH}/share/cyclonedds/lib:$LD_LIBRARY_PATH"

# ─────────────────────────── STEP 1: FFMPEG FOR NATIVE (x86_64) ─────────────
# Compile FFmpeg statically so OpenCV can find it via pkg-config.
# Skip if already built.
if [ ! -f "${FFMPEG_PREFIX}/lib/libavcodec.a" ]; then
    echo "=== Building FFmpeg ${FFMPEG_VERSION} for native (x86_64) ==="

    rm -rf ffmpeg ffmpeg.tar.bz2
    wget -O ffmpeg.tar.bz2 \
        "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2"
    mkdir -p ffmpeg
    tar -xf ffmpeg.tar.bz2 -C ffmpeg --strip-components=1
    rm ffmpeg.tar.bz2

    mkdir -p ffmpeg_build
    cd ffmpeg_build

    ../ffmpeg/configure \
        --prefix="${FFMPEG_PREFIX}" \
        --enable-static \
        --disable-shared \
        --enable-pic \
        --disable-programs \
        --disable-doc \
        --disable-debug \
        --disable-network \
        --disable-x86asm

    make -j$(nproc)
    make install
    cd ..
    rm -rf ffmpeg_build ffmpeg

    echo "=== FFmpeg built → ${FFMPEG_PREFIX} ==="
else
    echo "=== FFmpeg already built, skipping ==="
fi

# Set package config path for OpenCV build to find FFmpeg
export PKG_CONFIG_LIBDIR="${FFMPEG_PREFIX}/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""   # prevent pkg-config from prepending a sysroot

# ─────────────────────────── STEP 2: OPENCV FOR NATIVE (x86_64) ─────────────
# Skip if already built.
if [ ! -f "${OPENCV_PREFIX}/lib/libopencv_core.a" ]; then
    echo "=== Building OpenCV ${OPENCV_VERSION} for native (x86_64) ==="

    rm -rf opencv ${OPENCV_BUILD_DIR}

    wget -O opencv.tar.gz \
        "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz"
    mkdir -p opencv
    tar -xf opencv.tar.gz -C opencv --strip-components=1
    rm opencv.tar.gz

    mkdir -p ${OPENCV_BUILD_DIR}
    cd ${OPENCV_BUILD_DIR}

    cmake -DCMAKE_INSTALL_PREFIX="${OPENCV_PREFIX}" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_FIND_ROOT_PATH="${FFMPEG_PREFIX}" \
          -DBUILD_SHARED_LIBS=OFF \
          -DBUILD_TESTS=OFF \
          -DBUILD_PERF_TESTS=OFF \
          -DBUILD_EXAMPLES=OFF \
          -DBUILD_DOCS=OFF \
          -DBUILD_JAVA=OFF \
          -DBUILD_PYTHON3=OFF \
          -DWITH_GTK=OFF \
          -DWITH_QT=OFF \
          -DWITH_FFMPEG=ON \
          -DWITH_GSTREAMER=OFF \
          -DWITH_V4L=ON \
          -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config \
          -DOPENCV_FFMPEG_SKIP_BUILD_CHECK=ON \
          ../opencv

    cmake --build . -j$(nproc)
    cmake --install .
    cd ..
    rm -rf ${OPENCV_BUILD_DIR} opencv

    echo "=== OpenCV built → ${OPENCV_PREFIX} ==="
else
    echo "=== OpenCV already built, skipping ==="
fi

# ─────────────────────────── STEP 3: BUILD APPLICATION ───────────────────
echo "=== Building and Installing HPC DDS Machine (Native x86_64) ==="

# Clean build directory
echo "Cleaning build directory..."
rm -rf build

echo "Configuring CMake..."
cmake -B build \
    -DPARA_DDS_IMPLEMENTATION=cyclonedds-cxx \
    .

# Build and install
echo "Building and Installing..."
cmake --build build --target install -j$(nproc)

# Copy post-build files
cp show_cam.py build/install/
cp ./scripts/roudi_config.toml build/install/etc/ipc/roudi_config.toml
cp ./scripts/run.sh build/install/

echo "=== Build and installation completed successfully ==="
