#pragma once
#include "metrics.hpp"
#include "system_monitor.hpp"
#include <string>

namespace sysmon {

class TuiRenderer {
public:
    TuiRenderer() = default;
    void render(const SystemSnapshot& snap, SystemMonitor::SortMode sort_mode, int refresh_ms) const;

private:
    std::string make_progress_bar(double percentage, int width) const;
    std::string format_bytes_kb(uint64_t kb) const;
    std::string format_uptime(uint64_t seconds) const;
    std::string color_for_percent(double pct) const;
};

} // namespace sysmon
