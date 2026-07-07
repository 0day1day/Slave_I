#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace spectra5::services {

// Deduped list of SSIDs harvested from probe requests (clients' preferred networks),
// fed by the C6 Karma capture. The Karma responder beacons these back.
class ProbeStore {
public:
    static ProbeStore& instance();

    void add(const std::string& ssid);
    std::vector<std::string> snapshot() const;
    std::size_t size() const;
    uint32_t revision() const;
    void clear();

private:
    ProbeStore() = default;
    mutable std::mutex mutex_;
    std::vector<std::string> ssids_;
    uint32_t revision_ = 0;
};

}  // namespace spectra5::services
