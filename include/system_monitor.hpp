#pragma once
#include "metrics.hpp"
#include "process_parser.hpp"
#include <chrono>
#include <unordered_map>
#include <string>

namespace sysmon {

class SystemMonitor {
public:
    SystemMonitor();
    SystemSnapshot capture_snapshot(std::size_t top_n_procs = 15);

    enum class SortMode {
        CPU_DESC,
        MEM_DESC,
        PID_ASC
    };

    void set_sort_mode(SortMode mode) noexcept { sort_mode_ = mode; }
    SortMode get_sort_mode() const noexcept { return sort_mode_; }

private:
    void collect_system_info(SystemSnapshot& snap);
    void collect_cpu_metrics(SystemSnapshot& snap);
    void collect_memory_metrics(SystemSnapshot& snap);
    void collect_network_metrics(SystemSnapshot& snap, double elapsed_sec);
    void collect_disk_metrics(SystemSnapshot& snap, double elapsed_sec);

    ProcessParser proc_parser_;
    SortMode sort_mode_{SortMode::CPU_DESC};
    std::chrono::steady_clock::time_point last_capture_time_;

    struct RawCpuTicks {
        uint64_t active{0};
        uint64_t total{0};
    };
    RawCpuTicks prev_total_cpu_;
    std::vector<RawCpuTicks> prev_core_cpu_;

    struct RawNetBytes {
        uint64_t rx{0};
        uint64_t tx{0};
    };
    std::unordered_map<std::string, RawNetBytes> prev_net_;

    struct RawDiskBytes {
        uint64_t read{0};
        uint64_t write{0};
    };
    std::unordered_map<std::string, RawDiskBytes> prev_disk_;
};

} // namespace sysmon
