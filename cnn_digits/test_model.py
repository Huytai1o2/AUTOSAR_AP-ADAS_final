"""
Test script for the trained Multi-head CNN timer model.

Tests all three exported formats:
  1. pretrain_4heads_model.h5       — Keras (training machine)
  2. timer_model.tflite             — float32 TFLite
  3. timer_model_int8.tflite        — int8 quantised TFLite (RPi5 target)

Evaluation is done on realDatasetRoi/ (526 real images, ground-truth = filename).
Also supports testing on a single image passed via CLI.

Usage:
  python test_model.py                        # full evaluation
  python test_model.py --image path/to/img    # single image
  python test_model.py --model int8           # only int8 TFLite
  python test_model.py --no-plot              # skip matplotlib visualisation
"""

import os
import sys
import time
import argparse

import cv2
import numpy as np

# ─── CONFIG (must match multiheadCNN.py) ────────────────────────────────────
IMG_H, IMG_W = 64, 96
BLANK_CLASS  = 10
ROI_DIR      = "realDatasetRoi"
H5_PATH      = "pretrain_4heads_model.h5"
TFLITE_FP32  = "trained_model/v3/timer_model.tflite"
TFLITE_INT8  = "trained_model/v3/timer_model_int8.tflite"


# ─── HELPERS ─────────────────────────────────────────────────────────────────
def preprocess(img_bgr):
    """BGR image → (1, 64, 96, 1) float32 in [0, 1]."""
    gray   = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
    resized = cv2.resize(gray, (IMG_W, IMG_H), interpolation=cv2.INTER_AREA)
    x = resized.astype(np.float32) / 255.0
    return x[np.newaxis, :, :, np.newaxis]          # (1, H, W, 1)


def decode_prediction(pred_L, pred_S1, pred_S2, pred_S3):
    """Log-probability decoding — Goodfellow et al., Appendix A."""
    log_L  = np.log(np.clip(pred_L[0],  1e-10, 1.0))
    log_S1 = np.log(np.clip(pred_S1[0], 1e-10, 1.0))
    log_S2 = np.log(np.clip(pred_S2[0], 1e-10, 1.0))
    log_S3 = np.log(np.clip(pred_S3[0], 1e-10, 1.0))

    best_s1 = int(np.argmax(log_S1[:10]))   # exclude BLANK
    best_s2 = int(np.argmax(log_S2[:10]))   # exclude BLANK
    best_s3 = int(np.argmax(log_S3))        # S3 has no BLANK

    score_1 = log_L[0] + log_S3[best_s3]
    score_2 = log_L[1] + log_S2[best_s2] + log_S3[best_s3]
    score_3 = log_L[2] + log_S1[best_s1] + log_S2[best_s2] + log_S3[best_s3]

    best_L = int(np.argmax([score_1, score_2, score_3])) + 1

    if best_L == 1:
        return str(best_s3)
    elif best_L == 2:
        return f"{best_s2}{best_s3}"
    else:
        return f"{best_s1}{best_s2}{best_s3}"


# ─── LOADERS ─────────────────────────────────────────────────────────────────
def load_roi_dataset(roi_dir):
    """Returns list of (img_path, gt_number_str)."""
    samples = []
    if not os.path.isdir(roi_dir):
        print(f"[WARN] ROI dir not found: {roi_dir}")
        return samples
    for fname in sorted(os.listdir(roi_dir)):
        if not fname.lower().endswith(('.png', '.jpg', '.jpeg')):
            continue
        stem = os.path.splitext(fname)[0]        # e.g. "42_0003"
        parts = stem.split('_')
        if not parts[0].isdigit():
            continue
        gt = parts[0]                            # "42"
        samples.append((os.path.join(roi_dir, fname), gt))
    return samples


# ─── INFERENCE WRAPPERS ──────────────────────────────────────────────────────
class KerasPredictor:
    def __init__(self, path):
        import tensorflow as tf
        self.model = tf.keras.models.load_model(path, compile=False)
        self.name  = "Keras (.h5)"

    def predict_one(self, x):
        pred = self.model.predict(x, verbose=0)
        return decode_prediction(pred['L'], pred['S1'], pred['S2'], pred['S3'])

    def warmup(self, x):
        for _ in range(3):
            self.model.predict(x, verbose=0)


