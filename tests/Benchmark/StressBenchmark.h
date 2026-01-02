#pragma once

#include "RocketSim.h"
#include "ProfilerUtils.h"
#include <random>
#include <vector>
#include <string>

namespace StressBenchmark {

// Benchmark configuration matching the Rust implementation
constexpr int NUM_CARS = 6;
constexpr int NUM_EPISODE_TICKS = 10000;
constexpr int NUM_EPISODES = 100;
constexpr int TOTAL_TICKS = NUM_EPISODES * NUM_EPISODE_TICKS;

// Control update probability per tick (5% chance = avg 6 updates/sec at 120Hz)
constexpr float UPDATE_CHANCE = 0.05f;

// Ball velocity boost magnitude on episode reset
constexpr float VEL_ADD_MAG = 1000.0f;

struct BenchmarkResult {
    uint64_t ticks_simulated;
    double elapsed_seconds;
    double ticks_per_second;
    uint64_t total_ball_touches;
};

// Simple fast random number generator
class FastRNG {
public:
    FastRNG(uint64_t seed = 0) : state(seed) {}
    
    void seed(uint64_t s) { state = s; }
    
    // Returns float in [0, 1)
    float rand() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return (state & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
    }
    
    // Returns float in [-1, 1)
    float rand_axis_val() {
        return rand() * 2.0f - 1.0f;
    }
    
    // Returns true with given probability
    bool rand_chance(float thresh) {
        return rand() < thresh;
    }

private:
    uint64_t state;
};

// Calculate bot controls that chase the ball (matches Rust implementation)
RocketSim::CarControls CalcBotControls(
    const RocketSim::CarState& car_state,
    const RocketSim::BallState& ball_state,
    FastRNG& rng
);

// Run the stress benchmark matching Rust implementation
// collision_meshes_path: path to collision_meshes folder
// seed: random seed for reproducibility (0 = use random seed)
BenchmarkResult RunBenchmark(
    const std::string& collision_meshes_path,
    uint32_t seed = 0
);

// Print benchmark results
void PrintResults(const BenchmarkResult& result);

} // namespace StressBenchmark

