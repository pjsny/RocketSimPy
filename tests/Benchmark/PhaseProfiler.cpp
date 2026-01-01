#include "PhaseProfiler.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace PhaseProfiler {

void ProfileStep(RocketSim::Arena* arena, uint64_t num_ticks, ProfileResult& result) {
    result.ticks_simulated = num_ticks;
    result.tick_rate = arena->GetTickRate();
    
    // Warm-up
    for (int i = 0; i < 100; i++) {
        arena->Step(1);
    }
    
    // Profile overall step timing
    ProfilerUtils::Timer timer;
    ProfilerUtils::Statistics step_stats;
    
    // Sample individual step times
    const uint64_t sample_interval = std::max(1ULL, num_ticks / 10000); // Sample ~10k times
    
    timer.Start();
    uint64_t current_tick = 0;
    
    while (current_tick < num_ticks) {
        uint64_t ticks_this_batch = std::min(sample_interval, num_ticks - current_tick);
        
        ProfilerUtils::Timer step_timer;
        step_timer.Start();
        arena->Step(static_cast<int>(ticks_this_batch));
        step_timer.Stop();
        
        // Record per-tick time
        double time_per_tick = step_timer.GetElapsedSeconds() / ticks_this_batch;
        step_stats.AddSample(time_per_tick);
        
        current_tick += ticks_this_batch;
    }
    
    timer.Stop();
    
    result.total_time_seconds = timer.GetElapsedSeconds();
    result.ticks_per_second = num_ticks / result.total_time_seconds;
    result.game_time_per_real_second_minutes = (result.ticks_per_second * (1.0f / result.tick_rate)) / 60.0f;
    
    // Store overall step timing
    PhaseTiming& step_phase = result.phases["Total Step"];
    step_phase.phase_name = "Total Step";
    step_phase.stats = step_stats;
    step_phase.total_time_seconds = result.total_time_seconds;
    step_phase.sample_count = step_stats.Count();
}

ProfileResult RunProfile(
    RocketSim::GameMode game_mode,
    const RocketSim::ArenaConfig& config,
    size_t num_cars,
    uint64_t num_ticks,
    float tick_rate,
    const std::string& config_name
) {
    ProfileResult result;
    result.config_name = config_name;
    result.game_mode = game_mode;
    result.num_cars = num_cars;
    result.use_custom_boost_pads = config.useCustomBoostPads;
    result.use_custom_broadphase = config.useCustomBroadphase;
    result.mem_weight_mode = config.memWeightMode;
    result.tick_rate = tick_rate;
    
    // Create arena
    RocketSim::Arena* arena = RocketSim::Arena::Create(game_mode, config, tick_rate);
    
    // Add cars
    for (size_t i = 0; i < num_cars; i++) {
        RocketSim::Team team = (i % 2 == 0) ? RocketSim::Team::BLUE : RocketSim::Team::ORANGE;
        arena->AddCar(team, RocketSim::CAR_CONFIG_OCTANE);
    }
    
    // Profile
    ProfileStep(arena, num_ticks, result);
    
    // Cleanup
    delete arena;
    
    return result;
}

std::vector<ProfileResult> CompareConfigurations(
    RocketSim::GameMode game_mode,
    size_t num_cars,
    uint64_t num_ticks,
    float tick_rate
) {
    std::vector<ProfileResult> results;
    
    // Default configuration
    RocketSim::ArenaConfig default_config;
    results.push_back(RunProfile(game_mode, default_config, num_cars, num_ticks, tick_rate, "Default"));
    
    // Custom boost pads (slower)
    RocketSim::ArenaConfig custom_boost_config;
    custom_boost_config.useCustomBoostPads = true;
    results.push_back(RunProfile(game_mode, custom_boost_config, num_cars, num_ticks, tick_rate, "Custom Boost Pads"));
    
    // Light memory mode
    RocketSim::ArenaConfig light_mem_config;
    light_mem_config.memWeightMode = RocketSim::ArenaMemWeightMode::LIGHT;
    results.push_back(RunProfile(game_mode, light_mem_config, num_cars, num_ticks, tick_rate, "Light Memory"));
    
    // No custom broadphase
    RocketSim::ArenaConfig no_custom_broadphase_config;
    no_custom_broadphase_config.useCustomBroadphase = false;
    results.push_back(RunProfile(game_mode, no_custom_broadphase_config, num_cars, num_ticks, tick_rate, "Default Broadphase"));
    
    return results;
}

