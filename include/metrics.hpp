#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace sysmon {

struct CpuCoreMetrics {
    int core_id;
    double usage_percent;
    uint64_t user_ticks;
    uint64_t system_ticks;
    uint64_t idle_ticks;
    uint64_t total_ticks;
};

struct MemoryMetrics {
    uint64_t total_kb;
    uint64_t available_kb;
    uint64_t used_kb;
    uint64_t swap_total_kb;
    uint64_t swap_used_kb;
    double ram_usage_percent;
    double swap_usage_percent;
};

struct NetworkMetrics {
    std::string interface_name;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    double rx_rate_kbps;
    double tx_rate_kbps;
};

struct DiskMetrics {
    std::string device_name;
    uint64_t read_bytes;
    uint64_t write_bytes;
    double read_rate_kbps;
    double write_rate_kbps;
};

struct ProcessInfo {
    int pid;
    std::string name;
    std::string state;
    double cpu_percent;
    double mem_percent;
    uint64_t rss_kb;
    uint64_t threads;
};

struct SystemSnapshot {
    std::string hostname;
    std::string os_version;
    uint64_t uptime_seconds;
    double total_cpu_percent;
    double load_avg_1m;
    double load_avg_5m;
    double load_avg_15m;
    std::vector<CpuCoreMetrics> cores;
    MemoryMetrics memory;
    std::vector<NetworkMetrics> networks;
    std::vector<DiskMetrics> disks;
    std::vector<ProcessInfo> processes;
};

} // namespace sysmon
