#include "services/radio/mock_radio_engine.hpp"

#include "domain/radio/offensive_codec.hpp"

namespace spectra5::services {

using namespace spectra5::domain;

uint32_t MockRadioEngine::default_capabilities()
{
    return RadioCapability::WifiScan | RadioCapability::Monitor | RadioCapability::Inject |
           RadioCapability::Deauth | RadioCapability::BeaconSpam | RadioCapability::ProbeSpam |
           RadioCapability::StationScan | RadioCapability::BleScan | RadioCapability::BleSpam;
}

MockRadioEngine::MockRadioEngine(uint32_t capabilities) : capabilities_(capabilities)
{
    // A couple of plausible clients so ScanStations has something to report.
    fake_stations_ = {
        Station{{0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01}, {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}, -54, 120},
        Station{{0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02}, {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}, -71, 33},
    };
}

uint32_t MockRadioEngine::capabilities() const
{
    return capabilities_;
}

void MockRadioEngine::set_event_handler(EventHandler handler)
{
    handler_ = std::move(handler);
}

void MockRadioEngine::set_fake_stations(std::vector<Station> stations)
{
    fake_stations_ = std::move(stations);
}

void MockRadioEngine::emit(const RadioEvent& event)
{
    if (handler_) {
        handler_(event);
    }
}

bool MockRadioEngine::send(const RadioCommand& command)
{
    switch (command.opcode) {
        case CommandOpcode::GetCapabilities: {
            CapabilityReport report;
            report.fw_major     = 0;
            report.fw_minor     = 1;
            report.capabilities = capabilities_;
            emit(evt_capabilities(report));
            return true;
        }
        case CommandOpcode::ScanStations: {
            if (!has_capability(capabilities_, RadioCapability::StationScan)) {
                return false;  // a stock-firmware C6 would reject this
            }
            for (const auto& station : fake_stations_) {
                emit(evt_station_seen(station));
            }
            return true;
        }
        case CommandOpcode::StartDeauth:
            if (!has_capability(capabilities_, RadioCapability::Deauth)) {
                return false;
            }
            emit(evt_mode_changed(RadioMode::Inject));
            return true;
        case CommandOpcode::StartMonitor:
            if (!has_capability(capabilities_, RadioCapability::Monitor)) {
                return false;
            }
            emit(evt_mode_changed(RadioMode::Monitor));
            return true;
        case CommandOpcode::Stop:
            emit(evt_mode_changed(RadioMode::Idle));
            return true;
        default:
            // Nop / SetMode / SetChannel / StopMonitor / beacon / probe / InjectRaw:
            // accepted, no synthetic event in the mock yet.
            return true;
    }
}

}  // namespace spectra5::services
