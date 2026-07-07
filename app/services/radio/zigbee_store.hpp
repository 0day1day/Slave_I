#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace spectra5::services {

// 802.15.4 (Zigbee/Thread/Matter) energy scan: peak RSSI per channel 11..26.
class ZigbeeStore {
public:
    static ZigbeeStore& instance();

    void set(const std::array<int8_t, 16>& powers);
    std::array<int8_t, 16> snapshot() const;
    uint32_t revision() const;
    bool has_result() const;

private:
    ZigbeeStore() = default;
    mutable std::mutex mutex_;
    std::array<int8_t, 16> powers_{};
    uint32_t revision_ = 0;
};

}  // namespace spectra5::services
