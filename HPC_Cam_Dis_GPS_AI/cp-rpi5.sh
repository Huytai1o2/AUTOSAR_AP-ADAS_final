# Rename build folder to build-[date]_[time]
BUILD_DIR="build-$(date +%Y-%m-%d)_$(date +%H-%M-%S)"
mv build/ "${BUILD_DIR}"

scp -r "${BUILD_DIR}" l1ttled1no@dn-pi-001.local:~/Documents/adas-builds/

# Lab setup

# scp -r "${BUILD_DIR}" l1ttled1no@172.28.182.53:~/Documents/adas-builds/

#Rename build folder to build
mv "${BUILD_DIR}" build/