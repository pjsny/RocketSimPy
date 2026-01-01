#include "READMEBenchmark.h"
#include "PhaseProfiler.h"
#include "ProfilerUtils.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>

void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n";
    std::cout << "\n";
    std::cout << "Modes:\n";
    std::cout << "  --readme              Run README benchmark (4 cars, SOCCAR, 1M ticks) [default]\n";
    std::cout << "  --profile             Run phase profiling mode\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  --ticks N             Number of ticks to simulate (default: 1000000)\n";
    std::cout << "  --seed N              Random seed (0 = random, default: 0)\n";
    std::cout << "  --cars N              Number of cars for profiling (default: 2)\n";
    std::cout << "  --compare-configs     Compare different arena configurations\n";
    std::cout << "  --threads N           Number of threads for multi-threaded benchmark (default: 1)\n";
    std::cout << "  --meshes PATH         Path to collision meshes folder (default: collision_meshes)\n";
    std::cout << "  --no-subphase         Disable sub-phase profiling (reduces overhead)\n";
    std::cout << "  --help                Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " --readme\n";
    std::cout << "  " << program_name << " --readme --seed 12345\n";
    std::cout << "  " << program_name << " --profile --cars 4\n";
    std::cout << "  " << program_name << " --profile --compare-configs\n";
    std::cout << "  " << program_name << " --readme --threads 12\n";
}

struct BenchmarkArgs {
    enum Mode {
        README,
        PROFILE
    };
    
    Mode mode = README;
    uint64_t num_ticks = 1000000;
    uint32_t seed = 0;
    size_t num_cars = 2;
    size_t num_threads = 1;
    bool compare_configs = false;
    bool no_subphase = false;
    std::string collision_meshes_path = "collision_meshes";
};

BenchmarkArgs ParseArgs(int argc, char* argv[]) {
    BenchmarkArgs args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--readme") {
            args.mode = BenchmarkArgs::README;
        } else if (arg == "--profile") {
            args.mode = BenchmarkArgs::PROFILE;
        } else if (arg == "--ticks" && i + 1 < argc) {
            args.num_ticks = std::stoull(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            args.seed = std::stoul(argv[++i]);
        } else if (arg == "--cars" && i + 1 < argc) {
            args.num_cars = std::stoul(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            args.num_threads = std::stoul(argv[++i]);
        } else if (arg == "--compare-configs") {
            args.compare_configs = true;
        } else if (arg == "--no-subphase") {
            args.no_subphase = true;
        } else if (arg == "--meshes" && i + 1 < argc) {
            args.collision_meshes_path = argv[++i];
        } else if (arg == "--help") {
            PrintUsage(argv[0]);
            exit(0);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            exit(1);
        }
    }
    
    return args;
}

void RunMultiThreadedREADME(const BenchmarkArgs& args) {
    std::cout << "Running multi-threaded README benchmark...\n";
    std::cout << "Threads: " << args.num_threads << "\n";
    std::cout << "Ticks per thread: " << ProfilerUtils::FormatNumber(args.num_ticks) << "\n";
    std::cout << "\n";
    
    std::vector<std::thread> threads;
    std::vector<READMEBenchmark::BenchmarkResult> results(args.num_threads);
    std::atomic<size_t> threads_completed(0);
    
    // Initialize RocketSim once (thread-safe)
    std::filesystem::path meshes_path = args.collision_meshes_path;
    if (!std::filesystem::exists(meshes_path)) {
        meshes_path = std::filesystem::current_path() / args.collision_meshes_path;
    }
    RocketSim::Init(meshes_path, true);
    
    // Run benchmark on each thread
    for (size_t t = 0; t < args.num_threads; t++) {
        threads.emplace_back([&, t]() {
            uint32_t thread_seed = (args.seed == 0) ? (static_cast<uint32_t>(t + 1) * 12345) : (args.seed + static_cast<uint32_t>(t));
            results[t] = READMEBenchmark::RunBenchmark(
                args.collision_meshes_path,
                thread_seed,
                args.num_ticks
            );
            threads_completed++;
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Aggregate results
    double total_tps = 0.0;
    double total_time = 0.0;
    for (const auto& result : results) {
        total_tps += result.ticks_per_second;
        total_time += result.elapsed_seconds;
    }
    
    std::cout << "\n";
    std::cout << "=================================\n";
    std::cout << "Multi-threaded Performance (" << args.num_threads << " threads, " 
              << ProfilerUtils::FormatNumber(args.num_ticks) << " ticks per thread):\n";
    std::cout << "Total ticks per second: " << std::fixed << std::setprecision(0) 
              << total_tps << " tps\n";
    std::cout << "Average per thread: " << std::fixed << std::setprecision(0)
              << (total_tps / args.num_threads) << " tps\n";
    std::cout << "Total elapsed time: " << std::fixed << std::setprecision(3)
              << total_time << " seconds\n";
    std::cout << "=================================\n";
}

int main(int argc, char* argv[]) {
    BenchmarkArgs args = ParseArgs(argc, argv);
    
    if (args.mode == BenchmarkArgs::README) {
        if (args.num_threads > 1) {
            RunMultiThreadedREADME(args);
        } else {
            // Single-threaded README benchmark
            std::cout << "Running README benchmark...\n";
            std::cout << "Ticks: " << ProfilerUtils::FormatNumber(args.num_ticks) << "\n";
            if (args.seed != 0) {
                std::cout << "Seed: " << args.seed << "\n";
            }
            std::cout << "\n";
            
            READMEBenchmark::BenchmarkResult result = READMEBenchmark::RunBenchmark(
                args.collision_meshes_path,
                args.seed,
                args.num_ticks
            );
            
            READMEBenchmark::PrintResults(result);
        }
    } else if (args.mode == BenchmarkArgs::PROFILE) {
        // Initialize RocketSim
        std::filesystem::path meshes_path = args.collision_meshes_path;
        if (!std::filesystem::exists(meshes_path)) {
            meshes_path = std::filesystem::current_path() / args.collision_meshes_path;
        }
        RocketSim::Init(meshes_path, true);
        
        if (args.compare_configs) {
            // Compare different configurations
            std::cout << "Running configuration comparison...\n";
            std::cout << "Cars: " << args.num_cars << "\n";
            std::cout << "Ticks: " << ProfilerUtils::FormatNumber(args.num_ticks) << "\n";
            std::cout << "\n";
            
            std::vector<PhaseProfiler::ProfileResult> results = 
                PhaseProfiler::CompareConfigurations(
                    RocketSim::GameMode::SOCCAR,
                    args.num_cars,
                    args.num_ticks,
                    120.0f
                );
            
            PhaseProfiler::PrintComparison(results);
        } else {
            // Single configuration profiling
            std::cout << "Running phase profiling...\n";
            std::cout << "Cars: " << args.num_cars << "\n";
            std::cout << "Ticks: " << ProfilerUtils::FormatNumber(args.num_ticks) << "\n";
            std::cout << "\n";
            
            RocketSim::ArenaConfig config;
            PhaseProfiler::ProfileResult result = PhaseProfiler::RunProfile(
                RocketSim::GameMode::SOCCAR,
                config,
                args.num_cars,
                args.num_ticks,
                120.0f,
                "Default",
                !args.no_subphase  // enable sub-phase profiling unless --no-subphase
            );
            
            PhaseProfiler::PrintProfileResults(result);
        }
    }
    
    return 0;
}

