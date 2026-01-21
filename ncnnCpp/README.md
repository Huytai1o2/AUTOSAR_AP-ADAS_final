# About NCNN

**NCNN** is a high-performance neural network inference framework optimized for mobile and embedded platforms. Developed by Tencent, it provides a lightweight, dependency-free C++ implementation that runs efficiently on CPUs and **GPUs (via Vulkan).** NCNN supports a wide range of deep learning models — including **YOLO,** MobileNet, and ResNet — and is widely used for deploying AI models on devices such as Android, **Raspberry Pi,** and embedded boards. Its portability, speed, and low memory footprint make it ideal for real-time applications like image detection, video analysis, and IoT edge computing.

[Quick Start Guide: Raspberry Pi with Ultralytics YOLO11](https://docs.ultralytics.com/guides/raspberry-pi/)

[Example NCNN code C++](https://github.com/Tencent/ncnn/tree/master/examples)

# How to run the project
## Prerequisite (Dependencies)
1. Ultralytics (export YOLO.pt to YOLO.ncnn)
2. NCNN sdk
3. Using Yolo11n original

## Running the project
`mkdir build && cd build && cmake .. && make -j$(nproc) && ./yolov11_ncnn -data=../yellowLights.jpg -numTest=100`
