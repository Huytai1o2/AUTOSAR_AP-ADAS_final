#!/bin/bash
# Run the script with logging

# Create log directory if it doesn't exist
LOG_DIR="./log"
if [ ! -d "$LOG_DIR" ]; then
    echo "[LOG] Creating log directory: $LOG_DIR"
    mkdir -p "$LOG_DIR"
    chmod 777 "$LOG_DIR"
fi

# Generate timestamp for log filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$LOG_DIR/run_${TIMESTAMP}.log"

echo "[LOG] Log file: $LOG_FILE"
echo "=== Run started at $(date) ===" | tee "$LOG_FILE"

# First, clean any existing processes (optional)
# echo "[CLEANUP] Cleaning existing processes..." | tee -a "$LOG_FILE"
# ./kill-all.sh > /dev/null 2>&1
# sleep 1

# Check for build directories, prioritizing build_v4
if [ -d "build_v4" ]; then
    BUILD_DIR="build_v4"
elif [ -d "build" ]; then
    BUILD_DIR="build"
else
    # If neither exists, build the project
    echo "[ERROR] Build directory not found. Please run ./build.sh first" | tee -a "$LOG_FILE"
    exit 1
fi

echo "[BUILD] Using build directory: $BUILD_DIR" | tee -a "$LOG_FILE"

# Setup the environment
cd "./$BUILD_DIR/install/"
source /opt/para-sdk/para-env-setup.sh

# Add local lib directory to LD_LIBRARY_PATH
export LD_LIBRARY_PATH="$(pwd)/lib:$LD_LIBRARY_PATH"
echo "[ENV] LD_LIBRARY_PATH: $LD_LIBRARY_PATH" | tee -a "../../$LOG_FILE"

# Run the program and redirect output to log file
echo "[RUN] Starting para-exec.sh..." | tee -a "../../$LOG_FILE"
./para-exec.sh 2>&1 | tee -a "../../$LOG_FILE"