#pragma once

#include <vector>

#include "domain/radio/wifi.hpp"

namespace spectra5::services {

// Abstract Wi-Fi scanner (PRD Phase 3 interfaces). The desktop and the current
// Tab5 build use a deterministic mock; Phase 6 swaps in the real C6-backed
// scanner without touching the UI.
class IWifiScanner {
public:
    virtual ~IWifiScanner() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;
    virtual bool is_scanning() const = 0;

    // Tab5: tear down the C6 Wi-Fi stack so BLE can use the radio. Default stops scan only.
    virtual void release_radio() { stop(); }
    virtual bool is_radio_idle() const { return !is_scanning(); }

    // Current view of observed access points. Implementations may evolve signal
    // strength between calls while scanning.
    virtual std::vector<domain::WifiAccessPoint> snapshot() = 0;
};

// Injectable singleton (non-owning).
IWifiScanner* wifi_scanner();
void inject_wifi_scanner(IWifiScanner* scanner);
bool has_wifi_scanner();

}  // namespace spectra5::services
