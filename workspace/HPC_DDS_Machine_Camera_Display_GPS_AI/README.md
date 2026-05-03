# HPC DDS Machine — Camera · Display · GPS · AI

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│  Raspberry Pi 4 (aarch64 / RaspberryOS Bookworm 64-bit)              │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │  ProviderSensor  (cores 0-1-2, AUTOSAR Adaptive C++)         │    │
│  │                                                              │    │
│  │  ┌──────────────────────┐   ┌──────────────────────────┐    │    │
│  │  │  GPS Thread          │   │  Camera/Vision Thread    │    │    │
│  │  │                      │   │  (see AI pipeline below) │    │    │
│  │  │  UART /dev/ttyAMA0   │   │                          │    │    │
│  │  │  NMEA parser         │   │  OpenCV VideoCapture(0)  │    │    │
│  │  │  KD-tree (GeoJSON)   │   │  (fallback: test.mp4)   │    │    │
│  │  │  Haversine distance  │   │         │                │    │    │
│  │  │         │            │   │         ▼                │    │    │
│  │  │         ▼            │   │    AI pipeline           │    │    │
│  │  │   SeqLock snapshot   │   │         │                │    │    │
│  │  └──────────┬───────────┘   └─────────┬────────────────┘    │    │
│  │             │                         │                      │    │
│  │             └─────────────────────────┘                      │    │
│  │                  GpsJpegWireHeader + JPEG                     │    │
│  │                  (packed into one DDS sample)                 │    │
│  └───────────────────────────┬──────────────────────────────────┘    │
│                              │  CycloneDDS-CXX (IPC / wlan0)         │
│  ┌───────────────────────────▼──────────────────────────────────┐    │
│  │  ClientSensor  (AUTOSAR Adaptive C++)                         │    │
│  │                                                               │    │
│  │  Unpack wire header → forward JPEG + GPS over TCP             │    │
│  │  Protocol: [uint32 len][JPEG][uint8 in_range]                 │    │
│  │            [int64 node_id][int32 dist_m]                      │    │
│  └───────────────────────────┬──────────────────────────────────┘    │
│                              │  TCP 127.0.0.1:8888                   │
│  ┌───────────────────────────▼──────────────────────────────────┐    │
│  │  show_cam.py  (core 3, Python)                                │    │
│  │                                                               │    │
│  │  Decode JPEG → overlay traffic-light distance warning         │    │
│  │  Fullscreen display  (headless: display:=0)                   │    │
│  └───────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## AI Vision Pipeline (Camera/Vision Thread)

Each camera frame goes through the following stages before being sent over DDS.

