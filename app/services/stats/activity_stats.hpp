#pragma once

#include <atomic>
#include <cstdint>

namespace spectra5::services {

// Cumulative "what the tool did this session" counters. Header-only singleton so
// any layer (UI, radio hooks) can bump it without extra linkage. Shown on the
// dashboard as activity KPIs.
class ActivityStats {
public:
    static ActivityStats& instance()
    {
        static ActivityStats s;
        return s;
    }

    std::atomic<std::uint32_t> deauth_frames{0};    // deauth frames sent
    std::atomic<std::uint32_t> beacon_frames{0};    // beacon-spam frames sent
    std::atomic<std::uint32_t> ble_spam_sessions{0};// BLE spam runs launched
    std::atomic<std::uint32_t> zigbee_scans{0};     // 802.15.4 energy scans run
    std::atomic<std::uint32_t> handshakes{0};       // WPA handshakes / PMKIDs captured
    std::atomic<std::uint32_t> creds{0};            // portal / evil-twin creds captured
    std::atomic<std::uint32_t> evil_portals{0};     // evil portal / twin runs launched

    void bump(std::atomic<std::uint32_t>& c, std::uint32_t n = 1) { c.fetch_add(n); }
};

}  // namespace spectra5::services
