#include "system_monitor.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <iphlpapi.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace sysmon {

SystemMonitor::SystemMonitor()
    : last_capture_time_(std::chrono::steady_clock::now()) {
    SystemSnapshot dummy;
    collect_cpu_metrics(dummy);
    collect_network_metrics(dummy, 1.0);
    collect_disk_metrics(dummy, 1.0);
}

SystemSnapshot SystemMonitor::capture_snapshot(std::size_t top_n_procs) {
    auto now = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration<double>(now - last_capture_time_).count();
    if (elapsed_sec <= 0.0001) elapsed_sec = 0.5;
    last_capture_time_ = now;

    SystemSnapshot snap{};
    collect_system_info(snap);
    collect_cpu_metrics(snap);
    collect_memory_metrics(snap);
    collect_network_metrics(snap, elapsed_sec);
    collect_disk_metrics(snap, elapsed_sec);

    auto all_procs = proc_parser_.collect_processes(snap.memory.total_kb, elapsed_sec);
    switch (sort_mode_) {
        case SortMode::CPU_DESC:
            std::sort(all_procs.begin(), all_procs.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
                if (a.cpu_percent != b.cpu_percent) return a.cpu_percent > b.cpu_percent;
                return a.rss_kb > b.rss_kb;
            });
            break;
        case SortMode::MEM_DESC:
            std::sort(all_procs.begin(), all_procs.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
                return a.rss_kb > b.rss_kb;
            });
            break;
        case SortMode::PID_ASC:
            std::sort(all_procs.begin(), all_procs.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
                return a.pid < b.pid;
            });
            break;
    }

    if (all_procs.size() > top_n_procs) {
        all_procs.resize(top_n_procs);
    }
    snap.processes = std::move(all_procs);
    return snap;
}

void SystemMonitor::collect_system_info(SystemSnapshot& snap) {
#ifdef _WIN32
    char buf[256];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) snap.hostname = buf;
    else snap.hostname = "Windows-Host";
    snap.os_version = "Windows NT (Win32 API)";
    snap.uptime_seconds = GetTickCount64() / 1000;
    snap.load_avg_1m = 0.0;
    snap.load_avg_5m = 0.0;
    snap.load_avg_15m = 0.0;
#else
    struct utsname uts{};
    if (uname(&uts) == 0) {
        snap.hostname = uts.nodename;
        snap.os_version = std::string(uts.sysname) + " " + uts.release;
    } else {
        snap.hostname = "linux-host";
        snap.os_version = "Linux";
    }

    std::ifstream up_file("/proc/uptime");
    double up = 0.0;
    if (up_file >> up) {
        snap.uptime_seconds = static_cast<uint64_t>(up);
    }

    std::ifstream load_file("/proc/loadavg");
    if (load_file) {
        load_file >> snap.load_avg_1m >> snap.load_avg_5m >> snap.load_avg_15m;
    }
#endif
}

