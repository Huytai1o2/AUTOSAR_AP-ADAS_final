# ONNX to NCNN Conversion Guide

## Summary

Your `ssd_mobilenet_v3.onnx` model has been successfully converted to NCNN format!

## Generated Files

✅ **ssd_mobilenet_v3.param** (27 KB) - Model structure and parameters
✅ **ssd_mobilenet_v3.bin** (15 MB) - Model weights

## Conversion Steps Performed

1. **Installed dependencies**:
   - protobuf-dev and protobuf-compiler
   
2. **Compiled onnx2ncnn tool**:
   ```bash
   cd /home/l1ttled1no/Documents/paper/ncnn/tools/onnx
   protoc --cpp_out=. onnx.proto
   g++ -o onnx2ncnn onnx2ncnn.cpp onnx.pb.cc `pkg-config --cflags --libs protobuf` -I. -std=c++11 -O2
   ```

3. **Simplified ONNX model** (recommended for better compatibility):
   ```bash
   python3 -m onnxsim ssd_mobilenet_v3.onnx ssd_mobilenet_v3_sim.onnx
   ```
   - Reduced model size from 14.9 MiB to 14.3 MiB
   - Optimized computation graph

4. **Converted to NCNN format**:
   ```bash
   /home/l1ttled1no/Documents/paper/ncnn/tools/onnx/onnx2ncnn \
       ssd_mobilenet_v3_sim.onnx \
       ssd_mobilenet_v3.param \
       ssd_mobilenet_v3.bin
   ```

## How to Use These Files in C++

Based on the Medium article, here's a basic template for using your converted model:

```cpp
#include "net.h"
#include <opencv2/opencv.hpp>

int main() {
    // Load the NCNN model
    ncnn::Net net;
    net.load_param("ssd_mobilenet_v3.param");
    net.load_model("ssd_mobilenet_v3.bin");
    
    // Load and preprocess input image
    cv::Mat img = cv::imread("input.jpg");
    ncnn::Mat input = ncnn::Mat::from_pixels(img.data, 
                                             ncnn::Mat::PIXEL_BGR,
                                             img.cols, img.rows);
    
    // Normalize (adjust mean and std based on your model's training)
    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.0f/127.5f, 1.0f/127.5f, 1.0f/127.5f};
    input.substract_mean_normalize(mean_vals, norm_vals);
    
    // Run inference
    ncnn::Extractor extractor = net.create_extractor();
    extractor.input("input", input);  // "input" is your input node name
    
    ncnn::Mat output;
    extractor.extract("output", output);  // "output" is your output node name
    
    // Process output
    // ... your post-processing logic here
    
    return 0;
}
```

## Important Notes

1. **Input/Output Node Names**: Check your ONNX model to find the correct input and output node names. In the param file, the first layer shows `Input input 0 1 input`, so your input node name is `"input"`.

2. **Normalization**: Make sure to use the same mean and std values that were used during training of your PyTorch model.

3. **Image Preprocessing**: The model expects specific input dimensions. For SSD MobileNet v3, it's typically 320x320 or 640x640. Resize your images accordingly.

4. **CMakeLists.txt** for building your inference code:
```cmake
cmake_minimum_required(VERSION 3.10)
project(ssd_inference)

set(CMAKE_CXX_STANDARD 11)

# Set NCNN paths
set(NCNN_DIR "/home/l1ttled1no/Documents/paper/ncnn/build/install")
include_directories(${NCNN_DIR}/include/ncnn)
link_directories(${NCNN_DIR}/lib)

# Find OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(ssd_inference main.cpp)
target_link_libraries(ssd_inference ncnn ${OpenCV_LIBS})
```

## Next Steps

1. **Test the model**: Write a simple C++ program to load and test the model
2. **Verify accuracy**: Compare outputs with the original ONNX model
3. **Optimize**: If needed, you can quantize the model for better mobile performance
4. **Deploy**: Integrate into your Android/iOS application

## Troubleshooting

- If ncnnoptimize crashes (as it did during conversion), you can still use the non-optimized `.param` and `.bin` files
- For better conversion results with PyTorch models, consider using PNNX instead of ONNX2NCNN
- Make sure your input image preprocessing matches your training pipeline

## References

- Medium Article: https://medium.com/@freshtechyy/deployment-of-pytorch-model-using-ncnn-bceff5d846b0
- NCNN GitHub: https://github.com/Tencent/ncnn
- PNNX (alternative converter): https://github.com/pnnx/pnnx
