"""
Multi-head CNN — Mixed Dataset (3 sources)
Based on: "Multi-digit Number Recognition from Street View Imagery
           using Deep Convolutional Neural Networks"
           Goodfellow et al., arXiv:1312.6082v4

Data sources:
  1. MNIST_ORG/          — raw MNIST IDX binary files (70 K images)
  2. input_digits/       — real single-digit crops per light colour
                           red/0.png…9.png, green/0.png…9.png,
                           yellow/1.png…3.png  (23 total)
  3. realDatasetRoi/     — real composite ROI images labelled by name
                           format: <timer>_<id_image>.{png,jpg}
                           e.g. 37_0004.png  → number displayed = 37

Architecture (paper §4 + §5):
  5 conv blocks → 2 FC layers → feature H → 4 softmax heads
  (L: sequence length, S1/S2/S3: digit at hundreds/tens/units slot)

Preprocessing (paper §4):
  Subtract per-image mean (applied as first layer inside the model).

Decoding (paper Appendix A):
  argmax_s  log P(S|X)
    = argmax_{l, s1…sl}  [log P(L=l) + Σ log P(Si=si)]

Run:
  python multiheadCNN.py
"""

import os
import struct
import cv2
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models
from tensorflow.keras.callbacks import EarlyStopping, ReduceLROnPlateau

# =====================================================================
# GPU GUARD
# =====================================================================
gpus = tf.config.list_physical_devices('GPU')
if gpus:
    try:
        for gpu in gpus:
            tf.config.experimental.set_memory_growth(gpu, True)
        print(f"GPU: {gpus[0].name}")
    except RuntimeError as e:
        print("GPU error:", e)
else:
    print("No GPU — using CPU.")

# =====================================================================
# CONFIG
# =====================================================================
IMG_H, IMG_W = 64, 96
BLANK_CLASS  = 10   # label used when a slot has no digit

# Layout constants for full-width rendering (matches real ROI layout)
MARGIN_X = 4   # horizontal margin in pixels
MARGIN_Y = 8   # vertical margin in pixels

MNIST_DIR   = "MNIST_ORG"       # directory with IDX binary files
REAL_DIR    = "input_digits"    # single-digit crops per light colour
ROI_DIR     = "realDatasetRoiv2"  # full composite ROI images (timer box only)
LIGHT_TYPES = ["red", "green", "yellow"]


# =====================================================================
# LABEL HELPER
# =====================================================================
def number_to_labels(number):
    """Convert integer 0-999 → (L, S1, S2, S3).

    L   : digit-count minus 1  (0 = 1-digit, 1 = 2-digit, 2 = 3-digit)
    S1  : hundreds digit, or BLANK_CLASS if fewer than 3 digits
    S2  : tens digit,      or BLANK_CLASS if 1-digit number
    S3  : units digit      (always present)
    """
    s      = str(number)
    length = len(s)
    L  = length - 1
    S3 = int(s[-1])
    S2 = int(s[-2]) if length >= 2 else BLANK_CLASS
    S1 = int(s[-3]) if length >= 3 else BLANK_CLASS
    return L, S1, S2, S3


# =====================================================================
# 1. LOAD MNIST FROM IDX BINARY FILES
# =====================================================================
def load_mnist_binary(mnist_dir):
    """Read MNIST train + test sets from raw IDX files."""
    print(f"\nLoading MNIST from {mnist_dir} (binary IDX)...")

    def _read_images(path):
        with open(path, 'rb') as f:
            magic, n, rows, cols = struct.unpack('>IIII', f.read(16))
            assert magic == 0x803, f"Bad magic number in {path}"
            data = np.frombuffer(f.read(), dtype=np.uint8)
        return data.reshape(n, rows, cols)   # (N, 28, 28)

    def _read_labels(path):
        with open(path, 'rb') as f:
            magic, _ = struct.unpack('>II', f.read(8))
            assert magic == 0x801, f"Bad magic number in {path}"
            data = np.frombuffer(f.read(), dtype=np.uint8)
        return data                           # (N,)

    train_imgs = _read_images(os.path.join(mnist_dir, 'train-images-idx3-ubyte'))
    train_lbls = _read_labels(os.path.join(mnist_dir, 'train-labels-idx1-ubyte'))
    test_imgs  = _read_images(os.path.join(mnist_dir, 't10k-images-idx3-ubyte'))
    test_lbls  = _read_labels(os.path.join(mnist_dir, 't10k-labels-idx1-ubyte'))

    images = np.concatenate([train_imgs, test_imgs], axis=0)
    labels = np.concatenate([train_lbls, test_lbls], axis=0)
    print(f"  MNIST loaded: {len(images):,} images (28×28 uint8)")
    return images, labels


