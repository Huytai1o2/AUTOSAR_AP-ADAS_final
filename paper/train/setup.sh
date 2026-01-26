#!/bin/bash

# Setup script for SSD-MobileNetV3 training environment

echo "=========================================="
echo "SSD-MobileNetV3 Training Setup"
echo "=========================================="

# Check if virtual environment exists
if [ ! -d ".venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv .venv
fi

echo "Activating virtual environment..."
source .venv/bin/activate

echo "Upgrading pip..."
pip install --upgrade pip

echo "Installing dependencies..."
pip install -r requirements.txt

echo ""
echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Download dataset: python download_dataset.py"
echo "2. Start training: python train.py"
echo "3. Export to NCNN: python export.py --model checkpoints/ssd_mobilenet_v3_best.pth"
echo ""
echo "To activate the environment in the future, run:"
echo "  source .venv/bin/activate"
echo ""
