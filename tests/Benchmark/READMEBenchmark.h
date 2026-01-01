#pragma once

#include "RocketSim.h"
#include "ProfilerUtils.h"
#include <random>
#include <vector>
#include <string>

namespace READMEBenchmark {

struct BenchmarkResult {
    uint64_t ticks_simulated;
    double elapsed_seconds;
    double ticks_per_second;
    std::string version_info;
};

// Generate random car controls
RocketSim::CarControls GenerateRandomControls(std::mt19937& rng);

// Run the README benchmark
// collision_meshes_path: path to collision_meshes folder (e.g., "collision_meshes")
// seed: random seed for reproducibility (0 = use random seed)
// num_ticks: number of ticks to simulate (default 1M)
BenchmarkResult RunBenchmark(
    const std::string& collision_meshes_path,
    uint32_t seed = 0,
    uint64_t num_ticks = 1000000
);

// Print benchmark results in README format
void PrintResults(const BenchmarkResult& result);

} // namespace READMEBenchmark

