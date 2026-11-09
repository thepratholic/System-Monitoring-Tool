#include "process_parser.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace sysmon {

std::vector<ProcessInfo> ProcessParser::collect_processes(uint64_t total_ram_kb, double elapsed_sec) {
    std::vector<ProcessInfo> processes;
    if (elapsed_sec <= 0.001) elapsed_sec = 1.0;

#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return processes;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe32)) {
        do {
            ProcessInfo p{};
            p.pid = static_cast<int>(pe32.th32ProcessID);
            #ifdef UNICODE
            std::wstring ws(pe32.szExeFile);
            p.name = std::string(ws.begin(), ws.end());
            #else
            p.name = pe32.szExeFile;
            #endif
            p.threads = pe32.cntThreads;
            p.state = "R";
            p.rss_kb = 0;
            p.cpu_percent = 0.0;

            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProc) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                    p.rss_kb = pmc.WorkingSetSize / 1024;
                }
                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    ULARGE_INTEGER k, u;
                    k.LowPart = ftKernel.dwLowDateTime;
                    k.HighPart = ftKernel.dwHighDateTime;
                    u.LowPart = ftUser.dwLowDateTime;
                    u.HighPart = ftUser.dwHighDateTime;
                    uint64_t total_100ns = k.QuadPart + u.QuadPart;
                    auto it = prev_proc_ticks_.find(p.pid);
                    if (it != prev_proc_ticks_.end() && total_100ns >= it->second) {
                        double delta_sec = static_cast<double>(total_100ns - it->second) / 1e7;
                        p.cpu_percent = (delta_sec / elapsed_sec) * 100.0;
                    }
                    prev_proc_ticks_[p.pid] = total_100ns;
                }
                CloseHandle(hProc);
            }
            p.mem_percent = (total_ram_kb > 0) ? (static_cast<double>(p.rss_kb) * 100.0 / total_ram_kb) : 0.0;
            processes.push_back(std::move(p));
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
#else
    long clk_tck = sysconf(_SC_CLK_TCK);
    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
    if (clk_tck <= 0) clk_tck = 100;
    if (page_size_kb <= 0) page_size_kb = 4;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        if (!entry.is_directory(ec)) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.empty() || !std::all_of(dirname.begin(), dirname.end(), ::isdigit)) continue;

        int pid = std::stoi(dirname);
        std::ifstream stat_file(entry.path() / "stat");
        if (!stat_file.is_open()) continue;

        std::string line;
        std::getline(stat_file, line);
        auto lparen = line.find('(');
        auto rparen = line.rfind(')');
        if (lparen == std::string::npos || rparen == std::string::npos || rparen <= lparen) continue;

        ProcessInfo p{};
        p.pid = pid;
        p.name = line.substr(lparen + 1, rparen - lparen - 1);

        std::istringstream iss(line.substr(rparen + 2));
        std::string state;
        uint64_t dummy_u64;
        int64_t dummy_i64;
        uint64_t utime = 0, stime = 0, threads = 1;
        int64_t rss_pages = 0;

        iss >> state; // 3: state
        for (int i = 4; i <= 13; ++i) iss >> dummy_u64;
        iss >> utime >> stime; // 14, 15
        iss >> dummy_i64 >> dummy_i64 >> dummy_i64 >> dummy_i64; // 16..19
        iss >> threads; // 20
        iss >> dummy_i64 >> dummy_u64 >> dummy_u64 >> rss_pages; // 21..24

        p.state = state;
        p.threads = threads;
        p.rss_kb = (rss_pages > 0) ? static_cast<uint64_t>(rss_pages) * page_size_kb : 0;
        p.mem_percent = (total_ram_kb > 0) ? (static_cast<double>(p.rss_kb) * 100.0 / total_ram_kb) : 0.0;

        uint64_t total_ticks = utime + stime;
        auto it = prev_proc_ticks_.find(pid);
        if (it != prev_proc_ticks_.end() && total_ticks >= it->second) {
            double delta_sec = static_cast<double>(total_ticks - it->second) / static_cast<double>(clk_tck);
            p.cpu_percent = (delta_sec / elapsed_sec) * 100.0;
        } else {
            p.cpu_percent = 0.0;
        }
        prev_proc_ticks_[pid] = total_ticks;

        processes.push_back(std::move(p));
    }
#endif

    return processes;
}

} // namespace sysmon
