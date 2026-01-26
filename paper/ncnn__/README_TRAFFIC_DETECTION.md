# Traffic Light Detection with NCNN

This project uses your converted SSD MobileNet v3 NCNN model to detect traffic lights in images.

## Dataset Classes

Based on your Traffic-Light-1 dataset:
- **Class 0**: red-yellow-green (background)
- **Class 1**: Green light
- **Class 2**: Red light  
- **Class 3**: Yellow light

## Files

- `traffic_light_detector.cpp` - Main detection code
- `build_traffic_detector.sh` - Build script
- `model/ssd_mobilenet_v3.param` - NCNN model structure
- `model/ssd_mobilenet_v3.bin` - NCNN model weights
- `Traffic-Light-1/` - Your dataset folder

## How to Build

### 1. Make the build script executable:
```bash
chmod +x build_traffic_detector.sh
```

### 2. Run the build script:
```bash
./build_traffic_detector.sh
```

The script will:
- Check if NCNN is built
- Detect OpenCV installation
- Compile the traffic light detector
- Create the binary in `build_detector/` folder

## How to Use

### Run detection on a test image:
```bash
cd build_detector
./traffic_light_detector ../model/ssd_mobilenet_v3.param ../model/ssd_mobilenet_v3.bin ../Traffic-Light-1/test/[image_name.jpg]
```

### Example:
```bash
./traffic_light_detector \
    ../model/ssd_mobilenet_v3.param \
    ../model/ssd_mobilenet_v3.bin \
    ../Traffic-Light-1/test/74_jpg.rf.80fc033dbcf60dbe4d331e94cc40fbf9.jpg
```

## Understanding the Output

The program will:
1. Load your NCNN model
2. Resize the input image to 320x320 (or your model's expected size)
3. Run inference
4. Filter detections with confidence > 30%
5. Display results with bounding boxes:
   - **Green box** for green lights
   - **Red box** for red lights
   - **Yellow box** for yellow lights
6. Print detection information to console

## Important Notes

### Model Input/Output Names

The code uses these node names:
- **Input**: `"input"`
- **Output**: `"output"`

If your model uses different names, check your `.param` file:
```bash
head -20 model/ssd_mobilenet_v3.param
```

Look for the first `Input` line and last output layer. Update the code if needed:
```cpp
ex.input("your_input_name", in);
ex.extract("your_output_name", out);
```

### Normalization

The code uses standard MobileNet normalization:
```cpp
mean_vals = [127.5, 127.5, 127.5]
norm_vals = [1/127.5, 1/127.5, 1/127.5]
```

If your model was trained with different normalization, update these values.

### Input Size

Current code uses `320x320`. If your model expects different size (e.g., `640x640`), change:
```cpp
const int target_size = 640;  // Change here
```

## Troubleshooting

### Build fails with "OpenCV not found"
```bash
sudo apt install libopencv-dev
```

### Build fails with "NCNN library not found"
Build NCNN first:
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DNCNN_VULKAN=OFF ..
make -j$(nproc)
```

### No detections / Poor results

1. **Check model output format**: Different SSD implementations may output different formats
2. **Adjust confidence threshold**: Lower from 0.3 to 0.1 in the code
3. **Verify normalization**: Make sure it matches your training
4. **Check input size**: Ensure it matches what the model expects

### Wrong input/output names

Check your `.param` file:
```bash
# First line shows magic number and layer count
# Input layer name:
grep "Input" model/ssd_mobilenet_v3.param | head -1

# Output layers are usually at the end:
tail -10 model/ssd_mobilenet_v3.param
```

## Next Steps

1. **Test with multiple images**:
   ```bash
   for img in ../Traffic-Light-1/test/*.jpg; do
       echo "Testing: $img"
       ./traffic_light_detector ../model/ssd_mobilenet_v3.param ../model/ssd_mobilenet_v3.bin "$img"
   done
   ```

2. **Adjust confidence threshold** if needed

3. **Fine-tune for your specific use case**

4. **Add video support** - modify the code to process video frames

5. **Optimize for mobile** - rebuild with Vulkan support for GPU acceleration

## Performance Tips

- Use `-DNCNN_VULKAN=ON` when building NCNN for GPU acceleration
- Reduce input size for faster inference (but lower accuracy)
- Use threading for batch processing

## References

- NCNN: https://github.com/Tencent/ncnn
- Dataset: Traffic-Light-1 (Roboflow)
- Model: SSD MobileNet v3 (converted from ONNX)
