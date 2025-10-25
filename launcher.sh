#!/bin/bash
# Launcher script for AUTOSARio (Ubuntu 25.10 / Ptyxis)

APP_DIR="/home/l1ttled1no/AutoSAR.io"
APP_BIN="./AutoSARio"    # exact filename, case-sensitive

# Use -- to stop ptyxis option parsing, then run bash -lc to execute the command string
ptyxis exec -- bash -lc "cd '$APP_DIR' && sudo '$APP_BIN'; echo; read -p 'Press Enter to close...'"

