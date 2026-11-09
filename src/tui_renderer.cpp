#include "tui_renderer.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace sysmon {

namespace ansi {
    constexpr const char* CLEAR_HOME = "\033[2J\033[H";
    constexpr const char* RESET      = "\033[0m";
    constexpr const char* BOLD       = "\033[1m";
    constexpr const char* DIM        = "\033[2m";
    constexpr const char* CYAN       = "\033[36m";
    constexpr const char* GREEN      = "\033[32m";
    constexpr const char* YELLOW     = "\033[33m";
    constexpr const char* RED        = "\033[31m";
    constexpr const char* MAGENTA    = "\033[35m";
    constexpr const char* HEADER_BG  = "\033[44;37;1m";
}

std::string TuiRenderer::color_for_percent(double pct) const {
    if (pct >= 80.0) return ansi::RED;
    if (pct >= 50.0) return ansi::YELLOW;
    return ansi::GREEN;
}

std::string TuiRenderer::make_progress_bar(double percentage, int width) const {
    double clamped = std::clamp(percentage, 0.0, 100.0);
    int filled = static_cast<int>((clamped / 100.0) * width);
    std::ostringstream oss;
    oss << "[" << color_for_percent(clamped);
    for (int i = 0; i < width; ++i) {
        oss << (i < filled ? '|' : ' ');
    }
    oss << ansi::RESET << "]";
    return oss.str();
}

std::string TuiRenderer::format_bytes_kb(uint64_t kb) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    if (kb >= 1048576) {
        oss << (static_cast<double>(kb) / 1048576.0) << " GB";
    } else if (kb >= 1024) {
        oss << (static_cast<double>(kb) / 1024.0) << " MB";
    } else {
        oss << kb << " KB";
    }
    return oss.str();
}

std::string TuiRenderer::format_uptime(uint64_t seconds) const {
    uint64_t days = seconds / 86400;
    uint64_t hours = (seconds % 86400) / 3600;
    uint64_t mins = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;
    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    oss << std::setw(2) << std::setfill('0') << hours << ":"
        << std::setw(2) << std::setfill('0') << mins << ":"
        << std::setw(2) << std::setfill('0') << secs;
    return oss.str();
}

