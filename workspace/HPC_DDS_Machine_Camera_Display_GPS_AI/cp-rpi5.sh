#!/bin/bash

# Configuration
SOURCE_PATH="build/install"
REMOTE_HOST="pi-tunnel"
REMOTE_BASE_DIR="/home/huytai102"
TIMESTAMP=$(date +%Y_%m_%d_%H_%M_%S)
DESTINATION_NAME="adas_ara_${TIMESTAMP}"
ZIP_FILE="deploy_${TIMESTAMP}.zip"

echo "Starting deployment process..."

if [ -d "$SOURCE_PATH" ]; then
    echo "Compressing contents of $SOURCE_PATH..."
    (cd "$SOURCE_PATH" && zip -rq "../../${ZIP_FILE}" .)

    echo "Transferring $ZIP_FILE to $REMOTE_HOST..."
    # Thêm -o ConnectTimeout để tránh treo máy nếu mạng yếu
    scp "$ZIP_FILE" "${REMOTE_HOST}:${REMOTE_BASE_DIR}/"
    
    if [ $? -eq 0 ]; then
        echo "Unpacking files on remote host..."
        # Dùng -t -t để ép tạo terminal, tránh lỗi "Pseudo-terminal"
        # Dùng một dòng lệnh duy nhất để không phải kết nối nhiều lần
        ssh -t -t "$REMOTE_HOST" "mkdir -p ${REMOTE_BASE_DIR}/${DESTINATION_NAME} && unzip -q ${REMOTE_BASE_DIR}/${ZIP_FILE} -d ${REMOTE_BASE_DIR}/${DESTINATION_NAME} && rm ${REMOTE_BASE_DIR}/${ZIP_FILE}"
        
        rm "$ZIP_FILE"
        
        echo "Success: Deployment complete."
        echo "Location on Pi: ${REMOTE_BASE_DIR}/${DESTINATION_NAME}"
    else
        echo "Error: Failed to transfer zip file."
        rm -f "$ZIP_FILE"
        exit 1
    fi
else
    echo "Error: $SOURCE_PATH not found."
    exit 1
fi