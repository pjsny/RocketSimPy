#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#elif __linux__
#include <sys/utsname.h>
#include <fstream>
#elif _WIN32
#include <windows.h>
#include <intrin.h>
#endif

namespace ProfilerUtils {

// High-resolution timer using steady_clock
class Timer {
public:
    Timer() : start_time_(), end_time_(), is_running_(false) {}
    
    void Start() {
        start_time_ = std::chrono::steady_clock::now();
        is_running_ = true;
    }
    
    void Stop() {
        if (is_running_) {
            end_time_ = std::chrono::steady_clock::now();
            is_running_ = false;
        }
    }
    
    double GetElapsedSeconds() const {
        if (is_running_) {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration<double>(now - start_time_).count();
        }
        return std::chrono::duration<double>(end_time_ - start_time_).count();
    }
    
    double GetElapsedMicroseconds() const {
        return GetElapsedSeconds() * 1e6;
    }
    
    double GetElapsedNanoseconds() const {
        return GetElapsedSeconds() * 1e9;
    }
    
    bool IsRunning() const { return is_running_; }
    
    void Reset() {
        start_time_ = {};
        end_time_ = {};
        is_running_ = false;
    }

private:
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    bool is_running_;
};

// RAII timer that automatically stops on scope exit
class ScopedTimer {
public:
    ScopedTimer(Timer& timer) : timer_(timer) {
        timer_.Start();
    }
    
    ~ScopedTimer() {
        timer_.Stop();
    }
    
    // Non-copyable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    Timer& timer_;
};

// Statistics calculator for timing data
class Statistics {
public:
    Statistics() {}
    
    void AddSample(double value) {
        samples_.push_back(value);
    }
    
    void Clear() {
        samples_.clear();
    }
    
    size_t Count() const {
        return samples_.size();
    }
    
    double Mean() const {
        if (samples_.empty()) return 0.0;
        double sum = std::accumulate(samples_.begin(), samples_.end(), 0.0);
        return sum / samples_.size();
    }
    
    double Median() const {
        if (samples_.empty()) return 0.0;
        std::vector<double> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        if (n % 2 == 0) {
            return (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
        } else {
            return sorted[n/2];
        }
    }
    
    double Min() const {
        if (samples_.empty()) return 0.0;
        return *std::min_element(samples_.begin(), samples_.end());
    }
    
    double Max() const {
        if (samples_.empty()) return 0.0;
        return *std::max_element(samples_.begin(), samples_.end());
    }
    
    double StdDev() const {
        if (samples_.empty()) return 0.0;
        double mean = Mean();
        double sq_sum = 0.0;
        for (double sample : samples_) {
            double diff = sample - mean;
            sq_sum += diff * diff;
        }
        return std::sqrt(sq_sum / samples_.size());
    }
    
    double Percentile(double p) const {
        if (samples_.empty()) return 0.0;
        std::vector<double> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        size_t index = static_cast<size_t>(std::ceil(p * sorted.size() / 100.0)) - 1;
        if (index >= sorted.size()) index = sorted.size() - 1;
        return sorted[index];
    }
    
    double P95() const { return Percentile(95.0); }
    double P99() const { return Percentile(99.0); }
    
    const std::vector<double>& GetSamples() const {
        return samples_;
    }

private:
    std::vector<double> samples_;
};

// Format time in appropriate units
inline std::string FormatTime(double seconds) {
    if (seconds < 1e-6) {
        return std::to_string(seconds * 1e9) + " ns";
    } else if (seconds < 1e-3) {
        return std::to_string(seconds * 1e6) + " μs";
    } else if (seconds < 1.0) {
        return std::to_string(seconds * 1e3) + " ms";
    } else {
        return std::to_string(seconds) + " s";
    }
}

// Format time per tick in microseconds
inline std::string FormatTimePerTick(double seconds_per_tick) {
    double us_per_tick = seconds_per_tick * 1e6;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f μs", us_per_tick);
    return std::string(buffer);
}

// Format large numbers with commas
inline std::string FormatNumber(uint64_t n) {
    std::string s = std::to_string(n);
    int pos = s.length() - 3;
    while (pos > 0) {
        s.insert(pos, ",");
        pos -= 3;
    }
    return s;
}

// System information structure
struct SystemInfo {
    std::string os_name;
    std::string os_version;
    std::string cpu_model;
    std::string cpu_frequency;
    std::string ram_speed;
    std::string compiler_name;
    std::string compiler_version;
    int num_cores = 0;
};

// Get system information
SystemInfo GetSystemInfo();

// Print system information in README format
void PrintSystemInfo();

} // namespace ProfilerUtils

