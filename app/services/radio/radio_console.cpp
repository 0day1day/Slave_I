#include "services/radio/radio_console.hpp"

namespace spectra5::services {

RadioConsole& RadioConsole::instance()
{
    static RadioConsole console;
    return console;
}

void RadioConsole::log(const std::string& line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(line);
    while (lines_.size() > kMaxLines) {
        lines_.pop_front();
    }
    ++revision_;
}

std::string RadioConsole::text() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string out;
    // Newest first so the latest activity is at the top (no scrolling needed).
    for (auto it = lines_.rbegin(); it != lines_.rend(); ++it) {
        out += *it;
        out += '\n';
    }
    return out;
}

uint32_t RadioConsole::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

}  // namespace spectra5::services
