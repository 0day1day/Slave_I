#pragma once

#include <vector>

#include "domain/radio/ble.hpp"

namespace spectra5::services {

class IBleScanner {
public:
    virtual ~IBleScanner() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;
    virtual bool is_scanning() const = 0;

    // Tab5: release the C6 BLE stack so Wi-Fi can use the radio.
    virtual void release_radio() { stop(); }
    virtual bool is_radio_idle() const { return !is_scanning(); }

    virtual std::vector<domain::BleAdvertisement> snapshot() = 0;
};

IBleScanner* ble_scanner();
void inject_ble_scanner(IBleScanner* scanner);
bool has_ble_scanner();

}  // namespace spectra5::services
