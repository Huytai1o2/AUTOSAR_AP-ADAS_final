# SSD-MobileNetV3 Training for NCNN

Train SSD-MobileNetV3 object detection model on traffic light dataset and export to NCNN format for deployment on mobile/embedded devices.

## Features

- ✅ **SSD-MobileNetV3** architecture optimized for mobile deployment
- ✅ **Mixed Precision Training** (FP16) for faster training
- ✅ **100 Epochs** training with automatic checkpointing
- ✅ **320x320 Input Size** optimized for speed and accuracy
- ✅ **NCNN Export** for deployment on mobile/embedded devices
- ✅ **TensorBoard** logging for training visualization
- ✅ **COCO Format** dataset support via Roboflow

## Quick Start

### 1. Setup Environment

**Option A: Automatic Setup (Recommended)**
```bash
chmod +x setup.sh
./setup.sh
```

**Option B: Manual Setup**
```bash
# Create and activate virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install dependencies
pip install -r requirements.txt
```

### 2. Download Dataset

```bash
python download_dataset.py
```

This downloads the traffic light dataset from Roboflow in COCO format.

### 3. Train Model

```bash
python train.py
```

**Training Configuration:**
- **Epochs:** 100
- **Image Size:** 320x320
- **Batch Size:** 16
- **Mixed Precision:** Enabled (FP16)
- **Learning Rate:** 0.001 with step decay
- **Checkpoints:** Saved every 10 epochs

**Outputs:**
- `checkpoints/ssd_mobilenet_v3_best.pth` - Best model (lowest loss)
- `checkpoints/ssd_mobilenet_v3_final.pth` - Final model after 100 epochs
- `checkpoints/ssd_mobilenet_v3_epoch_N.pth` - Periodic checkpoints
- `ssd_mobilenet_v3_export.pth` - Model ready for export
- `runs/` - TensorBoard logs

### 4. Monitor Training

```bash
tensorboard --logdir=runs
```

Open http://localhost:6006 in your browser to view:
- Training loss curves
- Learning rate schedule
- Individual loss components (classification, bbox regression)

### 5. Export to NCNN

```bash
python export.py --model checkpoints/ssd_mobilenet_v3_best.pth
```

**Export Options:**
```bash
python export.py \
  --model checkpoints/ssd_mobilenet_v3_best.pth \
  --output ssd_mobilenet_v3 \
  --image-size 320
```

**Export Outputs:**
- `ssd_mobilenet_v3.pt` - TorchScript format
- `ssd_mobilenet_v3.onnx` - ONNX format
- `ssd_mobilenet_v3.ncnn.param` - NCNN parameter file
- `ssd_mobilenet_v3.ncnn.bin` - NCNN binary weights

**Note:** NCNN export requires [PNNX](https://github.com/pnnx/pnnx/releases). Download and install:
```bash
# Download from GitHub releases
wget https://github.com/pnnx/pnnx/releases/download/latest/pnnx-linux.zip
unzip pnnx-linux.zip
sudo cp pnnx /usr/local/bin/
```

## Project Structure

```
train/
├── train.py              # Main training script
├── export.py             # Model export to NCNN
├── download_dataset.py   # Dataset download from Roboflow
├── requirements.txt      # Python dependencies
├── setup.sh             # Automated setup script
├── README.md            # This file
├── checkpoints/         # Model checkpoints (created during training)
├── runs/                # TensorBoard logs (created during training)
└── .venv/              # Virtual environment
```

## Training Details

### Model Architecture
- **Backbone:** MobileNetV3-Large (pretrained on ImageNet)
- **Head:** SSDLite detection head
- **Input:** 320x320 RGB images
- **Output:** Bounding boxes + class scores

### Training Strategy
1. **Pretrained Backbone:** Uses ImageNet-pretrained MobileNetV3-Large
2. **Fine-tuned Head:** Custom classification head for your dataset
3. **Data Loading:** COCO format with automatic resizing
4. **Optimization:** SGD with momentum (0.9) and weight decay (0.0005)
5. **LR Schedule:** Step decay (0.1x every 30 epochs)
6. **Mixed Precision:** FP16 training for 2x speedup on modern GPUs

### Hardware Requirements
- **GPU:** NVIDIA GPU with CUDA support (recommended)
- **RAM:** 8GB minimum, 16GB recommended
- **Disk:** 10GB for dataset and checkpoints

### Performance Tips
1. **Batch Size:** Reduce if you run out of GPU memory
2. **Num Workers:** Adjust based on CPU cores (default: 4)
3. **Mixed Precision:** Disable on older GPUs by editing `CONFIG['use_amp'] = False`

## Deployment

### Using NCNN Model

After exporting to NCNN format, integrate into your C++ application:

```cpp
#include "net.h"

ncnn::Net ssd;
ssd.load_param("ssd_mobilenet_v3.ncnn.param");
ssd.load_model("ssd_mobilenet_v3.ncnn.bin");

// Run inference
ncnn::Mat in = ncnn::Mat::from_pixels_resize(
    image_data, ncnn::Mat::PIXEL_BGR, width, height, 320, 320
);

ncnn::Extractor ex = ssd.create_extractor();
ex.input("input", in);

ncnn::Mat boxes, scores;
ex.extract("boxes", boxes);
ex.extract("scores", scores);
```

### Optimization for Mobile

1. **Quantization:** Convert to INT8 for faster inference
2. **Pruning:** Remove redundant weights
3. **Vulkan:** Enable GPU acceleration on mobile

## Troubleshooting

### Out of Memory Error
Reduce batch size in `train.py`:
```python
CONFIG = {
    'batch_size': 8,  # Reduce from 16
    ...
}
```

### No GPU Detected
Training will automatically use CPU, but will be slower. Install CUDA-enabled PyTorch:
```bash
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu118
```

### PNNX Not Found
Download from https://github.com/pnnx/pnnx/releases and add to PATH

### Dataset Not Found
Ensure `download_dataset.py` completed successfully and check for directories containing "traffic" or "coco"

## References

- [PyTorch Object Detection](https://pytorch.org/vision/stable/models.html#object-detection)
- [NCNN Framework](https://github.com/Tencent/ncnn)
- [PNNX Converter](https://github.com/pnnx/pnnx)
- [MobileNetV3 Paper](https://arxiv.org/abs/1905.02244)
- [SSD Paper](https://arxiv.org/abs/1512.02325)

## License

MIT License - feel free to use for your projects!
