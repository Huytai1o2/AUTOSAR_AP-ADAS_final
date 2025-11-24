#!/bin/bash
# Kill all AUTOSAR processes and clean up IceOryx resources

echo "================================================"
echo "  Cleaning up AUTOSAR Adaptive Applications"
echo "================================================"

# Kill all AUTOSAR processes
echo "[1/5] Killing application processes..."
pkill -9 -f "EM|CM|ProviderSensor|ClientSensor|para-exec" 2>/dev/null
sleep 0.5

# Kill RouDi specifically and wait for it to fully terminate
echo "[2/5] Killing RouDi daemon..."
pkill -9 -f "iox-roudi|RouDi" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "  ✓ RouDi terminated, waiting for cleanup..."
    sleep 2  # Wait for RouDi to release shared memory
else
    echo "  ✓ No RouDi found"
fi

# Force kill any remaining processes
pkill -9 -f "EM|CM|RouDi|ProviderSensor|ClientSensor|iox-roudi|para-exec" 2>/dev/null
sleep 0.5

# Clean up IceOryx shared memory
echo "[3/5] Cleaning shared memory..."
rm -rf /dev/shm/iceoryx* /dev/shm/*roudi* 2>/dev/null
echo "  ✓ Shared memory cleaned"

# Clean up IceOryx lock files
echo "[4/5] Cleaning lock files..."
rm -rf /tmp/iox-unique-roudi.lock /tmp/roudi /tmp/roudi.lock 2>/dev/null
echo "  ✓ Lock files removed"

# Verify cleanup
echo "[5/5] Verifying cleanup..."
REMAINING=$(ps aux | grep -E "EM|CM|RouDi|ProviderSensor|ClientSensor|iox-roudi" | grep -v grep | wc -l)
if [ "$REMAINING" -eq 0 ]; then
    echo "  ✓ All processes stopped"
else
    echo "  ⚠ Warning: $REMAINING process(es) still running"
    ps aux | grep -E "EM|CM|RouDi|ProviderSensor|ClientSensor|iox-roudi" | grep -v grep
    echo "  → Attempting force kill..."
    pkill -9 -f "EM|CM|RouDi|ProviderSensor|ClientSensor|iox-roudi|para-exec" 2>/dev/null
    sleep 1
fi

# Final check for IceOryx resources
LOCKS=$(find /tmp -name "*iox*" -o -name "*roudi*" 2>/dev/null | wc -l)
if [ "$LOCKS" -gt 0 ]; then
    echo "  → Removing remaining lock files..."
    rm -rf /tmp/iox-unique-roudi.lock /tmp/roudi /tmp/roudi.lock 2>/dev/null
fi

echo ""
echo "================================================"
echo "  Cleanup complete! Ready to run ./run.sh"
echo "================================================"


