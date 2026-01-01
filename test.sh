#!/bin/bash

# RocketSim Test Script
# Builds and runs all unit tests

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

echo -e "${GREEN}Running RocketSim Tests...${NC}"
echo "Build directory: $BUILD_DIR"
echo "Using $NUM_JOBS parallel jobs"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${YELLOW}Configuring with CMake...${NC}"
    cmake ..
fi

# Build the tests
echo -e "${YELLOW}Building RocketSimTests...${NC}"
cmake --build . --target RocketSimTests -j$NUM_JOBS

# Run the tests
echo ""
echo -e "${GREEN}Running tests...${NC}"
echo "=========================================="
ctest --output-on-failure

echo ""
echo -e "${GREEN}Tests completed!${NC}"

