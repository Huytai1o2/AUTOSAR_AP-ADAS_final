import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
from torch.cuda.amp import autocast, GradScaler
import torchvision
from torchvision.models.detection import ssdlite320_mobilenet_v3_large
from torchvision.models.detection.ssd import SSDClassificationHead
import os
import json
from tqdm import tqdm
from pycocotools.coco import COCO
from PIL import Image
import numpy as np
from torch.utils.tensorboard import SummaryWriter

# Configuration
CONFIG = {
    'epochs': 100,
    'image_size': 320,
    'batch_size': 16,
    'learning_rate': 0.001,
    'weight_decay': 0.0005,
    'momentum': 0.9,
    'num_workers': 4,
    'device': 'cuda' if torch.cuda.is_available() else 'cpu',
    'checkpoint_dir': 'checkpoints',
    'log_dir': 'runs',
    'save_interval': 10,  # Save checkpoint every N epochs
    'use_amp': True,  # Mixed precision training (16-bit)
}

class COCODataset(torch.utils.data.Dataset):
    def __init__(self, root_dir, annotation_file, image_size=320, transforms=None):
        self.root_dir = root_dir
        self.image_size = image_size
        self.transforms = transforms
        
        # Load COCO annotations
        self.coco = COCO(annotation_file)
        self.image_ids = list(self.coco.imgs.keys())
        
        # Get category information
        self.categories = self.coco.loadCats(self.coco.getCatIds())
        self.cat_ids = self.coco.getCatIds()
        # Map category IDs to indices starting from 1 (0 is background)
        self.cat_id_to_label = {cat_id: i + 1 for i, cat_id in enumerate(self.cat_ids)}
        self.num_classes = len(self.cat_ids) + 1  # +1 for background
        
        print(f"Loaded {len(self.image_ids)} images with {len(self.cat_ids)} classes")
        print(f"Categories: {[cat['name'] for cat in self.categories]}")
    
    def __len__(self):
        return len(self.image_ids)
    
    def __getitem__(self, idx):
        # Load image
        img_id = self.image_ids[idx]
        img_info = self.coco.loadImgs(img_id)[0]
        img_path = os.path.join(self.root_dir, img_info['file_name'])
        
        image = Image.open(img_path).convert('RGB')
        
        # Load annotations
        ann_ids = self.coco.getAnnIds(imgIds=img_id)
        anns = self.coco.loadAnns(ann_ids)
        
        boxes = []
        labels = []
        
        for ann in anns:
            # COCO bbox format: [x, y, width, height]
            x, y, w, h = ann['bbox']
            # Convert to [x_min, y_min, x_max, y_max]
            boxes.append([x, y, x + w, y + h])
            labels.append(self.cat_id_to_label[ann['category_id']])
        
        # Convert to tensors
        if len(boxes) == 0:
            boxes = torch.zeros((0, 4), dtype=torch.float32)
            labels = torch.zeros((0,), dtype=torch.int64)
        else:
            boxes = torch.as_tensor(boxes, dtype=torch.float32)
            labels = torch.as_tensor(labels, dtype=torch.int64)
        
        target = {
            'boxes': boxes,
            'labels': labels,
            'image_id': torch.tensor([img_id])
        }
        
        # Apply transforms
        if self.transforms:
            image, target = self.transforms(image, target)
        else:
            # Default transform: resize and normalize
            image = torchvision.transforms.functional.resize(
                image, [self.image_size, self.image_size]
            )
            image = torchvision.transforms.functional.to_tensor(image)
            
            # Scale boxes to new image size
            orig_w, orig_h = img_info['width'], img_info['height']
            if len(boxes) > 0:
                target['boxes'][:, [0, 2]] *= self.image_size / orig_w
                target['boxes'][:, [1, 3]] *= self.image_size / orig_h
        
        return image, target


def collate_fn(batch):
    """Custom collate function for DataLoader"""
    return tuple(zip(*batch))


def get_model(num_classes):
    """Create SSD-MobileNetV3 model"""
    # Load pretrained SSDLite320_MobileNet_V3_Large model
    # Set num_classes to match our dataset
    model = ssdlite320_mobilenet_v3_large(num_classes=num_classes, weights_backbone='DEFAULT')
    
    return model


def train_one_epoch(model, optimizer, data_loader, device, epoch, scaler=None, writer=None):
    """Train for one epoch"""
    model.train()
    total_loss = 0
    
    pbar = tqdm(data_loader, desc=f'Epoch {epoch}')
    for i, (images, targets) in enumerate(pbar):
        images = [img.to(device) for img in images]
        targets = [{k: v.to(device) for k, v in t.items()} for t in targets]
        
        optimizer.zero_grad()
        
        # Mixed precision training
        if scaler is not None:
            with autocast():
                loss_dict = model(images, targets)
                losses = sum(loss for loss in loss_dict.values())
            
            scaler.scale(losses).backward()
            scaler.step(optimizer)
            scaler.update()
        else:
            loss_dict = model(images, targets)
            losses = sum(loss for loss in loss_dict.values())
            losses.backward()
            optimizer.step()
        
        total_loss += losses.item()
        
        # Update progress bar
        pbar.set_postfix({
            'loss': f'{losses.item():.4f}',
            'avg_loss': f'{total_loss / (i + 1):.4f}'
        })
        
        # Log to tensorboard
        if writer is not None:
            global_step = epoch * len(data_loader) + i
            writer.add_scalar('Loss/train_step', losses.item(), global_step)
            for k, v in loss_dict.items():
                writer.add_scalar(f'Loss/{k}', v.item(), global_step)
    
    avg_loss = total_loss / len(data_loader)
    
    if writer is not None:
        writer.add_scalar('Loss/train_epoch', avg_loss, epoch)
    
    return avg_loss


