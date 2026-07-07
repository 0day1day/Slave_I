#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "domain/radio/offensive.hpp"

namespace spectra5::services {

// Stations (clients) discovered by the C6 monitor, deduped by MAC. Written from
// the esp_hosted RX task (the 0x57 hook) and read by the LVGL thread, so it is
// mutex-guarded. The UI polls revision() to rebuild its list only on change.
class StationStore {
public:
    static StationStore& instance();

    void add(const domain::Station& station);  // dedups by MAC, updates RSSI
    void clear();
    std::vector<domain::Station> snapshot() const;
    uint32_t revision() const;

private:
    StationStore() = default;

    mutable std::mutex mutex_;
    std::vector<domain::Station> stations_;
    uint32_t revision_ = 0;
};

}  // namespace spectra5::services