class TFLitePredictor:
    def __init__(self, path):
        import tensorflow as tf
        self.interp = tf.lite.Interpreter(model_path=path)
        self.interp.allocate_tensors()

        inp  = self.interp.get_input_details()[0]
        outs = self.interp.get_output_details()

        self.inp_idx   = inp['index']
        self.inp_scale, self.inp_zero = (
            inp['quantization'] if inp['quantization'][0] != 0 else (1.0, 0)
        )
        self._is_int8  = (inp['dtype'] == np.uint8)

        # TFLite renames outputs — map by output shape instead of name:
        #   L  → shape (1, 3)   S3 → shape (1, 10)   S1/S2 → shape (1, 11)
        # The two 11-class heads appear in definition order: S1 first, S2 second.
        heads_11 = []
        self._out_map = {}   # 'L' / 'S1' / 'S2' / 'S3' → output detail dict
        for o in outs:
            n_cls = o['shape'][-1]
            if n_cls == 3:
                self._out_map['L']  = o
            elif n_cls == 10:
                self._out_map['S3'] = o
            elif n_cls == 11:
                heads_11.append(o)

        # Sort the two 11-class heads by tensor index (preserves definition order)
        heads_11.sort(key=lambda o: o['index'])
        if len(heads_11) >= 2:
            self._out_map['S1'] = heads_11[0]
            self._out_map['S2'] = heads_11[1]

        missing = [k for k in ('L', 'S1', 'S2', 'S3') if k not in self._out_map]
        if missing:
            names = [o['name'] for o in outs]
            raise RuntimeError(f"Cannot map TFLite outputs {names}. Missing: {missing}")

        model_size = os.path.getsize(path) / 1024
        self.name  = f"TFLite {os.path.basename(path)} ({model_size:.0f} KB)"

    def _set_input(self, x_f32):
        if self._is_int8:
            x_q = np.round(x_f32 / self.inp_scale + self.inp_zero).astype(np.uint8)
            self.interp.set_tensor(self.inp_idx, x_q)
        else:
            self.interp.set_tensor(self.inp_idx, x_f32)

    def _get_output(self, head):
        detail = self._out_map[head]
        raw    = self.interp.get_tensor(detail['index'])
        sc, zp = detail['quantization']
        if sc != 0:                              # dequantise int8 → float32
            return (raw.astype(np.float32) - zp) * sc
        return raw.astype(np.float32)

    def predict_one(self, x):
        self._set_input(x)
        self.interp.invoke()
        return decode_prediction(
            self._get_output('L'),
            self._get_output('S1'),
            self._get_output('S2'),
            self._get_output('S3'),
        )

    def warmup(self, x):
        for _ in range(5):
            self._set_input(x)
            self.interp.invoke()


# ─── EVALUATION ──────────────────────────────────────────────────────────────
def evaluate(predictor, samples, show_failures=True):
    """Run predictor on all samples. Returns accuracy and mean latency."""
    correct = 0
    latencies = []
    failures  = []

    for img_path, gt in samples:
        img = cv2.imread(img_path)
        if img is None:
            continue
        x = preprocess(img)

        t0   = time.perf_counter()
        pred = predictor.predict_one(x)
        dt   = (time.perf_counter() - t0) * 1000   # ms

        latencies.append(dt)
        if pred == gt:
            correct += 1
        else:
            failures.append((img_path, gt, pred))

    n        = len(latencies)
    accuracy = correct / n * 100 if n > 0 else 0
    avg_ms   = np.mean(latencies)
    p95_ms   = np.percentile(latencies, 95)

    print(f"\n{'─'*60}")
    print(f"  Model   : {predictor.name}")
    print(f"  Samples : {n}")
    print(f"  Accuracy: {correct}/{n}  ({accuracy:.1f}%)")
    print(f"  Latency : avg={avg_ms:.1f} ms  p95={p95_ms:.1f} ms")
    print(f"{'─'*60}")

    if show_failures and failures:
        print(f"\n  Failures ({len(failures)}):")
        for path, gt, pred in failures[:20]:       # cap at 20
            fname = os.path.basename(path)
            print(f"    {fname:30s}  GT={gt:>4s}  Pred={pred:>4s}")
        if len(failures) > 20:
            print(f"    ... and {len(failures)-20} more")

    return accuracy, avg_ms, failures


# ─── PER-CLASS BREAKDOWN ─────────────────────────────────────────────────────
def per_digit_count_accuracy(predictor, samples):
    """Show accuracy broken down by number of digits (1, 2, 3)."""
    buckets = {1: [0, 0], 2: [0, 0], 3: [0, 0]}
    for img_path, gt in samples:
        img = cv2.imread(img_path)
        if img is None:
            continue
        x    = preprocess(img)
        pred = predictor.predict_one(x)
        n    = len(gt)
        buckets[n][1] += 1
        if pred == gt:
            buckets[n][0] += 1

    print("\n  Per digit-count accuracy:")
    for n_digits, (ok, total) in sorted(buckets.items()):
        if total > 0:
            pct = ok / total * 100
            print(f"    {n_digits}-digit : {ok:3d}/{total:3d}  ({pct:.1f}%)")


# ─── VISUALISATION ───────────────────────────────────────────────────────────
def visualise_failures(failures, n=16):
    """Show a grid of failure cases."""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("[WARN] matplotlib not installed — skipping visualisation")
        return

    n     = min(n, len(failures))
    cols  = 4
    rows  = (n + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(cols * 3, rows * 2.5))
    axes  = np.array(axes).reshape(-1)

    for i, (path, gt, pred) in enumerate(failures[:n]):
        img  = cv2.imread(path)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        axes[i].imshow(gray, cmap='gray', interpolation='nearest')
        axes[i].set_title(f"GT={gt}  Pred={pred}", color='red', fontsize=9)
        axes[i].axis('off')

    for j in range(i + 1, len(axes)):
        axes[j].axis('off')

    plt.suptitle("Failure cases", fontsize=12)
    plt.tight_layout()
    plt.savefig("test_failures.png", dpi=120)
    print(f"\n  Failure grid saved → test_failures.png")
    plt.show()