void SystemMonitor::collect_cpu_metrics(SystemSnapshot& snap) {
#ifdef _WIN32
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        auto to_u64 = [](const FILETIME& ft) {
            return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        uint64_t idle = to_u64(idleTime);
        uint64_t kernel = to_u64(kernelTime);
        uint64_t user = to_u64(userTime);
        uint64_t total = kernel + user;
        uint64_t active = (total >= idle) ? (total - idle) : 0;

        uint64_t d_active = (active >= prev_total_cpu_.active) ? (active - prev_total_cpu_.active) : 0;
        uint64_t d_total = (total >= prev_total_cpu_.total) ? (total - prev_total_cpu_.total) : 0;
        snap.total_cpu_percent = (d_total > 0) ? (static_cast<double>(d_active) * 100.0 / d_total) : 0.0;
        prev_total_cpu_ = {active, total};

        unsigned int n_cores = std::thread::hardware_concurrency();
        if (n_cores == 0) n_cores = 4;
        for (unsigned int i = 0; i < n_cores; ++i) {
            snap.cores.push_back({static_cast<int>(i), snap.total_cpu_percent, user, kernel, idle, total});
        }
    }
#else
    std::ifstream stat_file("/proc/stat");
    std::string line;
    std::vector<RawCpuTicks> current_cores;

    while (std::getline(stat_file, line)) {
        if (line.rfind("cpu", 0) != 0) break;
        std::istringstream iss(line);
        std::string label;
        uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
        iss >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        uint64_t active = user + nice + system + irq + softirq + steal;
        uint64_t total = active + idle + iowait;

        if (label == "cpu") {
            uint64_t d_active = (active >= prev_total_cpu_.active) ? (active - prev_total_cpu_.active) : 0;
            uint64_t d_total = (total >= prev_total_cpu_.total) ? (total - prev_total_cpu_.total) : 0;
            snap.total_cpu_percent = (d_total > 0) ? (static_cast<double>(d_active) * 100.0 / d_total) : 0.0;
            prev_total_cpu_ = {active, total};
        } else {
            int core_id = std::stoi(label.substr(3));
            std::size_t idx = current_cores.size();
            double usage = 0.0;
            if (idx < prev_core_cpu_.size()) {
                uint64_t d_act = (active >= prev_core_cpu_[idx].active) ? (active - prev_core_cpu_[idx].active) : 0;
                uint64_t d_tot = (total >= prev_core_cpu_[idx].total) ? (total - prev_core_cpu_[idx].total) : 0;
                usage = (d_tot > 0) ? (static_cast<double>(d_act) * 100.0 / d_tot) : 0.0;
            }
            current_cores.push_back({active, total});
            snap.cores.push_back({core_id, usage, user, system, idle, total});
        }
    }
    prev_core_cpu_ = std::move(current_cores);
#endif
}

void SystemMonitor::collect_memory_metrics(SystemSnapshot& snap) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        snap.memory.total_kb = memInfo.ullTotalPhys / 1024;
        snap.memory.available_kb = memInfo.ullAvailPhys / 1024;
        snap.memory.used_kb = snap.memory.total_kb - snap.memory.available_kb;
        snap.memory.swap_total_kb = memInfo.ullTotalPageFile / 1024;
        uint64_t swap_avail = memInfo.ullAvailPageFile / 1024;
        snap.memory.swap_used_kb = (snap.memory.swap_total_kb >= swap_avail) ? (snap.memory.swap_total_kb - swap_avail) : 0;
    }
#else
    std::ifstream mem_file("/proc/meminfo");
    std::string key, unit;
    uint64_t val = 0;
    uint64_t mem_free = 0, buffers = 0, cached = 0, swap_free = 0;
    bool has_avail = false;

    while (mem_file >> key >> val) {
        std::getline(mem_file, unit);
        if (key == "MemTotal:") snap.memory.total_kb = val;
        else if (key == "MemAvailable:") { snap.memory.available_kb = val; has_avail = true; }
        else if (key == "MemFree:") mem_free = val;
        else if (key == "Buffers:") buffers = val;
        else if (key == "Cached:") cached = val;
        else if (key == "SwapTotal:") snap.memory.swap_total_kb = val;
        else if (key == "SwapFree:") swap_free = val;
    }

    if (!has_avail) {
        snap.memory.available_kb = mem_free + buffers + cached;
    }
    snap.memory.used_kb = (snap.memory.total_kb >= snap.memory.available_kb)
                              ? (snap.memory.total_kb - snap.memory.available_kb)
                              : 0;
    snap.memory.swap_used_kb = (snap.memory.swap_total_kb >= swap_free)
                                   ? (snap.memory.swap_total_kb - swap_free)
                                   : 0;
#endif

    snap.memory.ram_usage_percent = (snap.memory.total_kb > 0)
        ? (static_cast<double>(snap.memory.used_kb) * 100.0 / snap.memory.total_kb)
        : 0.0;
    snap.memory.swap_usage_percent = (snap.memory.swap_total_kb > 0)
        ? (static_cast<double>(snap.memory.swap_used_kb) * 100.0 / snap.memory.swap_total_kb)
        : 0.0;
}

