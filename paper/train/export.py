import torch
import os
import subprocess
from train import get_model, COCODataset

def export_to_ncnn(model_path, output_name='ssd_mobilenet_v3', image_size=320):
    """
    Export trained SSD-MobileNetV3 model to NCNN format
    
    Steps:
    1. Load the trained model
    2. Export to TorchScript
    3. Export to ONNX
    4. Use PNNX to convert ONNX to NCNN format
    """
    
    print("="*60)
    print("Exporting SSD-MobileNetV3 to NCNN")
    print("="*60)
    
    # Find dataset to get num_classes
    dataset_dirs = [d for d in os.listdir('.') if 'traffic' in d.lower() or 'coco' in d.lower()]
    if not dataset_dirs:
        print("ERROR: No dataset found! Cannot determine number of classes")
        return
    
    dataset_dir = dataset_dirs[0]
    
    # Find annotation file
    train_ann = None
    for root, dirs, files in os.walk(dataset_dir):
        for file in files:
            if file == '_annotations.coco.json' and 'train' in root:
                train_ann = os.path.join(root, file)
                break
        if train_ann:
            break
    
    if not train_ann:
        print("ERROR: Could not find training annotation file")
        return
    
    # Load dataset to get num_classes
    image_dir = os.path.dirname(train_ann)
    temp_dataset = COCODataset(
        root_dir=image_dir,
        annotation_file=train_ann,
        image_size=image_size
    )
    num_classes = temp_dataset.num_classes
    
    print(f"Number of classes: {num_classes}")
    print(f"Loading model from: {model_path}")
    
    # Create model
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = get_model(num_classes)
    
    # Load weights
    if os.path.isfile(model_path):
        checkpoint = torch.load(model_path, map_location=device)
        if 'model_state_dict' in checkpoint:
            model.load_state_dict(checkpoint['model_state_dict'])
        else:
            model.load_state_dict(checkpoint)
        print("Model loaded successfully")
    else:
        print(f"ERROR: Model file not found: {model_path}")
        return
    
    model.eval()
    model.to(device)
    
    # Set model to inference mode for export
    model.eval()
    
    # Dummy input
    dummy_input = torch.randn(1, 3, image_size, image_size).to(device)
    
    # Step 1: Export to TorchScript
    print("\nStep 1: Exporting to TorchScript...")
    torchscript_path = f'{output_name}.pt'
    try:
        with torch.no_grad():
            traced_model = torch.jit.trace(model, dummy_input)
            traced_model.save(torchscript_path)
        print(f"✓ TorchScript model saved: {torchscript_path}")
    except Exception as e:
        print(f"✗ TorchScript export failed: {e}")
        print("  Trying alternative method...")
        
        # Alternative: scripting instead of tracing
        try:
            scripted_model = torch.jit.script(model)
            scripted_model.save(torchscript_path)
            print(f"✓ TorchScript model saved (scripted): {torchscript_path}")
        except Exception as e2:
            print(f"✗ Alternative TorchScript export also failed: {e2}")
    
    # Step 2: Export to ONNX
    print("\nStep 2: Exporting to ONNX...")
    onnx_path = f'{output_name}.onnx'
    try:
        torch.onnx.export(
            model,
            dummy_input,
            onnx_path,
            export_params=True,
            opset_version=11,
            do_constant_folding=True,
            input_names=['input'],
            output_names=['boxes', 'scores'],
            dynamic_axes={
                'input': {0: 'batch_size'},
                'boxes': {0: 'batch_size'},
                'scores': {0: 'batch_size'}
            }
        )
        print(f"✓ ONNX model saved: {onnx_path}")
    except Exception as e:
        print(f"✗ ONNX export failed: {e}")
        print("  This is common with detection models. Continuing with TorchScript...")
    
    # Step 3: Convert to NCNN using PNNX
    print("\nStep 3: Converting to NCNN format using PNNX...")
    print("Note: This requires PNNX to be installed")
    print("Download from: https://github.com/pnnx/pnnx/releases")
    
    ncnn_param = f'{output_name}.ncnn.param'
    ncnn_bin = f'{output_name}.ncnn.bin'
    
    if os.path.exists(torchscript_path):
        pnnx_command = f'pnnx {torchscript_path} inputshape=[1,3,{image_size},{image_size}]'
        print(f"\nRun this command to convert to NCNN:")
        print(f"  {pnnx_command}")
        
        # Try to run PNNX if available
        try:
            result = subprocess.run(
                ['pnnx', torchscript_path, f'inputshape=[1,3,{image_size},{image_size}]'],
                capture_output=True,
                text=True,
                timeout=300
            )
            
            if result.returncode == 0:
                print("✓ PNNX conversion successful!")
                if os.path.exists(ncnn_param) and os.path.exists(ncnn_bin):
                    print(f"✓ NCNN model files created:")
                    print(f"  - {ncnn_param}")
                    print(f"  - {ncnn_bin}")
                else:
                    print("  Note: NCNN files may have different names")
            else:
                print(f"✗ PNNX conversion failed: {result.stderr}")
        except FileNotFoundError:
            print("\n⚠ PNNX not found in PATH")
            print("  Please install PNNX and run the command above manually")
        except subprocess.TimeoutExpired:
            print("✗ PNNX conversion timed out")
        except Exception as e:
            print(f"✗ Error running PNNX: {e}")
    
    print("\n" + "="*60)
    print("Export Summary")
    print("="*60)
    print(f"Input model: {model_path}")
    if os.path.exists(torchscript_path):
        print(f"✓ TorchScript: {torchscript_path}")
    if os.path.exists(onnx_path):
        print(f"✓ ONNX: {onnx_path}")
    if os.path.exists(ncnn_param) and os.path.exists(ncnn_bin):
        print(f"✓ NCNN param: {ncnn_param}")
        print(f"✓ NCNN bin: {ncnn_bin}")
    print("="*60)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Export SSD-MobileNetV3 model to NCNN')
    parser.add_argument('--model', type=str, default='ssd_mobilenet_v3_export.pth',
                        help='Path to trained model (.pth file)')
    parser.add_argument('--output', type=str, default='ssd_mobilenet_v3',
                        help='Output name prefix')
    parser.add_argument('--image-size', type=int, default=320,
                        help='Input image size')
    
    args = parser.parse_args()
    
    export_to_ncnn(args.model, args.output, args.image_size)


if __name__ == '__main__':
    main()
