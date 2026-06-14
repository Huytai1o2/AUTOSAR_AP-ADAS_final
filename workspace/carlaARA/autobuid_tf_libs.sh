#!/usr/bin/env bash

# --- color helpers (blue messages) ---
if [ -t 1 ]; then
  BLUE="$(printf '\033[1;34m')"  # bright blue
  RESET="$(printf '\033[0m')"
else
  BLUE=""; RESET=""
fi
say() { echo -e "${BLUE}$*${RESET}"; }

say "Step 1: Update system and install required packages"
cd "$HOME"
sudo apt update
sudo apt-get install build-essential git cmake libfarmhash-dev libpthreadpool-dev zip -y
rm -rf "abseil-cpp"
git clone "https://github.com/abseil/abseil-cpp.git"
cd abseil-cpp && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DABSL_ENABLE_INSTALL=ON -DCMAKE_CXX_STANDARD=17
cmake --build . -j"$(nproc)"
sudo cmake --install . --prefix /opt/absl

say "Step 2: Download TensorFlow source (tag v2.20.0)"
TF_VERSION="v2.20.0"
cd "$HOME"
rm -rf "tensorflow-${TF_VERSION#v}"
ARCHIVE_URL="https://github.com/tensorflow/tensorflow/archive/refs/tags/${TF_VERSION}.zip"
wget -O tensorflow.zip "${ARCHIVE_URL}"
unzip tensorflow.zip
rm tensorflow.zip
say "cd to tensorflow-${TF_VERSION#v} directory"
cd "tensorflow-${TF_VERSION#v}"  # change into tensorflow-2.20.0 directory

say "Step 3: Apply patch to CMake file if needed"
FILE="tensorflow/lite/tools/cmake/modules/ml_dtypes/CMakeLists.txt"
if grep -q '"${ML_DTYPES_SOURCE_DIR}"' "$FILE"; then
  say "→ Substituting block target_include_directories..."
  sed -i '/target_include_directories(ml_dtypes INTERFACE/,/ml_dtypes")/c\target_include_directories(ml_dtypes INTERFACE\
  "$<BUILD_INTERFACE:${ML_DTYPES_SOURCE_DIR}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"\
  "$<BUILD_INTERFACE:${ML_DTYPES_SOURCE_DIR}/ml_dtypes>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ml_dtypes>")' "$FILE"
  say "✓ Done"
else
  say "ML_DTYPES_SOURCE_DIR not found, skipping substitution."
fi

say "Step 4: Create build directory and run CMake"
mkdir -p tflite_build
cd tflite_build
say "→ Configuring using CMake..."
cmake ../tensorflow/lite \
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON \
  -DTFLITE_ENABLE_INSTALL=ON \
  -DCMAKE_C_FLAGS="-DTF_MAJOR_VERSION=2 -DTF_MINOR_VERSION=20 -DTF_PATCH_VERSION=0 -DTF_VERSION_SUFFIX=\"\"" \
  -DCMAKE_CXX_FLAGS="-DTF_MAJOR_VERSION=2 -DTF_MINOR_VERSION=20 -DTF_PATCH_VERSION=0 -DTF_VERSION_SUFFIX=\"\"" \
  -DSYSTEM_PTHREADPOOL=ON \
  -DSYSTEM_FARMHASH=ON \
  -Dabsl_DIR=/opt/absl/lib/cmake/absl \
  -DBUILD_SHARED_LIBS=OFF

cmake --build . -j"$(nproc)"

say "Step 5: Build and install the TFLite library"
say "→ Installing to folder tflite_libs"
mkdir -p tflite_libs
cmake --install . --prefix tflite_libs

# Fix the missing xnnpack-delegate library issue by copying it manually
if [ -f "libxnnpack-delegate.a" ]; then
  say "→ Copying missing libxnnpack-delegate.a to tflite_libs/lib/"
  cp "libxnnpack-delegate.a" "tflite_libs/lib/"
fi

say "→ Installing SDK to /opt/litert_x86_64_sdk"
sudo rm -rf /opt/litert_x86_64_sdk
sudo cp -r tflite_libs /opt/litert_x86_64_sdk

say "Build and install completed."
say "Installed to: /opt/litert_x86_64_sdk"
