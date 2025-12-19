# AUTOSAR_AP-ADAS

## Members
@l1ttled1no
@Huytai1o2

## Update 25/11/2025
Update the source code of HPC_Machine_v3, including:
1. **Environment:** import opencv via `apt-get install -y libopencv-dev`
2. **Build System:** update CmakeLists in

    2.1. [CmakeLists of whole project](./workspace/HPC_Machine_v3/CMakeLists.txt)

    2.2. [CmakeLists of ProviderSensor Application](./workspace/HPC_Machine_v3/ProviderSensor/src/CMakeLists.txt)

    2.3. [CmakeLists of ClientSensor Application](./workspace/HPC_Machine_v3/ClientSensor/src/CMakeLists.txt)

## Update 10/11/2025
Test LiteRT with model .tflite to evaluate between yolo.ncnn and yolo-mobilenetv4.tflite
Requirement:
1. ARM toolchain: google search
2. Must have flatbuffer in /usr/local/bin and flatbuffer version in host machine **should be as same as the version requirement** in tensorflow src (i forget how to fix version, but while compiling, it will print the log to help you fix =))

Before compiling:
Change the source code CMakeLists in `tensorflow/lite/tools/cmake/modules/ml_dtypes/CMakeLists.txt`

- target_include_directories(ml_dtypes INTERFACE
- "${ML_DTYPES_SOURCE_DIR}"
- "${ML_DTYPES_SOURCE_DIR}/ml_dtypes")
+ target_include_directories(ml_dtypes INTERFACE
+  "$<BUILD_INTERFACE:${ML_DTYPES_SOURCE_DIR}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
+  "$<BUILD_INTERFACE:${ML_DTYPES_SOURCE_DIR}/ml_dtypes>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ml_dtypes>")

Change the version of Flatbuffer in source code

How to compile:

`ARM_TOOLCHAIN="$HOME/toolchains/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-linux-gnu/bin/" ARMCC_FLAGS="-funsafe-math-optimizations"`

`git clone https://github.com/tensorflow/tensorflow.git tensorflow_src && cd tensorflow_src && mkdir tflite_build && cd tflite_build`

`cmake   -DCMAKE_C_COMPILER="$ARM_TOOLCHAIN/aarch64-none-linux-gnu-gcc"   -DCMAKE_CXX_COMPILER="$ARM_TOOLCHAIN/aarch64-none-linux-gnu-g++"   -DCMAKE_SYSTEM_NAME=Linux   -DCMAKE_SYSTEM_PROCESSOR=aarch64   -DCMAKE_BUILD_TYPE=Release   -DTFLITE_HOST_TOOLS_DIR=/usr/local/bin   -DCMAKE_INSTALL_PREFIX=$HOME/tflite_build/install -DCMAKE_C_FLAGS="${ARMCC_FLAGS}"  -DCMAKE_CXX_FLAGS="-DTF_MAJOR_VERSION=2 -DTF_MINOR_VERSION=20 -DTF_PATCH_VERSION=0 -DTF_VERSION_SUFFIX=\"\" ${ARMCC_FLAGS}"   -DABSL_ENABLE_INSTALL=ON   -DTFLITE_ENABLE_INSTALL=ON   -DRUY_ENABLE_INSTALL=ON -DEIGEN_BUILD_CMAKE_PACKAGE=ON -DFETCHCONTENT_FULLY_DISCONNECTED=OFF -DBUILD_SHARED_LIBS=ON ../tensorflow_src/tensorflow/lite/`

`cmake --build . -j$(nproc) && cmake --install .`

In pi5:

export LD_LIBRARY_PATH="$HOME/Developer/litertProj/litert_arm_sdk/lib:$LD_LIBRARY_PATH"


for x86_64 (dockeer):
In docker:
`cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_CXX_FLAGS="-DTF_MAJOR_VERSION=2 -DTF_MINOR_VERSION=20 -DTF_PATCH_VERSION=0 -DTF_VERSION_SUFFIX=\"\"" -DABSL_ENABLE_INSTALL=ON   -DTFLITE_ENABLE_INSTALL=ON   -DRUY_ENABLE_INSTALL=ON -DEIGEN_BUILD_CMAKE_PACKAGE=ON -DFETCHCONTENT_FULLY_DISCONNECTED=OFF -DBUILD_SHARED_LIBS=ON ../tensorflow_src/tensorflow/lite`

`export LD_LIBRARY_PATH="/opt/litert_x86_64_sdk/lib/:$LD_LIBRARY_PATH"`

## Update 08/11/2025
We decide change the LiteRT to NCNN becasue of supporting embedded comming soon =)) of LiteRTv2
[Prototype of run NCNN framework with YOLO](./ncnnCpp/README.md)

## Update 01/11/2025
We made the docker-compose.yaml for easily editing the COM, ip address, mount camera and etc within PARA environment
[How to run docker-compose.yaml](./setup/README)

## Update 23/10/2025 and before
### Remote tunnel ssh
https://vscode.dev/tunnel/raspberrypi/home/huytai102/Developer

### How to build LiteRT
Pre-codidtions: Ubuntu 24.03 x86-64

then run these command:
`source autobuild_tf_libs.sh`

and it will be install to home path
`cd ~`