```
Raw frame (640×480, BGR)
        │
        ▼
┌───────────────────────────────────────────────────────────────────────┐
│  STEP 1 — Letterbox to 416×416                                        │
│                                                                       │
│  Scale the frame uniformly so the longest side = 416 px,             │
│  then pad the short side with black borders (top/left offsets         │
│  stored in LbInfo for later inverse-mapping).                         │
│                                                                       │
│  Original:  640×480  →  padded:  416×416                              │
└───────────────────────────┬───────────────────────────────────────────┘
                            │  416×416 BGR square
                            ▼
┌───────────────────────────────────────────────────────────────────────┐
│  STEP 2 — YOLO26n Inference (TFLite, 3 threads, int8 preferred)       │
│                                                                       │
│  Detects 4 classes:                                                   │
│    cls=0  left       cls=1  right                                     │
│    cls=2  straight   cls=3  timer (countdown digit display)           │
│                                                                       │
│  Model format auto-detected at load time:                             │
│    FMT_POST   (1,300,6) — end-to-end, output already NMS'd           │
│    FMT_RAW_CM (NC+4, N) — needs NMS (IoU 0.45)                       │
│    FMT_RAW_AM (N, NC+4) — needs NMS (IoU 0.45)                       │
│                                                                       │
│  Confidence threshold: 0.32                                           │
│  Output: list of Boxes in 416-space {x1,y1,x2,y2, conf, cls}         │
└───────────────────────────┬───────────────────────────────────────────┘
                            │
                            ▼
┌───────────────────────────────────────────────────────────────────────┐
│  STEP 3 — Inverse-map boxes back to original resolution               │
│                                                                       │
│  Undo the letterbox transform using the saved LbInfo:                 │
│    x_orig = (x_416 - pad_left) / scale                               │
│    y_orig = (y_416 - pad_top)  / scale                               │
│                                                                       │
│  Coordinates are now in original 640×480 pixel space,                 │
│  which is what the ROI crops below use — so the OCR and HSV           │
│  classifiers operate on the full-resolution pixels rather than        │
│  the downscaled 416×416 input, giving higher accuracy.                │
└───────────────────────────┬───────────────────────────────────────────┘
                            │  Boxes in original-frame space
                            ▼
┌───────────────────────────────────────────────────────────────────────┐
│  STEP 4 — Box selection                                               │
│                                                                       │
│  For each of the 3 direction classes (left / right / straight):       │
│    keep the single highest-confidence detection.                      │
│  All timer boxes (cls=3) are collected separately.                    │
└───────────────────────────┬───────────────────────────────────────────┘
                            │
          ┌─────────────────┼─────────────────────┐
          │ (per direction in parallel threads)    │
          ▼                                        ▼
┌─────────────────────────────┐     ┌──────────────────────────────────┐
│  STEP 5a — HSV Color Detect │     │  STEP 5b — Timer OCR             │
│                             │     │                                  │
│  Crop direction ROI from    │     │  Find the timer box closest to   │
│  the ORIGINAL 640×480 frame │     │  this direction box (Euclidean   │
│  (full resolution).         │     │  centre distance).               │
│                             │     │                                  │
│  Convert BGR → HSV.         │     │  Crop timer ROI from original    │
│  Count pixels in ranges:    │     │  frame (full resolution).        │
│    Red   H∈[0,10]∪[155,179] │     │                                  │
│    Yellow H∈[18,35]         │     │  Resize crop to 96×64 greyscale. │
│    Green  H∈[40,90]         │     │  Run CNN (timer_model.tflite):   │
│  (S≥100, V≥50 for all)      │     │    Output L (3)  → digit count   │
│                             │     │    Output S1 (11) → tens digit   │
│  Winner = colour with most  │     │    Output S2 (11) → units digit  │
│  pixels above threshold.    │     │    Output S3 (10) → ones digit   │
│                             │     │                                  │
│  Result: "red" / "yellow"   │     │  decodeOcr() picks 1, 2, or 3   │
│          / "green"          │     │  digit string via log-probability │
│          / "unknown"        │     │  beam over (L, S1, S2, S3).      │
└────────────┬────────────────┘     └──────────────┬───────────────────┘
             │                                     │
             └──────────────────┬──────────────────┘
                                ▼
┌───────────────────────────────────────────────────────────────────────┐
│  STEP 6 — Annotate on DISPLAY image (320 px wide)                     │
│                                                                       │
│  processFrame() calls cv::resize to 320 px wide FIRST, then all      │
│  bounding boxes are scaled proportionally (sx = 320/640, sy = same). │
│  Drawing on the smaller canvas avoids double-scaling artefacts.       │
│                                                                       │
│  Direction box  →  coloured border (green/yellow/red/grey)            │
│                    label: "<dir>|<colour>|<conf>"                     │
│  Timer box      →  dark-red border                                    │
│                    label: "timer:<digits>"                            │
│  Extra timers   →  olive border, no label                             │
│                                                                       │
│  Box thickness and font scale adapt to image width                    │
│  (reference 640 px → 2 px / 0.5 scale).                              │
└───────────────────────────┬───────────────────────────────────────────┘
                            │  Annotated 320×240 BGR Mat
                            ▼
                   cv::imencode(".jpg", quality=80)
                            │
                            ▼
             Pack: GpsJpegWireHeader (24 B) + JPEG
                            │
                            ▼
               DDS publish → ClientSensor → TCP → show_cam.py
```

