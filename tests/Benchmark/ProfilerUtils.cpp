#include "ProfilerUtils.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <fstream>

namespace ProfilerUtils {

SystemInfo GetSystemInfo() {
    SystemInfo info;
    
    // Get compiler info
    #ifdef __clang__
        info.compiler_name = "Clang";
        info.compiler_version = std::to_string(__clang_major__) + "." + 
                               std::to_string(__clang_minor__) + "." + 
                               std::to_string(__clang_patchlevel__);
    #elif __GNUC__
        info.compiler_name = "GCC";
        info.compiler_version = std::to_string(__GNUC__) + "." + 
                               std::to_string(__GNUC_MINOR__) + "." + 
                               std::to_string(__GNUC_PATCHLEVEL__);
    #elif _MSC_VER
        info.compiler_name = "MSVC";
        info.compiler_version = std::to_string(_MSC_VER);
    #else
        info.compiler_name = "Unknown";
        info.compiler_version = "Unknown";
    #endif
    
    #ifdef __APPLE__
        // macOS
        info.os_name = "macOS";
        
        // Get OS version
        size_t size = 0;
        sysctlbyname("kern.osproductversion", nullptr, &size, nullptr, 0);
        std::vector<char> buffer(size);
        if (sysctlbyname("kern.osproductversion", buffer.data(), &size, nullptr, 0) == 0) {
            info.os_version = std::string(buffer.data(), size - 1);
        }
        
        // Get CPU model
        size = 0;
        sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0);
        buffer.resize(size);
        if (sysctlbyname("machdep.cpu.brand_string", buffer.data(), &size, nullptr, 0) == 0) {
            info.cpu_model = std::string(buffer.data(), size - 1);
        }
        
        // Get CPU frequency
        uint64_t freq = 0;
        size = sizeof(freq);
        if (sysctlbyname("hw.cpufrequency", &freq, &size, nullptr, 0) == 0) {
            double freq_ghz = freq / 1e9;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << freq_ghz << "GHz";
            info.cpu_frequency = oss.str();
        }
        
        // Get number of cores
        int cores = 0;
        size = sizeof(cores);
        sysctlbyname("hw.ncpu", &cores, &size, nullptr, 0);
        info.num_cores = cores;
        
    #elif __linux__
        // Linux
        struct utsname uname_info;
        if (uname(&uname_info) == 0) {
            info.os_name = uname_info.sysname;
            info.os_version = uname_info.release;
        }
        
        // Get CPU info from /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    info.cpu_model = line.substr(pos + 2);
                    break;
                }
            }
            if (line.find("cpu MHz") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    double mhz = std::stod(line.substr(pos + 2));
                    double ghz = mhz / 1000.0;
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << ghz << "GHz";
                    info.cpu_frequency = oss.str();
                }
            }
            if (line.find("processor") != std::string::npos) {
                info.num_cores++;
            }
        }
        if (info.num_cores == 0) {
            info.num_cores = std::thread::hardware_concurrency();
        }
        
    #elif _WIN32
        // Windows
        info.os_name = "Windows";
        
        OSVERSIONINFOEX osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
            info.os_version = std::to_string(osvi.dwMajorVersion) + "." + 
                             std::to_string(osvi.dwMinorVersion);
        }
        
        // Get CPU info
        int cpuInfo[4] = {-1};
        char cpuBrandString[0x40] = {0};
        __cpuid(cpuInfo, 0x80000002);
        memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000003);
        memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000004);
        memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));
        info.cpu_model = std::string(cpuBrandString);
        
        SYSTEM_INFO sysInfo;
        ::GetSystemInfo(&sysInfo);
        info.num_cores = sysInfo.dwNumberOfProcessors;
        
        // Note: Windows CPU frequency detection is more complex, skipping for now
        
    #else
        info.os_name = "Unknown";
        info.os_version = "Unknown";
        info.cpu_model = "Unknown";
        info.num_cores = std::thread::hardware_concurrency();
    #endif
    
    // Clean up CPU model string (remove extra whitespace)
    while (!info.cpu_model.empty() && (info.cpu_model.back() == ' ' || info.cpu_model.back() == '\0')) {
        info.cpu_model.pop_back();
    }
    
    return info;
}

void PrintSystemInfo() {
    SystemInfo info = GetSystemInfo();
    
    std::cout << "OS: " << info.os_name;
    if (!info.os_version.empty()) {
        std::cout << " " << info.os_version;
    }
    std::cout << "\n";
    
    if (!info.cpu_model.empty()) {
        std::cout << "CPU: " << info.cpu_model;
        if (!info.cpu_frequency.empty()) {
            std::cout << " @ " << info.cpu_frequency;
        }
        std::cout << "\n";
    }
    
    if (info.num_cores > 0) {
        std::cout << "Cores: " << info.num_cores << "\n";
    }
    
    if (!info.compiler_name.empty()) {
        std::cout << "Compiler: " << info.compiler_name;
        if (!info.compiler_version.empty()) {
            std::cout << " " << info.compiler_version;
        }
        std::cout << "\n";
    }
    
    std::cout << "=================================\n";
}

} // namespace ProfilerUtils