void SystemMonitor::collect_network_metrics(SystemSnapshot& snap, double elapsed_sec) {
#ifdef _WIN32
    PMIB_IFTABLE pIfTable = nullptr;
    ULONG dwSize = 0;
    if (GetIfTable(nullptr, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
        pIfTable = reinterpret_cast<PMIB_IFTABLE>( new char[dwSize] );
        if (GetIfTable(pIfTable, &dwSize, FALSE) == NO_ERROR) {
            for (DWORD i = 0; i < pIfTable->dwNumEntries; ++i) {
                const auto& row = pIfTable->table[i];
                if (row.dwType == MIB_IF_TYPE_LOOPBACK || (row.dwInOctets == 0 && row.dwOutOctets == 0)) continue;
                std::string name = "eth" + std::to_string(i);
                uint64_t rx = row.dwInOctets;
                uint64_t tx = row.dwOutOctets;
                double rx_rate = 0.0, tx_rate = 0.0;
                auto it = prev_net_.find(name);
                if (it != prev_net_.end()) {
                    if (rx >= it->second.rx) rx_rate = static_cast<double>(rx - it->second.rx) / 1024.0 / elapsed_sec;
                    if (tx >= it->second.tx) tx_rate = static_cast<double>(tx - it->second.tx) / 1024.0 / elapsed_sec;
                }
                prev_net_[name] = {rx, tx};
                snap.networks.push_back({name, rx, tx, rx_rate, tx_rate});
            }
        }
        delete[] reinterpret_cast<char*>(pIfTable);
    }
#else
    std::ifstream net_file("/proc/net/dev");
    std::string line;
    std::getline(net_file, line); // header 1
    std::getline(net_file, line); // header 2

    while (std::getline(net_file, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string iface = line.substr(0, colon);
        iface.erase(0, iface.find_first_not_of(" \t"));
        if (iface == "lo") continue;

        std::istringstream iss(line.substr(colon + 1));
        uint64_t rx_bytes = 0, tx_bytes = 0, dummy = 0;
        iss >> rx_bytes;
        for (int i = 0; i < 7; ++i) iss >> dummy;
        iss >> tx_bytes;

        double rx_rate = 0.0, tx_rate = 0.0;
        auto it = prev_net_.find(iface);
        if (it != prev_net_.end()) {
            if (rx_bytes >= it->second.rx)
                rx_rate = static_cast<double>(rx_bytes - it->second.rx) / 1024.0 / elapsed_sec;
            if (tx_bytes >= it->second.tx)
                tx_rate = static_cast<double>(tx_bytes - it->second.tx) / 1024.0 / elapsed_sec;
        }
        prev_net_[iface] = {rx_bytes, tx_bytes};
        snap.networks.push_back({iface, rx_bytes, tx_bytes, rx_rate, tx_rate});
    }
#endif
}

void SystemMonitor::collect_disk_metrics(SystemSnapshot& snap, double elapsed_sec) {
#ifndef _WIN32
    std::ifstream disk_file("/proc/diskstats");
    std::string line;
    while (std::getline(disk_file, line)) {
        std::istringstream iss(line);
        int major = 0, minor = 0;
        std::string dev;
        uint64_t reads_completed = 0, reads_merged = 0, sectors_read = 0, time_reading = 0;
        uint64_t writes_completed = 0, writes_merged = 0, sectors_written = 0;

        if (!(iss >> major >> minor >> dev >> reads_completed >> reads_merged >> sectors_read >> time_reading
                  >> writes_completed >> writes_merged >> sectors_written)) {
            continue;
        }

        // Filter out loop and ram devices
        if (dev.rfind("loop", 0) == 0 || dev.rfind("ram", 0) == 0) continue;

        uint64_t read_bytes = sectors_read * 512;
        uint64_t write_bytes = sectors_written * 512;

        double r_rate = 0.0, w_rate = 0.0;
        auto it = prev_disk_.find(dev);
        if (it != prev_disk_.end()) {
            if (read_bytes >= it->second.read)
                r_rate = static_cast<double>(read_bytes - it->second.read) / 1024.0 / elapsed_sec;
            if (write_bytes >= it->second.write)
                w_rate = static_cast<double>(write_bytes - it->second.write) / 1024.0 / elapsed_sec;
        }
        prev_disk_[dev] = {read_bytes, write_bytes};
        snap.disks.push_back({dev, read_bytes, write_bytes, r_rate, w_rate});
    }
#else
    (void)snap;
    (void)elapsed_sec;
#endif
}

} // namespace sysmon
