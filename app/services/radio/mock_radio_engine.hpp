#pragma once

#include <cstdint>
#include <vector>

#include "domain/radio/offensive.hpp"
#include "services/radio/radio_engine.hpp"

namespace spectra5::services {

// Desktop/sim implementation of IRadioEngine: it plays the part of the C6 so the
// UI and tests exercise the real command/event contract without hardware. send()
// reacts synchronously by emitting the events a real C6 would (capabilities on
// GetCapabilities, a few StationSeen on ScanStations, ModeChanged on Stop, etc.).
//
// The advertised capability set is configurable so we can test how the UI lights
// up against different (real / partial / stock) C6 firmware.
class MockRadioEngine final : public IRadioEngine {
public:
    // Defaults to the full offensive set the ported C6 engine will provide.
    explicit MockRadioEngine(uint32_t capabilities = default_capabilities());

    uint32_t capabilities() const override;
    bool send(const domain::RadioCommand& command) override;
    void set_event_handler(EventHandler handler) override;

    // Seed of synthetic stations returned for ScanStations (lets tests assert).
    void set_fake_stations(std::vector<domain::Station> stations);

    static uint32_t default_capabilities();

private:
    void emit(const domain::RadioEvent& event);

    uint32_t capabilities_;
    EventHandler handler_;
    std::vector<domain::Station> fake_stations_;
};

}  // namespace spectra5::services
