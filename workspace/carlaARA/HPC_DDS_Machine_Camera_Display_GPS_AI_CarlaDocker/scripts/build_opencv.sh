#!/bin/bash
set -e

# ─────────────────────────── CONFIGURATION ────────────────────────────────
OPENCV_VERSION="4.8.0"
FFMPEG_VERSION="6.1.2"

SDK_PATH="/opt/para-sdk_raspi-raspios64_bookworm"
TOOLCHAIN_FILE="${SDK_PATH}/share/cmake/toolchain/aarch64-linux-gnu.toolchain.cmake"

export PARA_TOOLCHAIN_PATH="/opt/cross-pi-gcc-12.2.0-64"
CROSS="${PARA_TOOLCHAIN_PATH}/bin/aarch64-linux-gnu-"

DEPS_DIR="$(pwd)/deps"
FFMPEG_PREFIX="${DEPS_DIR}/ffmpeg"
OPENCV_PREFIX="${DEPS_DIR}/opencv"
BUILD_DIR="build_opencv"

# ─────────────────────────── STEP 1: FFMPEG FOR ARM64 ─────────────────────
# Cross-compile FFmpeg statically so OpenCV can find it via pkg-config.
# Skip if already built.

if [ ! -f "${FFMPEG_PREFIX}/lib/libavcodec.a" ]; then
    echo "=== Building FFmpeg ${FFMPEG_VERSION} for aarch64 ==="

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
        --cross-prefix="${CROSS}" \
        --arch=aarch64 \
        --target-os=linux \
        --enable-cross-compile \
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

    echo "=== FFmpeg built → ${FFMPEG_PREFIX} ==="
else
    echo "=== FFmpeg already built, skipping ==="
fi

# OpenCV disables PKG_CONFIG_PATH during cross-compile to avoid using host libs.
# PKG_CONFIG_LIBDIR (replaces default search path) is the correct variable to use —
# OpenCV allows it even in cross-compile mode.
export PKG_CONFIG_LIBDIR="${FFMPEG_PREFIX}/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""   # prevent pkg-config from prepending a sysroot

# ─────────────────────────── STEP 2: OPENCV FOR ARM64 ────────────────────

echo "=== Building OpenCV ${OPENCV_VERSION} for aarch64 ==="

rm -rf opencv ${BUILD_DIR}

wget -O opencv.tar.gz \
    "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz"
mkdir -p opencv
tar -xf opencv.tar.gz -C opencv --strip-components=1
rm opencv.tar.gz

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# CMAKE_FIND_ROOT_PATH lets CMake's find_library() locate ARM64 FFmpeg libs
# even though the toolchain sets CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY.
# PKG_CONFIG_PATH (set above) lets pkg_check_modules find the .pc files.
cmake -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} \
      -DCMAKE_INSTALL_PREFIX=${OPENCV_PREFIX} \
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

echo "=== OpenCV installed → ${OPENCV_PREFIX} ==="
