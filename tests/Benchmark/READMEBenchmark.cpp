#include "READMEBenchmark.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include <iostream>
#include <iomanip>
#include <filesystem>

namespace READMEBenchmark {

RocketSim::CarControls GenerateRandomControls(std::mt19937& rng) {
    RocketSim::CarControls controls;
    
    std::uniform_real_distribution<float> float_dist(-1.0f, 1.0f);
    std::uniform_int_distribution<int> bool_dist(0, 1);
    
    controls.throttle = float_dist(rng);
    controls.steer = float_dist(rng);
    controls.pitch = float_dist(rng);
    controls.yaw = float_dist(rng);
    controls.roll = float_dist(rng);
    controls.jump = bool_dist(rng) != 0;
    controls.boost = bool_dist(rng) != 0;
    controls.handbrake = bool_dist(rng) != 0;
    
    return controls;
}

BenchmarkResult RunBenchmark(
    const std::string& collision_meshes_path,
    uint32_t seed,
    uint64_t num_ticks
) {
    BenchmarkResult result;
    result.ticks_simulated = num_ticks;
    
    // Initialize random number generator
    std::mt19937 rng;
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }
    rng.seed(seed);
    
    // Initialize RocketSim with collision meshes
    std::filesystem::path meshes_path = collision_meshes_path;
    if (!std::filesystem::exists(meshes_path)) {
        // Try relative to current directory
        meshes_path = std::filesystem::current_path() / collision_meshes_path;
        if (!std::filesystem::exists(meshes_path)) {
            throw std::runtime_error("Collision meshes path not found: " + collision_meshes_path);
        }
    }
    
    RocketSim::Init(meshes_path, true); // silent = true to avoid log spam
    
    // Create SOCCAR arena
    RocketSim::ArenaConfig config;
    RocketSim::Arena* arena = RocketSim::Arena::Create(RocketSim::GameMode::SOCCAR, config, 120.0f);
    
    // Add 4 cars (2 on each team)
    std::vector<RocketSim::Car*> cars;
    cars.push_back(arena->AddCar(RocketSim::Team::BLUE, RocketSim::CAR_CONFIG_OCTANE));
    cars.push_back(arena->AddCar(RocketSim::Team::BLUE, RocketSim::CAR_CONFIG_OCTANE));
    cars.push_back(arena->AddCar(RocketSim::Team::ORANGE, RocketSim::CAR_CONFIG_OCTANE));
    cars.push_back(arena->AddCar(RocketSim::Team::ORANGE, RocketSim::CAR_CONFIG_OCTANE));
    
    // Pre-generate control sequences for each car
    // Each car has independent random intervals (2-60 ticks) when controls change
    struct CarControlSequence {
        std::vector<RocketSim::CarControls> controls;
        std::vector<uint32_t> change_ticks; // Tick numbers when controls change
        uint32_t current_index = 0;
    };
    
    std::vector<CarControlSequence> car_sequences(cars.size());
    std::uniform_int_distribution<uint32_t> interval_dist(2, 60);
    
    // Generate control sequences for each car
    for (size_t car_idx = 0; car_idx < cars.size(); car_idx++) {
        auto& sequence = car_sequences[car_idx];
        uint64_t current_tick = 0;
        
        while (current_tick < num_ticks) {
            // Generate next control change interval
            uint32_t interval = interval_dist(rng);
            current_tick += interval;
            
            if (current_tick <= num_ticks) {
                sequence.change_ticks.push_back(static_cast<uint32_t>(current_tick));
                sequence.controls.push_back(GenerateRandomControls(rng));
            }
        }
        
        // Set initial controls
        if (!sequence.controls.empty()) {
            cars[car_idx]->controls = sequence.controls[0];
        }
    }
    
    // Warm-up: run a few ticks to stabilize
    for (int i = 0; i < 100; i++) {
        arena->Step(1);
    }
    
    // Reset timer and run benchmark
    ProfilerUtils::Timer timer;
    timer.Start();
    
    uint64_t current_tick = 0;
    while (current_tick < num_ticks) {
        // Update controls for each car if needed
        for (size_t car_idx = 0; car_idx < cars.size(); car_idx++) {
            auto& sequence = car_sequences[car_idx];
            if (sequence.current_index < sequence.change_ticks.size()) {
                uint32_t next_change_tick = sequence.change_ticks[sequence.current_index];
                if (current_tick >= next_change_tick) {
                    sequence.current_index++;
                    if (sequence.current_index < sequence.controls.size()) {
                        cars[car_idx]->controls = sequence.controls[sequence.current_index];
                    }
                }
            }
        }
        
        // Step simulation
        arena->Step(1);
        current_tick++;
    }
    
    timer.Stop();
    
    result.elapsed_seconds = timer.GetElapsedSeconds();
    result.ticks_per_second = num_ticks / result.elapsed_seconds;
    
    // Cleanup
    delete arena;
    
    return result;
}

void PrintResults(const BenchmarkResult& result) {
    // Print system info first
    ProfilerUtils::PrintSystemInfo();
    
    std::cout << "Arena: Default (Soccar)\n";
    std::cout << "Cars: 2 on each team (2v2)\n";
    std::cout << "Inputs: Randomly pre-generated, changed every 2-60 ticks for each car\n";
    std::cout << "=================================\n";
    std::cout << "Single-thread performance (calculated using average CPU cycles per tick on the RocketSim thread) (" 
              << ProfilerUtils::FormatNumber(result.ticks_simulated) << " ticks simulated):\n";
    std::cout << "Current version = " << std::fixed << std::setprecision(0) 
              << result.ticks_per_second << " tps\n";
    std::cout << "=================================\n";
    std::cout << "Elapsed time: " << std::fixed << std::setprecision(3) 
              << result.elapsed_seconds << " seconds\n";
    std::cout << "Ticks per second: " << std::fixed << std::setprecision(0) 
              << result.ticks_per_second << " tps\n";
}

} // namespace READMEBenchmark

