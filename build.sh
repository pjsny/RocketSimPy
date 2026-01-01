#!/bin/bash

# RocketSim Build Script
# Automates the CMake build process

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

BUILD_DIR="build"
NUM_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo -e "${GREEN}Building RocketSim...${NC}"
echo "Build directory: $BUILD_DIR"
echo "Using $NUM_JOBS parallel jobs"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake ..

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . -j$NUM_JOBS

echo -e "${GREEN}Build complete!${NC}"
echo "Library location: $SCRIPT_DIR/$BUILD_DIR/libRocketSim.a"