def visualise_samples(predictor, samples, n=16):
    """Show random sample predictions (correct=green, wrong=red)."""
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        return

    rng     = np.random.default_rng(42)
    indices = rng.choice(len(samples), size=min(n, len(samples)), replace=False)

    cols  = 4
    rows  = (len(indices) + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(cols * 3, rows * 2.5))
    axes  = np.array(axes).reshape(-1)

    for ax_i, sample_i in enumerate(indices):
        path, gt = samples[sample_i]
        img      = cv2.imread(path)
        gray     = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        x        = preprocess(img)
        pred     = predictor.predict_one(x)
        color    = 'green' if pred == gt else 'red'
        axes[ax_i].imshow(gray, cmap='gray', interpolation='nearest')
        axes[ax_i].set_title(f"GT={gt}  Pred={pred}", color=color, fontsize=9)
        axes[ax_i].axis('off')

    for j in range(ax_i + 1, len(axes)):
        axes[j].axis('off')

    plt.suptitle(f"Sample predictions — {predictor.name}", fontsize=11)
    plt.tight_layout()
    out = "test_samples.png"
    plt.savefig(out, dpi=120)
    print(f"  Sample grid saved → {out}")
    plt.show()


# ─── SINGLE IMAGE MODE ───────────────────────────────────────────────────────
def test_single_image(img_path, predictor):
    img = cv2.imread(img_path)
    if img is None:
        print(f"[ERROR] Cannot read: {img_path}")
        return

    x = preprocess(img)
    predictor.warmup(x)

    t0   = time.perf_counter()
    pred = predictor.predict_one(x)
    dt   = (time.perf_counter() - t0) * 1000

    print(f"\n  Image   : {img_path}")
    print(f"  Model   : {predictor.name}")
    print(f"  Pred    : {pred}")
    print(f"  Latency : {dt:.2f} ms")


# ─── MAIN ────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Test timer CNN model")
    parser.add_argument('--image',  default=None,   help='Single image path')
    parser.add_argument('--model',  default='all',
                        choices=['all', 'keras', 'fp32', 'int8'],
                        help='Which model format to test')
    parser.add_argument('--no-plot', action='store_true', help='Skip plots')
    parser.add_argument('--roi-dir', default=ROI_DIR, help='ROI dataset dir')
    args = parser.parse_args()

    # ── Build predictor list ─────────────────────────────────────────────────
    predictors = []

    if args.model in ('all', 'keras') and os.path.exists(H5_PATH):
        print(f"Loading Keras model from {H5_PATH} ...")
        predictors.append(KerasPredictor(H5_PATH))
    elif args.model == 'keras':
        print(f"[WARN] {H5_PATH} not found")

    if args.model in ('all', 'fp32') and os.path.exists(TFLITE_FP32):
        print(f"Loading float32 TFLite from {TFLITE_FP32} ...")
        predictors.append(TFLitePredictor(TFLITE_FP32))
    elif args.model == 'fp32':
        print(f"[WARN] {TFLITE_FP32} not found")

    if args.model in ('all', 'int8') and os.path.exists(TFLITE_INT8):
        print(f"Loading int8 TFLite from {TFLITE_INT8} ...")
        predictors.append(TFLitePredictor(TFLITE_INT8))
    elif args.model == 'int8':
        print(f"[WARN] {TFLITE_INT8} not found")

    if not predictors:
        print("[ERROR] No model files found. Run multiheadCNN.py first.")
        sys.exit(1)

    # ── Single image mode ────────────────────────────────────────────────────
    if args.image:
        for p in predictors:
            test_single_image(args.image, p)
        return

    # ── Full evaluation on ROI dataset ───────────────────────────────────────
    print(f"\nLoading ROI dataset from {args.roi_dir} ...")
    samples = load_roi_dataset(args.roi_dir)
    if not samples:
        print("[ERROR] No images found in ROI directory.")
        sys.exit(1)
    print(f"  Found {len(samples)} images")

    results = []
    for predictor in predictors:
        # Warmup
        dummy = preprocess(np.zeros((IMG_H, IMG_W, 3), dtype=np.uint8))
        predictor.warmup(dummy)

        acc, avg_ms, failures = evaluate(predictor, samples)
        per_digit_count_accuracy(predictor, samples)
        results.append((predictor.name, acc, avg_ms))

        if not args.no_plot and failures:
            visualise_failures(failures)

    if not args.no_plot and predictors:
        # Show sample predictions using first available predictor
        visualise_samples(predictors[-1], samples)

    # ── Summary table ─────────────────────────────────────────────────────────
    if len(results) > 1:
        print(f"\n{'='*60}")
        print("  SUMMARY")
        print(f"  {'Model':<40} {'Acc':>7}  {'Avg ms':>8}")
        print(f"  {'-'*55}")
        for name, acc, avg_ms in results:
            print(f"  {name:<40} {acc:>6.1f}%  {avg_ms:>7.1f}ms")
        print(f"{'='*60}")


if __name__ == '__main__':
    main()
