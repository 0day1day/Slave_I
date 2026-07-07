#include "services/radio/station_store.hpp"

#include <algorithm>

namespace spectra5::services {

StationStore& StationStore::instance()
{
    static StationStore store;
    return store;
}

void StationStore::add(const domain::Station& station)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& existing : stations_) {
        if (existing.mac == station.mac) {
            existing.rssi = station.rssi;  // refresh, no new entry
            ++existing.packets;
            ++revision_;
            return;
        }
    }
    stations_.push_back(station);
    ++revision_;
}

void StationStore::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stations_.clear();
    ++revision_;
}

std::vector<domain::Station> StationStore::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto copy = stations_;
    // Strongest signal first -- the device nearest the Tab5 (e.g. yours) floats to
    // the top. on_station_deauth indexes this same order, so it stays consistent.
    std::sort(copy.begin(), copy.end(),
              [](const domain::Station& a, const domain::Station& b) { return a.rssi > b.rssi; });
    return copy;
}

uint32_t StationStore::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

}  // namespace spectra5::services
