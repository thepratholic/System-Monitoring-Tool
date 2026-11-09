#include "system_monitor.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    sysmon::SystemMonitor mon;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto snap = mon.capture_snapshot(10);

    assert(!snap.hostname.empty() && "Hostname should not be empty");
    assert(snap.memory.total_kb > 0 && "Total RAM should be > 0 KB");
    assert(snap.memory.ram_usage_percent >= 0.0 && snap.memory.ram_usage_percent <= 100.0);
    assert(snap.total_cpu_percent >= 0.0 && snap.total_cpu_percent <= 100.0);
    assert(!snap.processes.empty() && "Process list should not be empty");

    std::cout << "[PASS] Hostname      : " << snap.hostname << "\n";
    std::cout << "[PASS] OS Version    : " << snap.os_version << "\n";
    std::cout << "[PASS] Total CPU %   : " << snap.total_cpu_percent << "%\n";
    std::cout << "[PASS] Memory Total  : " << snap.memory.total_kb << " KB\n";
    std::cout << "[PASS] Processes Cnt : " << snap.processes.size() << "\n";
    std::cout << "All unit assertions passed successfully!\n";
    return 0;
}
