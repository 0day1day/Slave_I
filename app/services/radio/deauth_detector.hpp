#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace spectra5::services {

// Defensive deauth/disassoc detector: the C6 reports every deauth frame it sees and
// this tallies them (total count + last transmitter) for a live "you're being
// deauthed" indicator.
class DeauthDetector {
public:
    static DeauthDetector& instance();

    void report(const std::string& src);
    uint32_t count() const;
    std::string last_src() const;
    uint32_t revision() const;
    void reset();

private:
    DeauthDetector() = default;
    mutable std::mutex mutex_;
    uint32_t count_    = 0;
    uint32_t revision_ = 0;
    std::string last_src_;
};

}  // namespace spectra5::services
