#include <cassert>
#include <vector>

#include "domain/radio/offensive.hpp"
#include "domain/radio/offensive_codec.hpp"
#include "services/radio/mock_radio_engine.hpp"
#include "services/radio/radio_engine.hpp"

using namespace spectra5::domain;
using namespace spectra5::services;

int main()
{
    // Collect every event the engine emits back to the P4.
    std::vector<RadioEvent> events;

    MockRadioEngine engine;  // full capability set
    engine.set_event_handler([&](const RadioEvent& e) { events.push_back(e); });

    // --- Injection + accessor wiring works like the other services ---
    assert(!has_radio_engine());
    inject_radio_engine(&engine);
    assert(has_radio_engine());
    assert(radio_engine() == &engine);

    // --- Boot handshake: GetCapabilities -> Capabilities event with the mask ---
    assert(radio_engine()->send(cmd_get_capabilities()));
    assert(events.size() == 1);
    assert(events[0].kind == EventKind::Capabilities);
    const auto report = decode_capability_report(events[0].data);
    assert(report.has_value());
    assert(has_capability(report->capabilities, RadioCapability::Deauth));
    assert(has_capability(report->capabilities, RadioCapability::Monitor));
    assert(report->capabilities == engine.capabilities());

    // --- "P4 tells C6: scan clients of this AP" -> StationSeen per client ---
    events.clear();
    const MacAddr bssid = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    assert(radio_engine()->send(cmd_scan_stations(bssid, 6)));
    assert(events.size() == 2);
    for (const auto& e : events) {
        assert(e.kind == EventKind::StationSeen);
        const auto st = decode_station(e.data);
        assert(st.has_value());
        assert(st->bssid == bssid);
    }

    // --- Deauth -> ModeChanged(Inject) ---
    events.clear();
    DeauthParams d;
    d.bssid   = bssid;
    d.channel = 6;
    assert(radio_engine()->send(cmd_deauth(d)));
    assert(events.size() == 1 && events[0].kind == EventKind::ModeChanged);
    assert(events[0].data.size() == 1 &&
           events[0].data[0] == static_cast<uint8_t>(RadioMode::Inject));

    // --- Stop -> ModeChanged(Idle) ---
    events.clear();
    assert(radio_engine()->send(cmd_stop()));
    assert(events.size() == 1 &&
           events[0].data[0] == static_cast<uint8_t>(RadioMode::Idle));

    // --- A stock/partial C6 (no offensive caps) must REJECT offensive commands,
    //     so the UI can grey them out instead of pretending they ran. ---
    MockRadioEngine stock(RadioCapability::WifiScan | RadioCapability::BleScan);
    std::vector<RadioEvent> stock_events;
    stock.set_event_handler([&](const RadioEvent& e) { stock_events.push_back(e); });
    assert(!stock.send(cmd_deauth(d)));            // no Deauth cap -> rejected
    assert(!stock.send(cmd_scan_stations(bssid, 6)));  // no StationScan cap -> rejected
    assert(stock_events.empty());

    inject_radio_engine(nullptr);
    assert(!has_radio_engine());
    return 0;
}
