#pragma once

namespace spectra5::services {

// Serialises exclusive use of the Tab5 ESP32-C6 radio between Wi-Fi and BLE.
// Desktop builds leave this uninjected (nullptr).
class IRadioCoordinator {
public:
    virtual ~IRadioCoordinator() = default;

    virtual bool acquire_for_wifi() = 0;
    virtual bool acquire_for_ble()  = 0;

    // Call before navigating to a radio screen (stops the other mode on C6).
    virtual void prepare_wifi_route() = 0;
    virtual void prepare_ble_route()  = 0;
    virtual void release_all()        = 0;
};

IRadioCoordinator* radio_coordinator();
void inject_radio_coordinator(IRadioCoordinator* coordinator);

}  // namespace spectra5::services
