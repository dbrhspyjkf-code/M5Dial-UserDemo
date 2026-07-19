#!/bin/bash

# Configs
IDF_PATH=${IDF_PATH:-$HOME/esp-idf-v5.1.3}
# Auto-detect the board's serial port (macOS: /dev/cu.usbmodem*), override via env if needed
SERIAL_PORT=${SERIAL_PORT:-$(ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null | head -1)}


# Help shit
help() {
    sed -rn 's/^### ?//;T;p' "$0"
}

if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    help
    exit 1
fi

# Get idf
. ${IDF_PATH}/export.sh

# Build and flash and monitor
idf.py -p ${SERIAL_PORT} flash -b 1500000 monitor