# =====================================================================
# 2. LOAD SINGLE-DIGIT CROPS  (input_digits/)
# =====================================================================
def load_real_digits(real_dir):
    """Load per-digit grayscale crops from input_digits/red|green|yellow/<d>.png."""
    real_pool = {d: [] for d in range(10)}
    total     = 0
    for light_type in LIGHT_TYPES:
        type_dir = os.path.join(real_dir, light_type)
        if not os.path.exists(type_dir):
            print(f"  WARNING: {type_dir} not found, skipping.")
            continue
        found = []
        for digit in range(10):
            fpath = os.path.join(type_dir, f"{digit}.png")
            if not os.path.exists(fpath):
                continue
            img = cv2.imread(fpath, cv2.IMREAD_GRAYSCALE)
            if img is None:
                print(f"  WARNING: cannot read {fpath}")
                continue
            real_pool[digit].append(img)
            total += 1
            found.append(digit)
        print(f"  {light_type}: digits {found}")
    print(f"  input_digits loaded: {total} images total")
    return real_pool


# =====================================================================
# 3. LOAD FULL ROI IMAGES  (realDatasetRoi/)
# =====================================================================
def load_roi_dataset(roi_dir):
    """Return list of (fpath, L, S1, S2, S3) — file paths only, NO image data.

    Scans root folder AND 'labelSecondByHuman' subfolder.
    Images are loaded from disk on-demand during training (streaming pipeline)
    to avoid materialising the full dataset in RAM.
    """
    if not os.path.exists(roi_dir):
        print(f"  WARNING: {roi_dir} not found — skipping ROI dataset.")
        return []

    scan_dirs = [roi_dir, os.path.join(roi_dir, "labelSecondByHuman")]
    samples   = []
    skipped   = 0
    seen      = set()

    for scan_dir in scan_dirs:
        if not os.path.exists(scan_dir):
            continue
        for fname in sorted(os.listdir(scan_dir)):
            stem, ext = os.path.splitext(fname)
            if ext.lower() not in ('.png', '.jpg', '.jpeg'):
                continue
            fpath = os.path.join(scan_dir, fname)
            if fpath in seen:
                continue
            seen.add(fpath)

            try:
                number = int(stem.split('_')[0])
            except (ValueError, IndexError):
                skipped += 1
                continue
            if not (0 <= number <= 999):
                skipped += 1
                continue

            L, S1, S2, S3 = number_to_labels(number)
            samples.append((fpath, L, S1, S2, S3))   # path, not array

    print(f"  realDatasetRoiv2 loaded: {len(samples)} paths "
          f"(skipped {skipped})")
    return samples


# =====================================================================
# 4. AUGMENTATION HELPERS
# =====================================================================
def augment_single_digit(img):
    """Strong augmentation for single-digit crops from input_digits/.

    Applied each time a real digit is selected for slot placement so
    the 23 source images generate many visual variants per epoch.
    """
    img = img.astype(np.float32)

    # Brightness & gamma
    img = np.clip(img * np.random.uniform(0.5, 1.8), 0, 255)
    gamma = np.random.uniform(0.5, 2.0)
    img = np.clip(255.0 * (img / 255.0) ** gamma, 0, 255)

    # Blur (simulate out-of-focus)
    if np.random.random() < 0.5:
        ksize = np.random.choice([3, 5])
        img = cv2.GaussianBlur(img.astype(np.uint8),
                               (ksize, ksize), 0).astype(np.float32)

    # Salt-and-pepper noise
    if np.random.random() < 0.4:
        h, w = img.shape
        for _ in range(np.random.randint(10, 80)):
            r = np.random.randint(0, h)
            c = np.random.randint(0, w)
            img[r, c] = 255 if np.random.random() < 0.5 else 0

    # Small shift (±4 px)
    dx, dy = np.random.randint(-4, 5), np.random.randint(-4, 5)
    if dx != 0 or dy != 0:
        M = np.float32([[1, 0, dx], [0, 1, dy]])
        h, w = img.shape
        img = cv2.warpAffine(img.astype(np.uint8), M,
                             (w, h)).astype(np.float32)

    # Small rotation (±10°)
    if np.random.random() < 0.5:
        angle = np.random.uniform(-10, 10)
        h, w = img.shape
        M = cv2.getRotationMatrix2D((w / 2, h / 2), angle, 1.0)
        img = cv2.warpAffine(img.astype(np.uint8), M,
                             (w, h)).astype(np.float32)

    # Gaussian noise
    noise = np.random.normal(0, np.random.uniform(3, 15), img.shape)
    img = np.clip(img + noise, 0, 255)
    return img.astype(np.uint8)


