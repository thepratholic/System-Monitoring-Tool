#include "system_monitor.hpp"
#include "tui_renderer.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <string>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false);
}

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -i, --interval <ms>     Refresh interval in milliseconds (default: 1000)\n"
              << "  -n, --top <count>       Number of top processes to show (default: 15)\n"
              << "  -s, --sort <mode>       Sort processes by: cpu, mem, pid (default: cpu)\n"
              << "  -c, --iterations <num>  Run for N iterations and exit (0 = infinite, default: 0)\n"
              << "  -h, --help              Show this help message\n";
}
} // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
#ifdef SIGTERM
    std::signal(SIGTERM, signal_handler);
#endif

    int interval_ms = 1000;
    std::size_t top_n = 15;
    int max_iterations = 0;
    sysmon::SystemMonitor::SortMode sort_mode = sysmon::SystemMonitor::SortMode::CPU_DESC;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_ms = std::max(200, std::stoi(argv[++i]));
        } else if ((arg == "-n" || arg == "--top") && i + 1 < argc) {
            top_n = static_cast<std::size_t>(std::max(1, std::stoi(argv[++i])));
        } else if ((arg == "-c" || arg == "--iterations") && i + 1 < argc) {
            max_iterations = std::max(0, std::stoi(argv[++i]));
        } else if ((arg == "-s" || arg == "--sort") && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "mem") sort_mode = sysmon::SystemMonitor::SortMode::MEM_DESC;
            else if (mode == "pid") sort_mode = sysmon::SystemMonitor::SortMode::PID_ASC;
            else sort_mode = sysmon::SystemMonitor::SortMode::CPU_DESC;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
    }

    sysmon::SystemMonitor monitor;
    monitor.set_sort_mode(sort_mode);
    sysmon::TuiRenderer renderer;

    // Warm-up sleep so initial delta tick calculation is accurate
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    int iter = 0;
    while (g_running.load()) {
        auto snapshot = monitor.capture_snapshot(top_n);
        renderer.render(snapshot, monitor.get_sort_mode(), interval_ms);

        if (max_iterations > 0 && ++iter >= max_iterations) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    std::cout << "\n\033[0mExiting sys_monitor cleanly.\n";
    return 0;
}
