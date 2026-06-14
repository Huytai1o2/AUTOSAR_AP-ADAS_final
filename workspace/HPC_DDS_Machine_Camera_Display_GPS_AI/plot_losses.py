import pandas as pd
import matplotlib.pyplot as plt

# File paths
csv1 = '/home/huytai102/AUTOSAR_AP-ADAS/trainYOLO26/runs/detect/runs/train/yolo26n_mobilenetv4/results.csv'
csv2 = '/home/huytai102/AUTOSAR_AP-ADAS/trainYOLO26/runs/detect/runs/train/yolo26n_original_custom/results.csv'

# Load data
df1 = pd.read_csv(csv1)
df2 = pd.read_csv(csv2)

# Clean column names in case there are trailing spaces
df1.columns = df1.columns.str.strip()
df2.columns = df2.columns.str.strip()

# Create subplots
fig, axes = plt.subplots(1, 3, figsize=(18, 6))

losses = [('train/box_loss', 'val/box_loss'), 
          ('train/cls_loss', 'val/cls_loss'), 
          ('train/dfl_loss', 'val/dfl_loss')]
titles = ['Box Loss', 'Class Loss', 'DFL Loss']

for i, ((train_loss, val_loss), title) in enumerate(zip(losses, titles)):
    # Model 1: MobileNetV4
    axes[i].plot(df1['epoch'], df1[train_loss], label='MobileNetV4 Train', color='blue', linestyle='--', alpha=0.7)
    axes[i].plot(df1['epoch'], df1[val_loss], label='MobileNetV4 Val', color='blue', linestyle='-', linewidth=2)
    
    # Model 2: Original Custom
    axes[i].plot(df2['epoch'], df2[train_loss], label='Original Train', color='red', linestyle='--', alpha=0.7)
    axes[i].plot(df2['epoch'], df2[val_loss], label='Original Val', color='red', linestyle='-', linewidth=2)
    
    axes[i].set_title(title, fontsize=14)
    axes[i].set_xlabel('Epoch', fontsize=12)
    axes[i].set_ylabel('Loss', fontsize=12)
    axes[i].legend(fontsize=10)
    axes[i].grid(True, linestyle=':', alpha=0.6)

plt.suptitle('Training and Validation Loss Comparison: YOLO26n-MobileNetV4 vs YOLO26n-Original', fontsize=16)
plt.tight_layout(rect=[0, 0.03, 1, 0.95])

output_path = '/home/huytai102/AUTOSAR_AP-ADAS/workspace/HPC_DDS_Machine_Camera_Display_GPS_AI/loss_comparison.png'
plt.savefig(output_path, dpi=300)
print(f'Plot successfully saved to: {output_path}')

# Evaluation Print
print("\n--- Evaluation Metrics ---")
for model_name, df in zip(['YOLO26n-MobileNetV4', 'YOLO26n-Original'], [df1, df2]):
    print(f"\nModel: {model_name}")
    for _, val_loss in losses:
        min_loss = df[val_loss].min()
        min_epoch = df['epoch'][df[val_loss].idxmin()]
        final_loss = df[val_loss].iloc[-1]
        print(f"  {val_loss}: Min = {min_loss:.4f} (at Epoch {min_epoch}), Final (Epoch 100) = {final_loss:.4f}")
