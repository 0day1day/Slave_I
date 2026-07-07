#pragma once

#include <functional>

#include "domain/radio/offensive.hpp"

namespace spectra5::services {

// RadioEngine: the single P4-side seam for talking to the C6 radio engine.
//
// Everything the UI wants the radio to do flows through here as a RadioCommand
// ("P4 tells the C6: do X"), and everything the C6 reports comes back as a
// RadioEvent through one handler. The concrete implementation owns the transport
// (the ESP_PRIV_IF side-channel over esp_hosted); a MockRadioEngine drives the
// same contract on desktop/sim so UI and tests run without hardware.
//
// Keeping this the ONLY place that speaks the protocol is the "simplification"
// the project needs: scattered radio calls collapse into one commander.
class IRadioEngine {
public:
    using EventHandler = std::function<void(const domain::RadioEvent&)>;

    virtual ~IRadioEngine() = default;

    // Capabilities advertised by the C6 in the boot handshake. The UI enables
    // features from this bitmask (see domain::RadioCapability). 0 until known.
    virtual uint32_t capabilities() const = 0;

    // Queue a command to the C6. Returns false if the engine has no transport
    // yet or the capability the command needs is absent.
    virtual bool send(const domain::RadioCommand& command) = 0;

    // Register the single sink for C6 -> P4 events. Replaces any previous one.
    virtual void set_event_handler(EventHandler handler) = 0;
};

// Process-wide accessor + injection, mirroring inject_wifi_scanner()/wifi_scanner().
IRadioEngine* radio_engine();
void inject_radio_engine(IRadioEngine* engine);
bool has_radio_engine();

}  // namespace spectra5::services