---

### Comparison of Original Backbone and MobileNetV4 Backbone of yolo26n

| Metric | YOLO26n Original (int8) | YOLO26n-MobileNetV4 (int8) | Change % |
|---|---:|---:|---:|
| Samples processed | 1074 | **1366** | +27.19% |
| Average latency (Pi 5) | 55.0853 ms | **43.2280 ms** | -21.52% |
| Maximum latency / spike (Pi 5) | 159.7330 ms | **78.6385 ms** | -50.77% |
| Minimum latency (Pi 5) | 50.1171 ms | **36.5037 ms** | -27.16% |
| Inference speed (Intel CPU) | 15.21 ms | **12.33 ms** | -18.93% |
| mAP50 | **89.90%** | 85.75% | -4.15% |
| mAP50-95 | **56.61%** | 53.31% | -3.30% |

Quick conclusion: YOLO26n-MobileNetV4 int8 keeps the same 416 imgsize, but reduces latency clearly on both the Pi 5 and Intel CPU, while mAP drops slightly compared with YOLO26n Original int8.

---

### DDS wire format (`ProviderSensor` → `ClientSensor`)

```
Offset  Size  Field
     0     4  magic = 0x47505331 ('GPS1')
     4     1  version = 1
     5     1  in_range  (1 = within RANGE_SIZE metres of nearest node)
     6     2  reserved
     8     8  node_id   (nearest traffic-light node ID from GeoJSON)
    16     4  dist_m    (distance in metres, signed)
    20     4  jpeg_len
    24     N  JPEG bytes
```

### TCP stream format (`ClientSensor` → `show_cam.py`)

```
[uint32 frame_len][frame_len JPEG bytes][uint8 in_range][int64 node_id][int32 dist_m]
```

### CPU affinity

| Component | Cores |
|-----------|-------|
| ProviderSensor (YOLO + GPS + OCR) | 0, 1, 2 |
| show_cam.py (display) | 3 |

---

## Build

### Prerequisites

| Tool | Version |
|------|---------|
| PARA SDK | `para-sdk_raspi-raspios64_bookworm` at `/opt/` |
| Cross-compiler | `cross-pi-gcc-12.2.0-64` at `/opt/` |
| CMake | ≥ 3.20 |
| Python | ≥ 3.9 (target Pi, for `show_cam.py`) |

### 1 — (First time only) Cross-compile OpenCV + FFmpeg for aarch64

OpenCV 4.8.0 with FFmpeg 6.1.2 are cross-compiled statically and installed
under `deps/opencv` and `deps/ffmpeg`.  Skip if already present.

```bash
./scripts/build_opencv.sh
```

### 2 — Apply DDS SDK header patches and build

`build_raspi.sh` patches three destructors in the CycloneDDS-CXX headers
(required once per SDK install), then runs the full CMake cross-compile.

```bash
./scripts/build_raspi.sh
```

What it does internally:

```bash
# Patch SDK headers (idempotent sed)
sed -i 's/~Reference<DELEGATE>()/~Reference()/g' \
    /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/core/detail/ReferenceImpl.hpp
# … (two more similar patches for TTopicImpl, TDataReaderImpl, DataWriterImpl)

# Cross-compile
source /opt/para-sdk_raspi-raspios64_bookworm/para-env-setup.sh
export PARA_TOOLCHAIN_PATH=/opt/cross-pi-gcc-12.2.0-64
cmake -B build -DPARA_DDS_IMPLEMENTATION=cyclonedds-cxx .
cmake --build build --target install -j$(nproc)

# Post-build: copy runtime files into install tree
cp show_cam.py                    build/install/
cp scripts/roudi_config.toml      build/install/etc/ipc/roudi_config.toml
cp scripts/run.sh                 build/install/

# Patch DDS network config for target Pi
sed -i 's/172.31.36.143/172.28.182.243/g' build/install/etc/com/CM_CmInstantiation.json
sed -i 's/172.31.36.143/172.28.182.243/g' build/install/etc/com/dds/CM_CycloneDDS_0.xml
sed -i 's/eth0/wlan0/g'                   build/install/etc/com/dds/CM_CycloneDDS_0.xml
sed -i 's/eth0/wlan0/g'                   build/install/etc/com/CM_CmInstantiation.json
```

