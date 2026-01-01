#!/bin/bash

# RocketSim Benchmark Script
# Runs performance benchmarks and profiling

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

BENCHMARK_EXE="$SCRIPT_DIR/build/tests/PerformanceBenchmark"
MESHES_PATH="$SCRIPT_DIR/collision_meshes"

# Check if benchmark executable exists
if [ ! -f "$BENCHMARK_EXE" ]; then
    echo -e "${YELLOW}Benchmark executable not found. Building with --benchmark...${NC}"
    ./build.sh --benchmark
fi

# Check if collision meshes exist
if [ ! -d "$MESHES_PATH" ]; then
    echo -e "${YELLOW}Warning: Collision meshes not found at $MESHES_PATH${NC}"
    echo "You may need to specify --meshes PATH"
fi

# Parse arguments or use defaults
MODE="${1:-readme}"
DEFAULT_TICKS=1000000

echo -e "${GREEN}Running RocketSim Benchmark...${NC}"
echo "Mode: $MODE"
echo "Executable: $BENCHMARK_EXE"
echo ""

case "$MODE" in
    readme|--readme)
        SEED="${2:-0}"
        TICKS="${3:-$DEFAULT_TICKS}"
        THREADS="${4:-1}"
        echo -e "${BLUE}Running README benchmark (4 cars, SOCCAR, ${TICKS} ticks)...${NC}"
        if [ "$THREADS" -gt 1 ]; then
            "$BENCHMARK_EXE" --readme --ticks "$TICKS" --seed "$SEED" --threads "$THREADS" --meshes "$MESHES_PATH"
        else
            "$BENCHMARK_EXE" --readme --ticks "$TICKS" --seed "$SEED" --meshes "$MESHES_PATH"
        fi
        ;;
    profile|--profile)
        CARS="${2:-2}"
        PROFILE_OPT="${3:-}"
        TICKS="$DEFAULT_TICKS"
        # Check if the third arg is 'fast' or a tick count
        if [ "$PROFILE_OPT" = "--no-subphase" ] || [ "$PROFILE_OPT" = "fast" ]; then
            echo -e "${BLUE}Running phase profiling (${CARS} cars, ${TICKS} ticks, no sub-phases)...${NC}"
            "$BENCHMARK_EXE" --profile --cars "$CARS" --ticks "$TICKS" --meshes "$MESHES_PATH" --no-subphase
        else
            echo -e "${BLUE}Running phase profiling (${CARS} cars, ${TICKS} ticks)...${NC}"
            "$BENCHMARK_EXE" --profile --cars "$CARS" --ticks "$TICKS" --meshes "$MESHES_PATH"
        fi
        ;;
    compare|--compare-configs)
        CARS="${2:-2}"
        echo -e "${BLUE}Comparing configurations (${CARS} cars, ${TICKS} ticks)...${NC}"
        "$BENCHMARK_EXE" --profile --compare-configs --cars "$CARS" --ticks "$TICKS" --meshes "$MESHES_PATH"
        ;;
    multithread|--multithread)
        THREADS="${2:-12}"
        TICKS="${3:-$DEFAULT_TICKS}"
        echo -e "${BLUE}Running multi-threaded benchmark (${THREADS} threads, ${TICKS} ticks per thread)...${NC}"
        "$BENCHMARK_EXE" --readme --threads "$THREADS" --ticks "$TICKS" --meshes "$MESHES_PATH"
        ;;
    help|--help|-h)
        echo "Usage: ./benchmark.sh [MODE] [ARGS...]"
        echo ""
        echo "Modes:"
        echo "  readme              README benchmark (default)"
        echo "  profile [CARS] [fast]  Phase profiling (default: 2 cars)"
        echo "                         Add 'fast' to disable sub-phase profiling"
        echo "  compare [CARS]      Compare configurations (default: 2 cars)"
        echo "  multithread [N] [TICKS]  Multi-threaded benchmark (default: 12 threads, 1M ticks)"
        echo "  help                Show this help"
        echo ""
        echo "Examples:"
        echo "  ./benchmark.sh readme"
        echo "  ./benchmark.sh readme 12345 1000000 1"
        echo "  ./benchmark.sh profile 4              # With sub-phase details (slower)"
        echo "  ./benchmark.sh profile 8 fast         # Without sub-phases (accurate timing)"
        echo "  ./benchmark.sh compare 2"
        echo "  ./benchmark.sh multithread 12"
        echo ""
        echo "Or run directly:"
        echo "  $BENCHMARK_EXE --help"
        exit 0
        ;;
    *)
        echo -e "${YELLOW}Unknown mode: $MODE${NC}"
        echo "Run './benchmark.sh help' for usage"
        exit 1
        ;;
esac

echo -e "${GREEN}Benchmark complete!${NC}"