def preaugment_real_pool(real_pool, n_variants=500):
    """Pre-generate N augmented variants per digit from input_digits/.

    Called once before data generation so that the inner loop can
    sample cheaply (array indexing) instead of calling augment_single_digit
    on every sample — speeds up [Real] 0-999 from ~4 num/s to ~150 num/s.
    """
    aug = {d: [] for d in range(10)}
    digits_found = [d for d in range(10) if real_pool[d]]
    for d in digits_found:
        for _ in range(n_variants):
            src = real_pool[d][np.random.randint(len(real_pool[d]))]
            aug[d].append(augment_single_digit(src))
    total = sum(len(v) for v in aug.values())
    print(f"  Pre-augmented input_digits: {total} variants "
          f"for digits {digits_found}")
    return aug


def augment_canvas(canvas):
    """Light noise + brightness jitter applied to the full composite canvas."""
    noise = np.random.normal(0, 0.03, canvas.shape).astype(np.float32)
    canvas = np.clip(canvas + noise, 0.0, 1.0)
    canvas = np.clip(canvas * np.random.uniform(0.88, 1.12), 0.0, 1.0)
    return canvas


def augment_roi_image(canvas):
    """Augment a full ROI image (H×W float32 [0,1]).

    Applies spatial jitter (translation ±4 px), brightness, contrast
    and Gaussian noise — mimicking the random-crop preprocessing in
    the SVHN paper (§5.1).
    """
    h, w = canvas.shape
    # Translation jitter
    dx = np.random.randint(-4, 5)
    dy = np.random.randint(-4, 5)
    if dx != 0 or dy != 0:
        M = np.float32([[1, 0, dx], [0, 1, dy]])
        canvas = cv2.warpAffine(canvas, M, (w, h))
    # Brightness / contrast
    canvas = np.clip(canvas * np.random.uniform(0.80, 1.20), 0.0, 1.0)
    canvas = np.clip(canvas + np.random.uniform(-0.05, 0.05), 0.0, 1.0)
    # Gaussian noise
    noise = np.random.normal(0, np.random.uniform(0.0, 0.04), canvas.shape)
    canvas = np.clip(canvas + noise, 0.0, 1.0).astype(np.float32)
    return canvas


