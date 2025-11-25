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
How to compile: *comming soon*

export LD_LIBRARY_PATH="$HOME/Developer/litertProj/litert_arm_sdk/lib:$LD_LIBRARY_PATH"

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
