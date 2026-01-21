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

# Then find if the build directory exists
if [ -d "build" ]; then
    # If it exists, run the script
    echo "[BUILD] Build directory found, running the script" | tee -a "$LOG_FILE"
    # Setup the environment
    cd ./build/install/
    source /opt/para-sdk/para-env-setup.sh
    # Run the program and redirect output to log file
    echo "[RUN] Starting para-exec.sh..." | tee -a "../../$LOG_FILE"
    ./para-exec.sh 2>&1 | tee -a "../../$LOG_FILE"
else
    # If it does not exist, build the project
    echo "[ERROR] Build directory not found. Please run ./build.sh first" | tee -a "$LOG_FILE"
    exit 1
fi