# =====================================================================
# 5. DIGIT PLACEMENT — full-width layout (matches real ROI)
# =====================================================================
def make_one_sample(number, mnist_images, digit_idx, real_pool,
                    source, do_augment):
    """Create one composite 64×96 canvas for `number`.

    Layout (both MNIST and LED input_digits sources):
      - Zoom   : group width = 80–100% of available canvas width (100% default)
      - Align  : random left / center / right within the canvas
      - Vertical: random margin jitter
      - Bounds : all digit pixels are strictly within [0,IMG_W) × [0,IMG_H)

    `source` is 'mnist' or 'real' (LED 7-segment crops from input_digits/).
    Returns (canvas [H,W,1] float32, L, S1, S2, S3).
    """
    L, S1, S2, S3 = number_to_labels(number)
    digits = [int(c) for c in str(number)]
    n = len(digits)

    # ── Vertical layout ──────────────────────────────────────────────────
    my = np.random.randint(4, 14) if do_augment else MARGIN_Y
    dh = max(4, IMG_H - 2 * my)        # at least 4 px tall
    my = (IMG_H - dh) // 2             # recompute to stay symmetric

    # ── Horizontal zoom: group_w = 80–100% of available_w ───────────────
    # available_w leaves MARGIN_X px on each side as absolute boundary guard
    available_w = IMG_W - 2 * MARGIN_X                  # 88 px  (96 - 2×4)
    zoom   = np.random.uniform(0.80, 1.00) if do_augment else 1.0
    dw     = max(1, int(available_w * zoom) // n)       # width per digit slot
    group_w = dw * n                                    # actual group width

    # ── Horizontal alignment ─────────────────────────────────────────────
    leftover = max(0, available_w - group_w)
    if do_augment:
        align = np.random.choice(['left', 'center', 'right'])
    else:
        align = 'center'

    if align == 'left':
        x_start = MARGIN_X
    elif align == 'right':
        x_start = MARGIN_X + leftover
    else:                                               # center
        x_start = MARGIN_X + leftover // 2

    # ── Draw digits — with explicit bounds clamp ──────────────────────────
    canvas = np.zeros((IMG_H, IMG_W), dtype=np.float32)

    for i, d in enumerate(digits):
        x0 = x_start + i * dw
        x1 = x0 + dw
        y0, y1 = my, my + dh

        # Hard clamp: all coordinates strictly inside canvas
        x0 = int(np.clip(x0, 0, IMG_W - 1))
        x1 = int(np.clip(x1, x0 + 1, IMG_W))
        y0 = int(np.clip(y0, 0, IMG_H - 1))
        y1 = int(np.clip(y1, y0 + 1, IMG_H))

        if source == 'real' and real_pool[d]:
            img = real_pool[d][np.random.randint(len(real_pool[d]))]
        else:
            img = mnist_images[np.random.choice(digit_idx[d])]

        resized = cv2.resize(img, (x1 - x0, y1 - y0),
                             interpolation=cv2.INTER_AREA)
        canvas[y0:y1, x0:x1] = resized.astype(np.float32) / 255.0

    if do_augment:
        canvas = augment_canvas(canvas)

    return np.expand_dims(canvas, -1).astype(np.float32), L, S1, S2, S3


# =====================================================================
# 7. STREAMING DATA PIPELINE  (no large arrays in RAM)
# =====================================================================
def make_data_generators(reps_per_number=50,
                         mnist_images=None, mnist_labels=None,
                         real_pool=None, roi_paths=None,
                         do_augment=True, val_fraction=0.15,
                         reps_roi=150):
    """Build streaming train/val generator functions — no large arrays in RAM.

    ROI images are read from disk one-by-one inside the generator.
    Peak RAM usage: MNIST (~55 MB) + pre-aug pool (~100 MB) + one batch.

    Returns (synth_tr_gen, roi_tr_gen, synth_val_gen, roi_val_gen,
             n_tr_synth, n_tr_roi, n_val_synth, n_val_roi).
    """
    digit_idx     = {d: np.where(mnist_labels == d)[0] for d in range(10)}
    has_real      = any(len(v) > 0 for v in real_pool.values())
    sources       = ['mnist', 'real'] if has_real else ['mnist']
    aug_real_pool = preaugment_real_pool(real_pool) if has_real else real_pool

    # Number list matched to realDatasetRoiv2: 1-digit 28.4%, 2-digit 71.6%
    one_digit = list(range(0,  10))
    two_digit = list(range(10, 100))
    numbers   = one_digit * 30 + two_digit * 8   # 1020 total

    # Split ROI paths train / val
    n_roi     = len(roi_paths) if roi_paths else 0
    perm      = np.random.permutation(n_roi)
    n_val_roi = int(n_roi * val_fraction)
    roi_val   = [roi_paths[i] for i in perm[:n_val_roi]]
    roi_tr    = [roi_paths[i] for i in perm[n_val_roi:]]

    # Split number list train / val
    perm2     = np.random.permutation(len(numbers))
    n_val_num = int(len(numbers) * val_fraction)
    nums_val  = [numbers[i] for i in perm2[:n_val_num]]
    nums_tr   = [numbers[i] for i in perm2[n_val_num:]]

    n_tr  = len(nums_tr)  * reps_per_number * len(sources) + len(roi_tr)  * reps_roi
    n_val = len(nums_val) * reps_per_number * len(sources) + len(roi_val) * reps_roi

    print(f"\nStreaming dataset (no pre-allocation):")
    print(f"  Synthetic : {len(nums_tr)*reps_per_number*len(sources):>8,} train "
          f"| {len(nums_val)*reps_per_number*len(sources):>7,} val")
    print(f"  ROI       : {len(roi_tr)*reps_roi:>8,} train "
          f"| {len(roi_val)*reps_roi:>7,} val")
    print(f"  Total     : {n_tr:>8,} train | {n_val:>7,} val")

    def _synth_gen(num_list, shuffle, do_aug):
        def _gen():
            while True:          # infinite — stopped by steps_per_epoch
                nums = list(num_list)
                if shuffle:
                    np.random.shuffle(nums)
                for src in sources:
                    pool = aug_real_pool if src == 'real' else {}
                    for number in nums:
                        for _ in range(reps_per_number):
                            x, l, s1, s2, s3 = make_one_sample(
                                number, mnist_images, digit_idx, pool, src, do_aug)
                            yield (x, {'L':  np.int32(l),  'S1': np.int32(s1),
                                       'S2': np.int32(s2), 'S3': np.int32(s3)})
        return _gen

    def _roi_gen(roi_list, shuffle, do_aug):
        def _gen():
            while True:          # each pass = 1 augmentation per image
                rois = list(roi_list)
                if shuffle:
                    np.random.shuffle(rois)
                for fpath, l, s1, s2, s3 in rois:
                    img = cv2.imread(fpath, cv2.IMREAD_GRAYSCALE)
                    if img is None:
                        continue
                    img_r  = cv2.resize(img, (IMG_W, IMG_H),
                                        interpolation=cv2.INTER_AREA)
                    canvas = img_r.astype(np.float32) / 255.0
                    c = augment_roi_image(canvas) if do_aug else canvas
                    yield (np.expand_dims(c, -1).astype(np.float32),
                           {'L':  np.int32(l),  'S1': np.int32(s1),
                            'S2': np.int32(s2), 'S3': np.int32(s3)})
        return _gen

    n_tr_synth  = len(nums_tr)  * reps_per_number * len(sources)
    n_val_synth = len(nums_val) * reps_per_number * len(sources)
    n_tr_roi    = len(roi_tr)   * reps_roi
    n_val_roi   = len(roi_val)  * reps_roi

    return (
        _synth_gen(nums_tr,  shuffle=True,  do_aug=do_augment),
        _roi_gen  (roi_tr,   shuffle=True,  do_aug=do_augment),
        _synth_gen(nums_val, shuffle=False, do_aug=False),
        _roi_gen  (roi_val,  shuffle=False, do_aug=False),
        n_tr_synth, n_tr_roi,
        n_val_synth, n_val_roi,
    )


# =====================================================================
# 8. MODEL  — lightweight for Raspberry Pi 5 real-time inference
# =====================================================================
def build_model():
    """Lightweight 4-head CNN, ~109 K parameters.

    Designed for 20 fps+ inference on Raspberry Pi 5 (LiteRT/TFLite):

    Key design choices vs. a naive deep CNN:
    - SeparableConv2D (depthwise + pointwise) from block 2 onward:
        same receptive field as Conv2D but ~8× fewer multiply-adds.
    - GlobalAveragePooling2D instead of Flatten:
        avoids a huge dense layer from spatial dims (e.g. Flatten on
        4×6×192 = 4608 inputs → Dense(1024) = 4.7 M params alone).
    - Single FC(256) between GAP and heads: sufficient capacity after
        four progressive feature-extraction stages.
    - use_bias=False on conv layers (bias absorbed by BatchNorm).

    Architecture (spatial dims for 64×96 input):
      Conv2D(32)     + BN + ReLU + Pool  →  32×48×32   ~  320 params
      SepConv2D(64)  + BN + ReLU + Pool  →  16×24×64   ~2 400 params
      SepConv2D(96)  + BN + ReLU + Pool  →   8×12×96   ~6 720 params
      SepConv2D(128) + BN + ReLU + Pool  →   4× 6×128  ~13 152 params
      SepConv2D(192) + BN + ReLU         →   4× 6×192  ~25 728 params
      GlobalAveragePooling2D             →   192
      Dense(256) + Dropout(0.35)         →   256        ~49 408 params
      4 heads (L/S1/S2/S3)               →              ~ 8 995 params
                                               ─────────────────────
                                               Total  ≈  109 K params

    Preprocessing (Goodfellow et al. §4):
      Per-image mean subtraction as the first network operation so the
      model is self-contained and no external normalisation is needed
      at inference time.

    Input  : (batch, 64, 96, 1)  uint8/float32 grayscale, values in [0,1]
    Output : {L: (B,3), S1: (B,11), S2: (B,11), S3: (B,10)}
    """
    inputs = layers.Input(shape=(IMG_H, IMG_W, 1), name='input')

    # Per-image mean subtraction (paper §4)
    x = layers.Lambda(
        lambda t: t - tf.reduce_mean(t, axis=[1, 2, 3], keepdims=True),
        name='mean_subtract'
    )(inputs)

    # Block 1 — standard Conv2D (only 1 input channel; depthwise not helpful here)
    x = layers.Conv2D(32, 3, padding='same', use_bias=False, name='conv1')(x)
    x = layers.BatchNormalization(name='bn1')(x)
    x = layers.Activation('relu', name='relu1')(x)
    x = layers.MaxPooling2D(2, name='pool1')(x)          # → 32×48×32

    # Blocks 2-5 — SeparableConv2D  (depthwise + pointwise, ~8× fewer MACs)
    x = layers.SeparableConv2D(64,  3, padding='same', use_bias=False, name='sep2')(x)
    x = layers.BatchNormalization(name='bn2')(x)
    x = layers.Activation('relu', name='relu2')(x)
    x = layers.MaxPooling2D(2, name='pool2')(x)          # → 16×24×64

    x = layers.SeparableConv2D(96,  3, padding='same', use_bias=False, name='sep3')(x)
    x = layers.BatchNormalization(name='bn3')(x)
    x = layers.Activation('relu', name='relu3')(x)
    x = layers.MaxPooling2D(2, name='pool3')(x)          # →  8×12×96

    x = layers.SeparableConv2D(128, 3, padding='same', use_bias=False, name='sep4')(x)
    x = layers.BatchNormalization(name='bn4')(x)
    x = layers.Activation('relu', name='relu4')(x)
    x = layers.MaxPooling2D(2, name='pool4')(x)          # →  4× 6×128

    x = layers.SeparableConv2D(192, 3, padding='same', use_bias=False, name='sep5')(x)
    x = layers.BatchNormalization(name='bn5')(x)
    x = layers.Activation('relu', name='relu5')(x)
    # No pool — preserve final spatial context for GAP

    # GlobalAveragePooling: collapses 4×6×192 spatial map → 192-d vector
    # (vs Flatten which would give 4608-d, requiring a 4.7 M-param Dense)
    x = layers.GlobalAveragePooling2D(name='gap')(x)     # → 192

    x = layers.Dense(256, activation='relu', name='fc1')(x)
    x = layers.Dropout(0.35, name='drop1')(x)

    # Independent softmax heads (Goodfellow et al. §4, Fig. 8)
    # L  : 3 classes  (0→1-digit, 1→2-digit, 2→3-digit)
    # S1 : 11 classes (0-9 + BLANK=10)  hundreds place
    # S2 : 11 classes (0-9 + BLANK=10)  tens place
    # S3 : 10 classes (0-9)             units place — always occupied
    head_L  = layers.Dense(3,  activation='softmax', name='L')(x)
    head_S1 = layers.Dense(11, activation='softmax', name='S1')(x)
    head_S2 = layers.Dense(11, activation='softmax', name='S2')(x)
    head_S3 = layers.Dense(10, activation='softmax', name='S3')(x)

    return models.Model(
        inputs=inputs,
        outputs={'L': head_L, 'S1': head_S1, 'S2': head_S2, 'S3': head_S3}
    )


# =====================================================================
# 8b. TFLITE EXPORT  — for Raspberry Pi 5 (LiteRT)
# =====================================================================
def export_tflite(keras_model, out_path="timer_model.tflite",
                  int8_quantize=False, rep_data=None):
    """Convert the trained Keras model to a TFLite flatbuffer.

    Args:
        keras_model   : trained tf.keras.Model
        out_path      : output .tflite file path
        int8_quantize : if True, apply post-training full-integer
                        quantisation (requires rep_data)
        rep_data      : numpy array of representative inputs, shape
                        (N, 64, 96, 1), used only when int8_quantize=True

    The resulting .tflite runs on Raspberry Pi 5 via the LiteRT runtime:
        interpreter = tflite.Interpreter(model_path="timer_model.tflite")
        interpreter.allocate_tensors()
        # set input, invoke, read outputs …
    """
    converter = tf.lite.TFLiteConverter.from_keras_model(keras_model)

    if int8_quantize:
        assert rep_data is not None, "rep_data required for int8 quantisation"
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type  = tf.uint8
        converter.inference_output_type = tf.uint8

        def rep_gen():
            for i in range(min(200, len(rep_data))):
                yield [rep_data[i:i+1]]
        converter.representative_dataset = rep_gen

    tflite_bytes = converter.convert()
    with open(out_path, 'wb') as f:
        f.write(tflite_bytes)

    kb = len(tflite_bytes) / 1024
    print(f"TFLite model saved → {out_path}  ({kb:.0f} KB)")


# =====================================================================
# 9. LOG-PROBABILITY DECODING  (Goodfellow et al., Appendix A)
# =====================================================================
def decode_prediction(pred):
    """Decode model output via log-probability maximisation.

    For each candidate length l ∈ {1, 2, 3}:
      log P(S, L=l | X) = log P(L=l) + Σ_{i=1}^{l} max_{si} log P(Si=si)

    We take argmax over digit classes 0-9 (excluding BLANK) for the
    active slots, then pick the length l that maximises the total score.
    Returns the predicted number as a string.
    """
    log_p_L  = np.log(np.clip(pred['L'][0],  1e-10, 1.0))
    log_p_S1 = np.log(np.clip(pred['S1'][0], 1e-10, 1.0))
    log_p_S2 = np.log(np.clip(pred['S2'][0], 1e-10, 1.0))
    log_p_S3 = np.log(np.clip(pred['S3'][0], 1e-10, 1.0))

    # Best digit (0-9) per slot — exclude BLANK_CLASS from argmax
    best_s1 = int(np.argmax(log_p_S1[:10]))
    best_s2 = int(np.argmax(log_p_S2[:10]))
    best_s3 = int(np.argmax(log_p_S3))       # S3 has no BLANK class

    # Cumulative log-probability for each length hypothesis
    score_1 = log_p_L[0] + log_p_S3[best_s3]
    score_2 = log_p_L[1] + log_p_S2[best_s2] + log_p_S3[best_s3]
    score_3 = (log_p_L[2] + log_p_S1[best_s1]
               + log_p_S2[best_s2] + log_p_S3[best_s3])

    best_L = int(np.argmax([score_1, score_2, score_3])) + 1

    if best_L == 1:
        return str(best_s3)
    elif best_L == 2:
        return f"{best_s2}{best_s3}"
    else:
        return f"{best_s1}{best_s2}{best_s3}"


# =====================================================================
# 10. MAIN
# =====================================================================
import math

mnist_images, mnist_labels = load_mnist_binary(MNIST_DIR)
real_pool                  = load_real_digits(REAL_DIR)
roi_paths                  = load_roi_dataset(ROI_DIR)   # paths only, no arrays

if not any(len(v) > 0 for v in real_pool.values()):
    print(f"\nWARNING: No single-digit crops found in {REAL_DIR}/")
    print("  Continuing with MNIST-only synthetic composites.\n")

(synth_tr_gen, roi_tr_gen,
 synth_val_gen, roi_val_gen,
 n_tr_synth, n_tr_roi,
 n_val_synth, n_val_roi) = make_data_generators(
    reps_per_number=15,
    mnist_images=mnist_images,
    mnist_labels=mnist_labels,
    real_pool=real_pool,
    roi_paths=roi_paths,
    do_augment=True,
    reps_roi=40,
)
n_tr  = n_tr_synth  + n_tr_roi
n_val = n_val_synth + n_val_roi

model = build_model()
model.summary()
print(f"\nTotal parameters: {model.count_params():,}")

model.compile(
    optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
    loss={
        'L':  'sparse_categorical_crossentropy',
        'S1': 'sparse_categorical_crossentropy',
        'S2': 'sparse_categorical_crossentropy',
        'S3': 'sparse_categorical_crossentropy',
    },
    loss_weights={'L': 4.0, 'S1': 1.0, 'S2': 1.0, 'S3': 1.0},
    metrics={
        'L':  'accuracy',
        'S1': 'accuracy',
        'S2': 'accuracy',
        'S3': 'accuracy',
    },
)

callbacks = [
    EarlyStopping(monitor='val_loss', patience=6,
                  restore_best_weights=True, verbose=1),
    ReduceLROnPlateau(monitor='val_loss', factor=0.5,
                      patience=3, min_lr=1e-6, verbose=1),
]

BATCH    = 64
AUTOTUNE = tf.data.AUTOTUNE

_out_sig = (
    tf.TensorSpec(shape=(IMG_H, IMG_W, 1), dtype=tf.float32),
    {'L':  tf.TensorSpec(shape=(), dtype=tf.int32),
     'S1': tf.TensorSpec(shape=(), dtype=tf.int32),
     'S2': tf.TensorSpec(shape=(), dtype=tf.int32),
     'S3': tf.TensorSpec(shape=(), dtype=tf.int32)},
)

# Interleave synthetic and ROI randomly so both appear in every batch window.
# sample_from_datasets picks from each sub-dataset proportionally every step.
synth_tr_ds = tf.data.Dataset.from_generator(synth_tr_gen,  output_signature=_out_sig)
roi_tr_ds   = tf.data.Dataset.from_generator(roi_tr_gen,    output_signature=_out_sig)
synth_val_ds= tf.data.Dataset.from_generator(synth_val_gen, output_signature=_out_sig)
roi_val_ds  = tf.data.Dataset.from_generator(roi_val_gen,   output_signature=_out_sig)

w_tr  = [n_tr_synth  / n_tr,  n_tr_roi  / n_tr]
w_val = [n_val_synth / n_val, n_val_roi / n_val]

# Generators are infinite (while True inside), steps_per_epoch controls length.
# shuffle(2000) breaks up consecutive augmentations of the same ROI image.
train_ds = (tf.data.Dataset.sample_from_datasets(
                [synth_tr_ds, roi_tr_ds], weights=w_tr)
            .shuffle(buffer_size=2000)
            .batch(BATCH).prefetch(AUTOTUNE))
val_ds   = (tf.data.Dataset.sample_from_datasets(
                [synth_val_ds, roi_val_ds], weights=w_val)
            .batch(BATCH).prefetch(AUTOTUNE))

steps_per_epoch  = math.ceil(n_tr  / BATCH)
validation_steps = math.ceil(n_val / BATCH)

print(f"\nTraining…")
print(f"  Train: {n_tr:,}  |  Val: {n_val:,}  |  "
      f"Batch: {BATCH}  |  Steps/epoch: {steps_per_epoch}")

model.fit(
    train_ds,
    validation_data=val_ds,
    epochs=5,
    steps_per_epoch=steps_per_epoch,
    validation_steps=validation_steps,
    callbacks=callbacks,
)

model.save("pretrain_4heads_model.h5")
print("\nSaved: pretrain_4heads_model.h5")

export_tflite(model, "timer_model.tflite")

# Build small representative dataset for int8 calibration (load from disk)
_rep_list = []
for _fpath, *_ in roi_paths[:200]:
    _img = cv2.imread(_fpath, cv2.IMREAD_GRAYSCALE)
    if _img is None:
        continue
    _img_r = cv2.resize(_img, (IMG_W, IMG_H), interpolation=cv2.INTER_AREA)
    _rep_list.append(_img_r.astype(np.float32) / 255.0)
_rep_data = np.array(_rep_list)[..., np.newaxis]   # (N, 64, 96, 1)

export_tflite(model, "timer_model_int8.tflite", int8_quantize=True,
              rep_data=_rep_data)


# =====================================================================
# 11. SANITY CHECK  — dùng ảnh thực từ realDatasetRoi
# =====================================================================
print("\nSanity check on real ROI images (log-prob decoding, Goodfellow et al. App. A):")
print(f"{'File':<22} | {'GT':>4} | {'L':>2} | {'S1':>4} {'S2':>4} {'S3':>3} | {'Pred':>6} | OK?")
print("-" * 65)

if roi_paths:
    # Pick up to 3 samples per digit-count (1, 2, 3 digits) from roi_samples.
    # roi_samples is a list of (img_array, L, S1, S2, S3) — no filename stored.
    # Re-scan the directory to get filenames alongside the arrays.
    _roi_files = sorted(f for f in os.listdir(ROI_DIR)
                        if os.path.splitext(f)[1].lower() in ('.png', '.jpg', '.jpeg')
                        and os.path.splitext(f)[0].split('_')[0].isdigit())

    # Build dict: gt_str → first matching filename
    _fname_map = {}
    for fname in _roi_files:
        gt_str = os.path.splitext(fname)[0].split('_')[0]
        if gt_str not in _fname_map:
            _fname_map[gt_str] = fname

    # Select representative numbers (spread across 1/2/3 digits)
    _seen_lengths = {1: 0, 2: 0, 3: 0}
    _check_fnames = []
    for fname in _roi_files:
        gt_str = os.path.splitext(fname)[0].split('_')[0]
        n_dig  = len(gt_str)
        if n_dig in _seen_lengths and _seen_lengths[n_dig] < 4:
            _check_fnames.append(fname)
            _seen_lengths[n_dig] += 1
        if sum(_seen_lengths.values()) >= 12:
            break

    correct_roi = 0
    for fname in _check_fnames:
        gt_str = os.path.splitext(fname)[0].split('_')[0]
        img    = cv2.imread(os.path.join(ROI_DIR, fname), cv2.IMREAD_GRAYSCALE)
        img_r  = cv2.resize(img, (IMG_W, IMG_H), interpolation=cv2.INTER_AREA)
        inp    = img_r.astype(np.float32) / 255.0
        inp    = inp[np.newaxis, ..., np.newaxis]

        pred   = model.predict(inp, verbose=0)
        result = decode_prediction(pred)

        L_pred  = int(np.argmax(pred['L'][0])) + 1
        S1_pred = int(np.argmax(pred['S1'][0]))
        S2_pred = int(np.argmax(pred['S2'][0]))
        S3_pred = int(np.argmax(pred['S3'][0]))
        s1d = str(S1_pred) if S1_pred != BLANK_CLASS else 'BLK'
        s2d = str(S2_pred) if S2_pred != BLANK_CLASS else 'BLK'

        ok = (result == gt_str)
        correct_roi += int(ok)
        print(f"{fname:<22} | {gt_str:>4} | {L_pred:>2} | {s1d:>4} {s2d:>4} "
              f"{S3_pred:>3} | {result:>6} | {'OK' if ok else 'FAIL'}")

    print(f"\n  Sanity pass: {correct_roi}/{len(_check_fnames)}")
else:
    print("  [SKIP] No ROI samples loaded — check ROI_DIR path.")
