#pragma once
#include "metrics.hpp"
#include <unordered_map>
#include <vector>

namespace sysmon {

class ProcessParser {
public:
    ProcessParser() = default;
    std::vector<ProcessInfo> collect_processes(uint64_t total_ram_kb, double elapsed_sec);

private:
    std::unordered_map<int, uint64_t> prev_proc_ticks_;
};

} // namespace sysmon