void TuiRenderer::render(const SystemSnapshot& snap, SystemMonitor::SortMode sort_mode, int refresh_ms) const {
    std::ostringstream out;
    out << ansi::CLEAR_HOME;

    // Top Banner
    out << ansi::BOLD << ansi::CYAN
        << "====================================================================================\n"
        << "  SYS_MONITOR C++20 | Host: " << snap.hostname
        << " | OS: " << snap.os_version
        << " | Uptime: " << format_uptime(snap.uptime_seconds) << "\n"
        << "===================================================================================="
        << ansi::RESET << "\n";

    // CPU Total & Load Avg
    out << ansi::BOLD << "CPU Total : " << ansi::RESET
        << make_progress_bar(snap.total_cpu_percent, 30) << " "
        << std::fixed << std::setprecision(1) << std::setw(5) << snap.total_cpu_percent << "%";

    if (snap.load_avg_1m > 0.0 || snap.load_avg_5m > 0.0) {
        out << "   | Load Avg: " << std::setprecision(2)
            << snap.load_avg_1m << ", " << snap.load_avg_5m << ", " << snap.load_avg_15m;
    }
    out << "\n";

    // Per-Core Grid (up to 8 cores displayed cleanly in 2 columns)
    std::size_t max_cores_shown = std::min<std::size_t>(snap.cores.size(), 8);
    for (std::size_t i = 0; i < max_cores_shown; ++i) {
        const auto& c = snap.cores[i];
        out << "  Core " << std::setw(2) << c.core_id << " : "
            << make_progress_bar(c.usage_percent, 15) << " "
            << std::fixed << std::setprecision(1) << std::setw(5) << c.usage_percent << "%";
        if (i % 2 == 1 || i + 1 == max_cores_shown) out << "\n";
        else out << "    ";
    }

    // Memory & Swap
    out << "\n" << ansi::BOLD << "Memory    : " << ansi::RESET
        << make_progress_bar(snap.memory.ram_usage_percent, 30) << " "
        << std::fixed << std::setprecision(1) << std::setw(5) << snap.memory.ram_usage_percent << "% ("
        << format_bytes_kb(snap.memory.used_kb) << " / " << format_bytes_kb(snap.memory.total_kb) << ")\n";

    out << ansi::BOLD << "Swap      : " << ansi::RESET
        << make_progress_bar(snap.memory.swap_usage_percent, 30) << " "
        << std::fixed << std::setprecision(1) << std::setw(5) << snap.memory.swap_usage_percent << "% ("
        << format_bytes_kb(snap.memory.swap_used_kb) << " / " << format_bytes_kb(snap.memory.swap_total_kb) << ")\n";

    // Network & Disk IO Summary
    out << "\n" << ansi::MAGENTA << ansi::BOLD << "[ Network & Disk I/O ]" << ansi::RESET << "\n";
    if (snap.networks.empty()) {
        out << "  Network : No active non-loopback interfaces detected\n";
    } else {
        for (std::size_t i = 0; i < std::min<std::size_t>(snap.networks.size(), 3); ++i) {
            const auto& net = snap.networks[i];
            out << "  Net [" << std::left << std::setw(8) << net.interface_name << std::right << "] "
                << "RX: " << std::setw(8) << std::fixed << std::setprecision(1) << net.rx_rate_kbps << " KB/s "
                << "(Total: " << format_bytes_kb(net.rx_bytes / 1024) << ") | "
                << "TX: " << std::setw(8) << net.tx_rate_kbps << " KB/s "
                << "(Total: " << format_bytes_kb(net.tx_bytes / 1024) << ")\n";
        }
    }

    if (!snap.disks.empty()) {
        for (std::size_t i = 0; i < std::min<std::size_t>(snap.disks.size(), 2); ++i) {
            const auto& d = snap.disks[i];
            out << "  Disk[" << std::left << std::setw(8) << d.device_name << std::right << "] "
                << "Read: " << std::setw(6) << std::fixed << std::setprecision(1) << d.read_rate_kbps << " KB/s | "
                << "Write: " << std::setw(6) << d.write_rate_kbps << " KB/s\n";
        }
    }

    // Process Table Header
    std::string sort_label = (sort_mode == SystemMonitor::SortMode::CPU_DESC) ? "CPU%" :
                             (sort_mode == SystemMonitor::SortMode::MEM_DESC) ? "MEM%" : "PID";

    out << "\n" << ansi::HEADER_BG
        << std::left << std::setw(8)  << " PID"
        << std::setw(26) << " PROCESS NAME"
        << std::setw(7)  << " STATE"
        << std::right << std::setw(9) << "CPU% "
        << std::setw(9)  << "MEM% "
        << std::setw(12) << "RSS "
        << std::setw(9)  << "THREADS "
        << ansi::RESET << "\n";

    for (const auto& p : snap.processes) {
        std::string pname = p.name;
        if (pname.size() > 24) pname = pname.substr(0, 21) + "...";

        out << " " << std::left << std::setw(7) << p.pid
            << std::setw(26) << pname
            << std::setw(7) << p.state
            << std::right
            << color_for_percent(p.cpu_percent) << std::setw(8) << std::fixed << std::setprecision(1) << p.cpu_percent << " " << ansi::RESET
            << std::setw(8) << std::fixed << std::setprecision(1) << p.mem_percent << " "
            << std::setw(11) << format_bytes_kb(p.rss_kb) << " "
            << std::setw(8) << p.threads << "\n";
    }

    out << ansi::DIM
        << "------------------------------------------------------------------------------------\n"
        << " Sort: [" << sort_label << "] | Refresh: " << refresh_ms << "ms | Press Ctrl+C to exit (or use --iterations N)\n"
        << ansi::RESET;

    std::cout << out.str() << std::flush;
}

} // namespace sysmon