def save_checkpoint(model, optimizer, epoch, loss, filename):
    """Save model checkpoint"""
    checkpoint = {
        'epoch': epoch,
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'loss': loss,
    }
    torch.save(checkpoint, filename)
    print(f'Checkpoint saved: {filename}')


def main():
    print("="*60)
    print("SSD-MobileNetV3 Training Configuration")
    print("="*60)
    for key, value in CONFIG.items():
        print(f"{key:20s}: {value}")
    print("="*60)
    
    # Create directories
    os.makedirs(CONFIG['checkpoint_dir'], exist_ok=True)
    os.makedirs(CONFIG['log_dir'], exist_ok=True)
    
    # Find dataset directory
    dataset_dirs = [d for d in os.listdir('.') if 'traffic' in d.lower() or 'coco' in d.lower()]
    if not dataset_dirs:
        print("ERROR: No dataset found! Please run download_dataset.py first")
        return
    
    dataset_dir = dataset_dirs[0]
    print(f"\nUsing dataset: {dataset_dir}")
    
    # Look for annotation files
    train_ann = None
    val_ann = None
    
    for root, dirs, files in os.walk(dataset_dir):
        for file in files:
            if file == '_annotations.coco.json':
                if 'train' in root:
                    train_ann = os.path.join(root, file)
                elif 'val' in root or 'valid' in root:
                    val_ann = os.path.join(root, file)
    
    if not train_ann:
        print("ERROR: Could not find training annotation file (_annotations.coco.json)")
        print(f"Please check the dataset directory: {dataset_dir}")
        return
    
    print(f"Train annotations: {train_ann}")
    if val_ann:
        print(f"Val annotations: {val_ann}")
    
    # Determine image directory
    image_dir = os.path.dirname(train_ann)
    
    # Create datasets
    print("\nLoading datasets...")
    train_dataset = COCODataset(
        root_dir=image_dir,
        annotation_file=train_ann,
        image_size=CONFIG['image_size']
    )
    
    num_classes = train_dataset.num_classes
    print(f"Number of classes (including background): {num_classes}")
    
    # Create data loader
    train_loader = DataLoader(
        train_dataset,
        batch_size=CONFIG['batch_size'],
        shuffle=True,
        num_workers=CONFIG['num_workers'],
        collate_fn=collate_fn,
        pin_memory=True if CONFIG['device'] == 'cuda' else False
    )
    
    # Create model
    print("\nCreating SSD-MobileNetV3 model...")
    device = torch.device(CONFIG['device'])
    model = get_model(num_classes)
    model.to(device)
    
    # Count parameters
    total_params = sum(p.numel() for p in model.parameters())
    trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    print(f"Total parameters: {total_params:,}")
    print(f"Trainable parameters: {trainable_params:,}")
    
    # Optimizer
    optimizer = optim.SGD(
        model.parameters(),
        lr=CONFIG['learning_rate'],
        momentum=CONFIG['momentum'],
        weight_decay=CONFIG['weight_decay']
    )
    
    # Learning rate scheduler
    lr_scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=30, gamma=0.1)
    
    # Mixed precision scaler
    scaler = GradScaler() if CONFIG['use_amp'] and CONFIG['device'] == 'cuda' else None
    if scaler:
        print("Mixed precision training (FP16) enabled")
    
    # Tensorboard writer
    writer = SummaryWriter(CONFIG['log_dir'])
    
    # Training loop
    print("\n" + "="*60)
    print("Starting training...")
    print("="*60)
    
    best_loss = float('inf')
    
    for epoch in range(1, CONFIG['epochs'] + 1):
        print(f"\nEpoch {epoch}/{CONFIG['epochs']}")
        print("-" * 60)
        
        # Train
        avg_loss = train_one_epoch(
            model, optimizer, train_loader, device, epoch, scaler, writer
        )
        
        # Update learning rate
        lr_scheduler.step()
        current_lr = optimizer.param_groups[0]['lr']
        print(f"Average Loss: {avg_loss:.4f}, Learning Rate: {current_lr:.6f}")
        
        writer.add_scalar('Learning_Rate', current_lr, epoch)
        
        # Save checkpoint
        if epoch % CONFIG['save_interval'] == 0:
            checkpoint_path = os.path.join(
                CONFIG['checkpoint_dir'],
                f'ssd_mobilenet_v3_epoch_{epoch}.pth'
            )
            save_checkpoint(model, optimizer, epoch, avg_loss, checkpoint_path)
        
        # Save best model
        if avg_loss < best_loss:
            best_loss = avg_loss
            best_model_path = os.path.join(
                CONFIG['checkpoint_dir'],
                'ssd_mobilenet_v3_best.pth'
            )
            save_checkpoint(model, optimizer, epoch, avg_loss, best_model_path)
            print(f"New best model saved with loss: {best_loss:.4f}")
    
    # Save final model
    final_model_path = os.path.join(
        CONFIG['checkpoint_dir'],
        'ssd_mobilenet_v3_final.pth'
    )
    save_checkpoint(model, optimizer, CONFIG['epochs'], avg_loss, final_model_path)
    
    # Save model for NCNN export (state dict only)
    export_path = 'ssd_mobilenet_v3_export.pth'
    torch.save(model.state_dict(), export_path)
    print(f"\nModel saved for export: {export_path}")
    
    writer.close()
    print("\n" + "="*60)
    print("Training completed!")
    print("="*60)
    print(f"Best loss: {best_loss:.4f}")
    print(f"Final model: {final_model_path}")
    print(f"Export model: {export_path}")
    print("\nNext steps:")
    print("1. Run tensorboard --logdir=runs to view training progress")
    print("2. Create export script to convert model to NCNN format")


if __name__ == '__main__':
    main()
