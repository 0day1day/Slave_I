#pragma once

#include "services/radio/ble_scanner.hpp"
#include "services/radio/radio_coordinator.hpp"
#include "services/radio/wifi_scanner.hpp"

namespace spectra5::platform {

class Tab5RadioCoordinator final : public services::IRadioCoordinator {
public:
    void bind(services::IWifiScanner* wifi, services::IBleScanner* ble);

    bool acquire_for_wifi() override;
    bool acquire_for_ble() override;
    void prepare_wifi_route() override;
    void prepare_ble_route() override;
    void release_all() override;

private:
    static bool wait_idle(services::IWifiScanner* wifi, services::IBleScanner* ble, int timeout_ms);

    services::IWifiScanner* wifi_ = nullptr;
    services::IBleScanner* ble_   = nullptr;
};

}  // namespace spectra5::platform
