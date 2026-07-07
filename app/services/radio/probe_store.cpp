#include "services/radio/probe_store.hpp"

#include <algorithm>

namespace spectra5::services {

ProbeStore& ProbeStore::instance()
{
    static ProbeStore store;
    return store;
}

void ProbeStore::add(const std::string& ssid)
{
    if (ssid.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(ssids_.begin(), ssids_.end(), ssid) != ssids_.end()) {
        return;  // already harvested
    }
    if (ssids_.size() >= 64) {
        return;  // cap the list
    }
    ssids_.push_back(ssid);
    ++revision_;
}

std::vector<std::string> ProbeStore::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ssids_;
}

std::size_t ProbeStore::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ssids_.size();
}

uint32_t ProbeStore::revision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

void ProbeStore::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ssids_.clear();
    ++revision_;
}

}  // namespace spectra5::services