void PrintProfileResults(const ProfileResult& result) {
    std::cout << "\n";
    
    // Print system info first
    ProfilerUtils::PrintSystemInfo();
    
    std::cout << "Performance Profile";
    if (!result.config_name.empty()) {
        std::cout << " (" << result.config_name << ")";
    }
    std::cout << ":\n";
    std::cout << "Game Mode: " << (result.game_mode == RocketSim::GameMode::SOCCAR ? "SOCCAR" : "Other") << "\n";
    std::cout << "Cars: " << result.num_cars << "\n";
    std::cout << "Ticks simulated: " << ProfilerUtils::FormatNumber(result.ticks_simulated) << "\n";
    std::cout << "Tick rate: " << result.tick_rate << " tps\n";
    std::cout << "\n";
    
    // Print table header
    std::cout << std::left << std::setw(35) << "Phase" 
              << std::right << std::setw(12) << "Time/tick"
              << std::setw(12) << "% of tick"
              << std::setw(12) << "Cumul. %" << "\n";
    std::cout << std::string(71, '-') << "\n";
    
    double cumulative_time = 0.0;
    double total_mean = 0.0;
    
    // Calculate total mean time per tick
    if (result.phases.find("Total Step") != result.phases.end()) {
        total_mean = result.phases.at("Total Step").GetMeanMicroseconds();
    }
    
    // Print phases
    for (const auto& [phase_name, phase] : result.phases) {
        double mean_us = phase.GetMeanMicroseconds();
        double percentage = (total_mean > 0) ? (mean_us / total_mean * 100.0) : 0.0;
        cumulative_time += mean_us;
        double cumul_percentage = (total_mean > 0) ? (cumulative_time / total_mean * 100.0) : 0.0;
        
        std::cout << std::left << std::setw(35) << phase.phase_name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(12) << mean_us << " μs"
                  << std::setw(12) << std::setprecision(1) << percentage << "%"
                  << std::setw(12) << std::setprecision(1) << cumul_percentage << "%"
                  << "\n";
    }
    
    std::cout << std::string(71, '-') << "\n";
    std::cout << std::left << std::setw(35) << "Total per tick"
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(12) << total_mean << " μs"
              << std::setw(12) << "100.0%"
              << std::setw(12) << "100.0%" << "\n";
    std::cout << "\n";
    std::cout << "Ticks per second: " << std::fixed << std::setprecision(0) 
              << result.ticks_per_second << " tps\n";
    std::cout << "Game time per real second: " << std::fixed << std::setprecision(1)
              << result.game_time_per_real_second_minutes << " minutes\n";
    std::cout << "\n";
}

void PrintComparison(const std::vector<ProfileResult>& results) {
    std::cout << "\n";
    std::cout << "Configuration Comparison:\n";
    std::cout << std::string(80, '=') << "\n";
    
    // Print header
    std::cout << std::left << std::setw(25) << "Configuration"
              << std::right << std::setw(15) << "Ticks/sec"
              << std::setw(15) << "Time/tick (μs)"
              << std::setw(15) << "Game min/sec" << "\n";
    std::cout << std::string(80, '-') << "\n";
    
    // Find baseline (default config)
    double baseline_tps = 0.0;
    for (const auto& result : results) {
        if (result.config_name == "Default") {
            baseline_tps = result.ticks_per_second;
            break;
        }
    }
    
    // Print each configuration
    for (const auto& result : results) {
        double mean_us = 0.0;
        if (result.phases.find("Total Step") != result.phases.end()) {
            mean_us = result.phases.at("Total Step").GetMeanMicroseconds();
        }
        
        double speedup = (baseline_tps > 0) ? (result.ticks_per_second / baseline_tps) : 1.0;
        std::string speedup_str = (speedup != 1.0) ? 
            " (" + std::to_string(speedup * 100.0) + "%)" : "";
        
        std::cout << std::left << std::setw(25) << result.config_name
                  << std::right << std::fixed << std::setprecision(0)
                  << std::setw(15) << result.ticks_per_second
                  << std::setprecision(2)
                  << std::setw(15) << mean_us
                  << std::setprecision(1)
                  << std::setw(15) << result.game_time_per_real_second_minutes
                  << speedup_str << "\n";
    }
    
    std::cout << std::string(80, '=') << "\n";
    std::cout << "\n";
}

} // namespace PhaseProfiler

