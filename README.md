# sys_monitor (Modern C++20 System & Process Resource Monitor)

A lightweight, zero-dependency, real-time command-line system and process monitoring tool written in **Modern C++20**. Inspired by `htop` and `sys_monitor`, it parses low-level kernel telemetry directly from `/proc` on Linux (with native Win32 API fallback on Windows) and renders a flicker-free ANSI Terminal UI (TUI).

---

## Key Features

- **Real-Time CPU Telemetry:** Computes exact delta active/idle clock ticks across aggregate CPU and individual hardware cores, along with 1m/5m/15m system load averages.
- **Memory & Swap Analytics:** Tracks physical RAM (`MemTotal`, `MemAvailable`, `Buffers`, `Cached`) and Swap utilization with dynamic color-coded progress bars.
- **Network & Disk I/O Bandwidth:** Calculates live RX/TX throughput (`KB/s`) per network interface (`/proc/net/dev`) and block device read/write rates (`/proc/diskstats`).
- **Process Inspector & Sorter:** Parses `/proc/[pid]/stat` to compute per-process CPU%, Resident Set Size (`RSS`), memory share, thread count, and execution state (`R`, `S`, `D`, `Z`).
- **Cross-Platform Engine:** Native `/proc` filesystem parser on Linux and Win32 (`GetSystemTimes`, `GlobalMemoryStatusEx`, `Toolhelp32Snapshot`, `GetIfTable`) support on Windows.
- **Zero External Dependencies:** Pure C++20 STL + ANSI escape sequences—no `ncurses` installation required.

---

## Project Architecture

```text
sys_monitor_cpp/
├── CMakeLists.txt              # CMake build configuration (C++20)
├── Dockerfile                  # Containerized build & execution environment
├── include/
│   ├── metrics.hpp             # Core POD data structures for system telemetry
│   ├── process_parser.hpp      # Per-PID CPU & RSS delta parser interface
│   ├── system_monitor.hpp      # Stateful telemetry aggregator & sorter
│   └── tui_renderer.hpp        # Flicker-free ANSI terminal dashboard renderer
├── src/
│   ├── main.cpp                # CLI argument parser, signal handler & event loop
│   ├── process_parser.cpp      # Linux /proc/[pid]/stat & Win32 process collector
│   ├── system_monitor.cpp      # Kernel telemetry collectors (CPU, RAM, Net, Disk)
│   └── tui_renderer.cpp        # ANSI color progress bars & formatted table output
└── tests/
    └── test_metrics.cpp        # Automated CTest unit verification suite
```

---

## Getting Started

### Prerequisites
- **C++20 Compiler:** `g++ >= 10`, `clang++ >= 11`, or `MSVC 2019+`
- **Build System:** `CMake >= 3.16` (or direct `g++` compilation)

### Option 1: Build with CMake (Linux / macOS / Windows)
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Option 2: Direct One-Line Compilation with `g++` (No CMake needed)
**Linux:**
```bash
g++ -std=c++20 -O3 -Wall -Wextra -Iinclude src/*.cpp -o sys_monitor -pthread
```

**Windows (MinGW g++):**
```bash
g++ -std=c++20 -O3 -Wall -Wextra -Iinclude src/*.cpp -o sys_monitor.exe -lpdh -lpsapi -liphlpapi -lws2_32
```

---

## Usage & CLI Flags

```bash
# Run with default settings (1000ms refresh, top 15 processes sorted by CPU%)
./build/sys_monitor

# Refresh every 500ms, show top 20 processes, sort by Memory usage
./build/sys_monitor --interval 500 --top 20 --sort mem

# Run for 5 snapshots and exit automatically
./build/sys_monitor --iterations 5
```

| Flag | Description | Default |
| :--- | :--- | :--- |
| `-i, --interval <ms>` | Refresh interval in milliseconds (minimum `200ms`) | `1000` |
| `-n, --top <count>` | Number of top processes displayed in the table | `15` |
| `-s, --sort <mode>` | Sort processes by `cpu`, `mem`, or `pid` | `cpu` |
| `-c, --iterations <N>` | Run for `N` refresh cycles and exit (`0` = infinite) | `0` |
| `-h, --help` | Display CLI usage instructions | — |

---

## Running Unit Tests

```bash
cd build
ctest --output-on-failure
```

---

## License
MIT License
