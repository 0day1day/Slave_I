#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace spectra5::services {

// Live sniffer per-type frame counts reported by the C6:
// [0] beacon, [1] probe, [2] data, [3] deauth/disassoc, [4] other.
class SnifferStore {
public:
    static SnifferStore& instance();

    void set(const std::array<uint32_t, 5>& counts);
    std::array<uint32_t, 5> snapshot() const;
    uint32_t total() const;
    uint32_t revision() const;
    void reset();

private:
    SnifferStore() = default;
    mutable std::mutex mutex_;
    std::array<uint32_t, 5> counts_{};
    uint32_t revision_ = 0;
};

}  // namespace spectra5::services