> Edit the IP address and interface name in `build_raspi.sh` if your Pi's
> network differs before running.

### Build output

```
build/install/
├── bin/                  # ProviderSensor, ClientSensor executables
├── lib/                  # shared libraries
├── etc/
│   ├── com/
│   │   ├── CM_CmInstantiation.json
│   │   └── dds/CM_CycloneDDS_0.xml
│   └── ipc/roudi_config.toml
├── show_cam.py
└── run.sh
```

---

## Deploy and run on Raspberry Pi

### 1 — Copy the install tree to the Pi

```bash
rsync -avz --progress build/install/ pi@<PI_IP>:~/dds-app/
```

### 2 — Copy model and data files to the Pi

Place the following files alongside the binaries (`~/dds-app/`):

| File | Description |
|------|-------------|
| `best_yolo26n_e2e_416/best_int8.tflite` | Primary YOLO model (fastest) |
| `best_yolo26n_e2e_416/best_float16.tflite` | fp16 fallback |
| `best_yolo26n_e2e_416/best_float32.tflite` | fp32 fallback |
| `myCNN/timer_model.tflite` | Traffic-light timer OCR model |
| `hochiminh.geojson` | Traffic-node map (GeoJSON FeatureCollection) |
| `test.mp4` | Camera fallback video |

### 3 — Start RouDi (IPC middleware daemon)

RouDi must be running before any AUTOSAR component starts.

```bash
cd ~/dds-app
LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH" ./RouDi \
    --config-file etc/ipc/roudi_config.toml
```

The `roudi_config.toml` allocates shared-memory pools large enough for
JPEG payloads up to 128 MB.

### 4 — Start the display script

Launch `show_cam.py` **before** the C++ components so the TCP server is
ready when `ClientSensor` tries to connect.

```bash
# Fullscreen (default)
python3 ~/dds-app/show_cam.py

# Headless (SSH / no monitor)
python3 ~/dds-app/show_cam.py display:=0
```

### 5 — Start the AUTOSAR application

```bash
cd ~/dds-app
./run.sh
```

`run.sh` sets `LD_LIBRARY_PATH` and invokes `para-exec.sh` which launches
ProviderSensor and ClientSensor as managed AUTOSAR processes.

---

# Appendix: DDS SDK Header Patches

## Config dds-cxx-2.5.0-1 into sdk para-sdk_raspi-raspios64_bookworm

sed -i 's/~Reference<DELEGATE>()/~Reference()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/core/detail/ReferenceImpl.hpp

sed -i 's/~Topic<T>()/~Topic()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/topic/detail/TTopicImpl.hpp

sed -i 's/~DataReader<T>()/~DataReader()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/sub/detail/TDataReaderImpl.hpp

sed -i 's/~DataWriter<T>()/~DataWriter()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/pub/detail/DataWriterImpl.hpp


## Roudi fix (install/etc/ipc/roudi_config.toml)
```
[general]
version = 1
 
[[segment]]
 
[[segment.mempool]]
size = 128
count = 10000

[[segment.mempool]]
size = 1024
count = 5000

[[segment.mempool]]
size = 16384
count = 1000

[[segment.mempool]]
size = 131072
count = 200

[[segment.mempool]]
size = 524288
count = 50

[[segment.mempool]]
size = 1048576
count = 30

[[segment.mempool]]
size = 4194304
count = 10

[[segment.mempool]]
size = 33554432
count = 2

[[segment.mempool]]
size = 134217728
count = 2

```