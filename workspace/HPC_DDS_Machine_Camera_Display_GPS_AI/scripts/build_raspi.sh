# #!/bin/bash
# set -e

# # Environment setup for para-sdk_raspi-raspios64_bookworm
# source /opt/para-sdk_raspi-raspios64_bookworm/para-env-setup.sh

# # Export Toolchain Path
# export PARA_TOOLCHAIN_PATH=/opt/cross-pi-gcc-12.2.0-64

# # CMake configuration
# echo "Cleaning build directory..."
# rm -rf build
# echo "Configuring CMake..."
# cmake -B build -DPARA_DDS_IMPLEMENTATION=cyclonedds-cxx .
# # cmake -B build .

# # Build and install
# echo "Building and Installing..."
# cmake --build build --target install -j$(nproc)

sed -i 's/~Reference<DELEGATE>()/~Reference()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/core/detail/ReferenceImpl.hpp

sed -i 's/~Topic<T>()/~Topic()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/topic/detail/TTopicImpl.hpp

sed -i 's/~DataReader<T>()/~DataReader()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/sub/detail/TDataReaderImpl.hpp

sed -i 's/~DataWriter<T>()/~DataWriter()/g' /opt/para-sdk_raspi-raspios64_bookworm/external/include/ddscxx/dds/pub/detail/DataWriterImpl.hpp

#!/bin/bash
set -e

# ============================================================================
# Options (override from CLI, e.g. ./build_raspi.sh --home-vlan=yes)
# ============================================================================
# HOME_VLAN="${HOME_VLAN:-no}"
# REMOTE_USER="l1ttled1no"
# REMOTE_HOST="dn-pi-001.local"
# REMOTE_DIR="~/Documents/"

# for arg in "$@"; do
#     case "$arg" in
#         --home-vlan=*|-home-vlan=*)
#             HOME_VLAN="${arg#*=}"
#             ;;
#         *)
#             echo "Unknown option: $arg"
#             ;;
#     esac
# done

# Environment setup for para-sdk_raspi-raspios64_bookworm
source /opt/para-sdk_raspi-raspios64_bookworm/para-env-setup.sh

# Export Toolchain Path
export PARA_TOOLCHAIN_PATH=/opt/cross-pi-gcc-12.2.0-64

# CycloneDDS runtime library path (để idlc chạy được)
export LD_LIBRARY_PATH=/opt/para-sdk_raspi-raspios64_bookworm/share/cyclonedds/lib:$LD_LIBRARY_PATH

# CMake configuration
echo "Cleaning build directory..."
rm -rf build

echo "Configuring CMake..."
cmake -B build \
    -DPARA_DDS_IMPLEMENTATION=cyclonedds-cxx \
    .

# Build and install
echo "Building and Installing..."
cmake --build build --target install -j$(nproc)

# Copy show_cam.py to build folder

cp show_cam.py build/install/

# Config roudi: ./build/install/etc/ipc/roudi_config.toml
# Replace the file in install folder

cp ./scripts/roudi_config.toml build/install/etc/ipc/roudi_config.toml

cp ./scripts/run.sh build/install/

# Change the ip address in the CM_CmInstantiation.json file
sed -i 's/172.31.36.143/172.28.182.243/g' build/install/etc/com/CM_CmInstantiation.json

# Change the ip address in the CM_CycloneDDS_0.xml file
sed -i 's/172.31.36.143/172.28.182.243/g' build/install/etc/com/dds/CM_CycloneDDS_0.xml

# Change network interface from eth0 to wlan0
sed -i 's/eth0/wlan0/g' build/install/etc/com/dds/CM_CycloneDDS_0.xml

# Change network interface from eth0 to wlan0 in the JSON file
sed -i 's/eth0/wlan0/g' build/install/etc/com/CM_CmInstantiation.json

