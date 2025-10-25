#!/bin/bash
# filepath: /home/l1ttled1no/PARA_SDK/run.sh

export IMAGE_REPO="edupopcornsar/pc-linux"
export IMAGE_TAG="R2011-v250829"

# Default SHM size
export SHM_SIZE="8g"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--shm-size)
            SHM_SIZE="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -s, --shm-size SIZE    Set shared memory size (default: 8g)"
            echo "  -h, --help            Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo "Starting container with SHM size: ${SHM_SIZE}"
sudo docker run --shm-size ${SHM_SIZE} -it ${IMAGE_REPO}:${IMAGE_TAG}