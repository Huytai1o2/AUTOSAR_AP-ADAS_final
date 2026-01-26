#!/bin/bash

# Build script for Traffic Light Detector using NCNN
# This script compiles the traffic_light_detector.cpp with NCNN and OpenCV

set -e  # Exit on error

echo "========================================"
echo "Building Traffic Light Detector"
echo "========================================"

# Configuration
PROJECT_DIR="$(pwd)"
BUILD_DIR="$PROJECT_DIR/build_detector"
NCNN_BUILD="$PROJECT_DIR/build"
SOURCE_FILE="$PROJECT_DIR/traffic_light_detector.cpp"
OUTPUT_BINARY="traffic_light_detector"

# Check if source file exists
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file not found: $SOURCE_FILE"
    exit 1
fi

# Check if NCNN is built, if not, build it automatically
if [ ! -d "$NCNN_BUILD" ] || [ ! -f "$NCNN_BUILD/src/libncnn.a" ]; then
    echo "========================================"
    echo "NCNN not found. Building NCNN first..."
    echo "========================================"
    
    mkdir -p "$NCNN_BUILD"
    cd "$NCNN_BUILD"
    
    echo "Running CMake..."
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DNCNN_VULKAN=OFF \
          -DNCNN_BUILD_EXAMPLES=ON \
          ..
    
    if [ $? -ne 0 ]; then
        echo "Error: CMake configuration failed"
        exit 1
    fi
    
    echo "Building NCNN (this may take a few minutes)..."
    make -j$(nproc)
    
    if [ $? -ne 0 ]; then
        echo "Error: NCNN build failed"
        exit 1
    fi
    
    echo "NCNN build completed successfully!"
    cd "$PROJECT_DIR"
fi

# Create build directory
echo "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Find NCNN library and includes
NCNN_INCLUDE="$PROJECT_DIR/src"
NCNN_BUILD_INCLUDE="$NCNN_BUILD/src"  # For generated headers like platform.h
NCNN_LIB_DIR="$NCNN_BUILD/src"

if [ ! -f "$NCNN_LIB_DIR/libncnn.a" ]; then
    echo "Error: NCNN library not found: $NCNN_LIB_DIR/libncnn.a"
    echo "Please build NCNN first"
    exit 1
fi

echo "NCNN Include: $NCNN_INCLUDE"
echo "NCNN Build Include: $NCNN_BUILD_INCLUDE"
echo "NCNN Library: $NCNN_LIB_DIR"

# Detect OpenCV
echo "Detecting OpenCV..."
if pkg-config --exists opencv4; then
    OPENCV_CFLAGS=$(pkg-config --cflags opencv4)
    OPENCV_LIBS=$(pkg-config --libs opencv4)
    echo "Found OpenCV 4"
elif pkg-config --exists opencv; then
    OPENCV_CFLAGS=$(pkg-config --cflags opencv)
    OPENCV_LIBS=$(pkg-config --libs opencv)
    echo "Found OpenCV"
else
    echo "Error: OpenCV not found. Please install OpenCV:"
    echo "  sudo apt install libopencv-dev"
    exit 1
fi

# Compile
echo "Compiling $SOURCE_FILE..."
g++ -g -O0 -std=c++11 \
    "$SOURCE_FILE" \
    -I"$NCNN_INCLUDE" \
    -I"$NCNN_BUILD_INCLUDE" \
    $OPENCV_CFLAGS \
    -L"$NCNN_LIB_DIR" \
    -lncnn \
    $OPENCV_LIBS \
    -lpthread \
    -fopenmp \
    -o "$OUTPUT_BINARY"

if [ $? -eq 0 ]; then
    echo "========================================"
    echo "Build successful!"
    echo "Binary location: $BUILD_DIR/$OUTPUT_BINARY"
    echo "========================================"
    echo ""
    echo "Usage example:"
    echo "  cd $BUILD_DIR"
    echo "  ./$OUTPUT_BINARY ../model/ssd_mobilenet_v3.param ../model/ssd_mobilenet_v3.bin ../Traffic-Light-1/test/[image.jpg]"
    echo ""
else
    echo "========================================"
    echo "Build failed!"
    echo "========================================"
    exit 1
fi
