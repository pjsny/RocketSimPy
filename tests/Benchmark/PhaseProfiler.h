#pragma once

#include "RocketSim.h"
#include "ProfilerUtils.h"
#include "Sim/Arena/ArenaConfig/ArenaConfig.h"
#include <string>
#include <map>
#include <vector>

namespace PhaseProfiler {

struct PhaseTiming {
    std::string phase_name;
    ProfilerUtils::Statistics stats;
    double total_time_seconds = 0.0;
    uint64_t sample_count = 0;
    
    double GetMeanMicroseconds() const {
        return stats.Mean() * 1e6;
    }
    
    double GetMeanSeconds() const {
        return stats.Mean();
    }
};

struct ProfileResult {
    std::map<std::string, PhaseTiming> phases;
    double total_time_seconds = 0.0;
    uint64_t ticks_simulated = 0;
    double ticks_per_second = 0.0;
    double game_time_per_real_second_minutes = 0.0;
    float tick_rate = 120.0f;
    
    // Configuration info
    std::string config_name;
    RocketSim::GameMode game_mode;
    size_t num_cars = 0;
    bool use_custom_boost_pads = false;
    bool use_custom_broadphase = false;
    RocketSim::ArenaMemWeightMode mem_weight_mode = RocketSim::ArenaMemWeightMode::HEAVY;
};

// Profile a single arena step (times the entire step)
// This is a simplified version since we can't easily instrument Arena internals
void ProfileStep(RocketSim::Arena* arena, uint64_t num_ticks, ProfileResult& result);

// Run profiling with specific configuration
ProfileResult RunProfile(
    RocketSim::GameMode game_mode,
    const RocketSim::ArenaConfig& config,
    size_t num_cars,
    uint64_t num_ticks,
    float tick_rate = 120.0f,
    const std::string& config_name = ""
);

// Compare multiple configurations
std::vector<ProfileResult> CompareConfigurations(
    RocketSim::GameMode game_mode,
    size_t num_cars,
    uint64_t num_ticks,
    float tick_rate = 120.0f
);

// Print profile results in table format
void PrintProfileResults(const ProfileResult& result);
void PrintComparison(const std::vector<ProfileResult>& results);

} // namespace PhaseProfiler

