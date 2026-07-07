#pragma once

#include "domain/radio/offensive.hpp"
#include "services/radio/radio_engine.hpp"

namespace spectra5::platform {

// Real RadioEngine on the Tab5: sends RadioCommands to the ESP32-C6 over the
// esp_hosted priv side-channel (esp_hosted_send_priv_command -> the C6's
// spectra5_offensive_dispatch). Replaces MockRadioEngine on device.
//
// Until the C6 capability handshake (Phase A remaining) is wired, capabilities()
// returns what our current C6 firmware actually implements.
class HostedRadioEngine final : public services::IRadioEngine {
public:
    uint32_t capabilities() const override;
    bool send(const domain::RadioCommand& command) override;
    void set_event_handler(EventHandler handler) override;

private:
    EventHandler handler_;
};

}  // namespace spectra5::platform
