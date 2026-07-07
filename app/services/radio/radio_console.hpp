#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace spectra5::services {

// In-memory ring buffer of recent radio-engine activity, shown as a live console
// in the UI. Written from multiple tasks (the LVGL thread on send, the esp_hosted
// RX task on a C6 result), so every access is mutex-guarded. The UI polls
// revision() to know when to refresh.
class RadioConsole {
public:
    static RadioConsole& instance();

    // Append a line (newest last). Trims to the most recent kMaxLines.
    void log(const std::string& line);

    // Newest-first text block (for a top-anchored console label), and a revision
    // that bumps on every log() so the UI only repaints when something changed.
    std::string text() const;
    uint32_t revision() const;

    static constexpr std::size_t kMaxLines = 40;

private:
    RadioConsole() = default;

    mutable std::mutex mutex_;
    std::deque<std::string> lines_;
    uint32_t revision_ = 0;
};

}  // namespace spectra5::services
