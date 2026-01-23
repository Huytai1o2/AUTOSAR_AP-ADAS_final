# CMake configuration
cmake -B build .

# Build and install
cmake --build build --target install -j $(nproc)

# # Setup the environment
# chmod +x /opt/para-sdk/para-env-setup.sh
# chmod +x ./build/install/para-exec.sh
# cd ./build/install/

# source /opt/para-sdk/para-env-setup.sh
