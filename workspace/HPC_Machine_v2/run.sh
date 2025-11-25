# Run the script 
# First, clean any existing processes
# echo "[CLEANUP] Cleaning existing processes..."
# ./kill-all.sh > /dev/null 2>&1
# sleep 1

# Then find if the build directory exists
if [ -d "build" ]; then
    # If it exists, run the script
    echo "[BUILD] Build directory found, running the script"
    # Setup the environment
    cd ./build/install/
    source /opt/para-sdk/para-env-setup.sh
    # Run the program
    ./para-exec.sh
else
    # If it does not exist, build the project
    ./build.sh
fi