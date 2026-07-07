#include <cassert>
#include <optional>
#include <vector>

#include "domain/radio/offensive.hpp"
#include "domain/radio/offensive_codec.hpp"
#include "domain/radio/rpc_codec.hpp"

using namespace spectra5::domain;

int main()
{
    // --- Capability bitmask helpers ---
    const uint32_t caps = RadioCapability::WifiScan | RadioCapability::Monitor |
                          RadioCapability::Inject | RadioCapability::Deauth |
                          RadioCapability::StationScan;
    assert(has_capability(caps, RadioCapability::Monitor));
    assert(has_capability(caps, RadioCapability::StationScan));
    assert(!has_capability(caps, RadioCapability::Ieee802154));
    assert(!has_capability(caps, RadioCapability::BleSpam));

    // --- CapabilityReport round-trip ---
    CapabilityReport report;
    report.fw_major     = 1;
    report.fw_minor     = 4;
    report.capabilities = caps;
    const auto report_bytes = encode_capability_report(report);
    const auto report_back   = decode_capability_report(report_bytes);
    assert(report_back.has_value());
    assert(report_back->fw_major == 1);
    assert(report_back->fw_minor == 4);
    assert(report_back->capabilities == caps);

    // --- DeauthParams round-trip ---
    DeauthParams deauth;
    deauth.bssid   = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    deauth.target  = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    deauth.channel = 6;
    deauth.reason  = 7;
    deauth.bursts  = 64;
    const auto deauth_bytes = encode_deauth_params(deauth);
    const auto deauth_back   = decode_deauth_params(deauth_bytes);
    assert(deauth_back.has_value());
    assert(deauth_back->bssid == deauth.bssid);
    assert(deauth_back->target == deauth.target);
    assert(deauth_back->channel == 6);
    assert(deauth_back->reason == 7);
    assert(deauth_back->bursts == 64);

    // --- Station round-trip, including negative RSSI ---
    Station station;
    station.mac     = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    station.bssid   = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    station.rssi    = -72;
    station.packets = 1234;
    const auto station_bytes = encode_station(station);
    const auto station_back   = decode_station(station_bytes);
    assert(station_back.has_value());
    assert(station_back->mac == station.mac);
    assert(station_back->bssid == station.bssid);
    assert(station_back->rssi == -72);
    assert(station_back->packets == 1234);

    // --- MonitorParams round-trip ---
    MonitorParams mon;
    mon.channel     = 0;  // hop
    mon.filter_mask = static_cast<uint32_t>(EventKind::Handshake);
    const auto mon_back = decode_monitor_params(encode_monitor_params(mon));
    assert(mon_back.has_value());
    assert(mon_back->channel == 0);
    assert(mon_back->filter_mask == static_cast<uint32_t>(EventKind::Handshake));

    // --- Truncated buffers must fail, not read past the end ---
    auto truncated = deauth_bytes;
    truncated.pop_back();
    assert(!decode_deauth_params(truncated).has_value());
    assert(!decode_capability_report({0x01, 0x02}).has_value());  // missing the u32

    // --- Command envelope wrapped in an RpcFrame, through the real codec ---
    RadioCommand cmd;
    cmd.opcode = CommandOpcode::StartDeauth;
    cmd.params = deauth_bytes;
    const RpcFrame cmd_frame = make_command_frame(7, cmd);
    assert(cmd_frame.header.message_type == static_cast<uint8_t>(RpcMessageType::Command));

    const std::vector<uint8_t> wire = rpc_encode(cmd_frame);
    const auto wire_back = rpc_decode_frame(wire.data(), wire.size());
    assert(wire_back.has_value());
    const auto cmd_back = command_from_frame(*wire_back);
    assert(cmd_back.has_value());
    assert(cmd_back->opcode == CommandOpcode::StartDeauth);
    const auto deauth_in_cmd = decode_deauth_params(cmd_back->params);
    assert(deauth_in_cmd.has_value());
    assert(deauth_in_cmd->channel == 6);

    // --- Event envelope: C6 -> P4 (StationSeen carrying a Station) ---
    RadioEvent ev;
    ev.kind = EventKind::StationSeen;
    ev.data = station_bytes;
    const RpcFrame ev_frame = make_event_frame(9, ev);
    const auto ev_wire = rpc_encode(ev_frame);
    const auto ev_decoded = rpc_decode_frame(ev_wire.data(), ev_wire.size());
    assert(ev_decoded.has_value());
    const auto ev_back = event_from_frame(*ev_decoded);
    assert(ev_back.has_value());
    assert(ev_back->kind == EventKind::StationSeen);
    const auto station_in_ev = decode_station(ev_back->data);
    assert(station_in_ev.has_value());
    assert(station_in_ev->rssi == -72);

    // --- A command frame must not decode as an event and vice-versa ---
    assert(!event_from_frame(cmd_frame).has_value());
    assert(!command_from_frame(ev_frame).has_value());

    return 0;
}
